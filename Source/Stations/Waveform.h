#pragma once
// Волна для интерфейса и авто-обрезка тишины. Чистые функции без JUCE (вне аудиопотока).
#include "StationBuffer.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace fmrx
{
    // Огибающая файла: мин/макс по корзинам для каждого канала (для моно оба канала одинаковы).
    struct Peaks
    {
        int buckets = 0;
        std::array<std::vector<float>, 2> mn, mx;
        double durationSec = 0.0;
    };

    inline std::shared_ptr<const Peaks> computePeaks(const StationBuffer& b, int buckets = 4096)
    {
        auto p = std::make_shared<Peaks>();
        if (b.length <= 0 || buckets <= 0) return p;
        buckets = std::min(buckets, b.length);
        p->buckets = buckets;
        p->durationSec = b.sampleRate > 0 ? b.length / b.sampleRate : 0.0;
        for (int c = 0; c < 2; ++c)
        {
            const auto& src = b.ch[(size_t) (c < b.numChannels ? c : 0)];
            p->mn[(size_t) c].assign((size_t) buckets, 0.f);
            p->mx[(size_t) c].assign((size_t) buckets, 0.f);
            for (int k = 0; k < buckets; ++k)
            {
                const long long a = (long long) k * b.length / buckets;
                const long long e = std::max(a + 1, (long long) (k + 1) * b.length / buckets);
                float lo = 0.f, hi = 0.f;
                for (long long i = a; i < e; ++i) { lo = std::min(lo, src[(size_t) i]); hi = std::max(hi, src[(size_t) i]); }
                p->mn[(size_t) c][(size_t) k] = lo;
                p->mx[(size_t) c][(size_t) k] = hi;
            }
        }
        return p;
    }

    struct TrimResult { double startSec = 0.0, endSec = 0.0; bool found = false; };

    // Первый и последний отсчёт громче порога (по сумме каналов), с запасом marginSec.
    inline TrimResult findSilenceTrim(const StationBuffer& b, float thresholdDb, double marginSec = 0.01)
    {
        TrimResult r;
        if (b.length <= 0 || b.sampleRate <= 0) return r;
        const float thr = std::pow(10.f, thresholdDb / 20.f);
        auto loud = [&](int i)
        {
            float v = std::fabs(b.ch[0][(size_t) i]);
            if (b.numChannels > 1) v = std::max(v, std::fabs(b.ch[1][(size_t) i]));
            return v >= thr;
        };
        int first = -1, last = -1;
        for (int i = 0; i < b.length; ++i) if (loud(i)) { first = i; break; }
        for (int i = b.length - 1; i >= 0; --i) if (loud(i)) { last = i; break; }
        if (first < 0) return r;
        const int margin = (int) (marginSec * b.sampleRate);
        r.startSec = std::max(0, first - margin) / b.sampleRate;
        r.endSec = std::min(b.length, last + 1 + margin) / b.sampleRate;
        r.found = true;
        return r;
    }

    // Ближайший переход через ноль (в пределах ±radius сэмплов) - маркер без щелчка.
    inline int snapToZeroCrossing(const StationBuffer& b, int pos, int radius)
    {
        if (b.length < 2) return std::clamp(pos, 0, std::max(0, b.length));
        pos = std::clamp(pos, 0, b.length - 1);
        const auto& x = b.ch[0];
        for (int d = 0; d <= radius; ++d)
            for (int s : { pos - d, pos + d })
                if (s > 0 && s < b.length && ((x[(size_t) s - 1] <= 0.f && x[(size_t) s] >= 0.f) || (x[(size_t) s - 1] >= 0.f && x[(size_t) s] <= 0.f)))
                    return s;
        return pos;
    }
}
