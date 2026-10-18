#pragma once
// Воспроизведение файла станции: LOOP (по кругу с равномощностным кроссфейдом на стыке) или ONE-SHOT
// (один раз, затем тишина). park() - затухание ~12 мс и пауза в начале; restart() - с начала с появлением 10 мс.
// Без JUCE, без аллокаций.
#include "StationBuffer.h"
#include <algorithm>
#include <cmath>

namespace fmrx
{
    class LoopPlayer
    {
    public:
        void reset() noexcept { pos_ = 0; bufId_ = 0; fade_ = 0.f; target_ = 1.f; ended_ = false; }

        void restart() noexcept { pos_ = 0; fade_ = 0.f; target_ = 1.f; ended_ = false; }
        void park() noexcept { target_ = 0.f; }                     // после затухания позиция -> 0
        bool parked() const noexcept { return target_ <= 0.f && fade_ <= 0.f; }
        bool ended() const noexcept { return ended_; }

        // Пишет n сэмплов. Возвращает true, если в этом блоке one-shot дошёл до конца.
        bool render(const StationBuffer* buf, float* outL, float* outR, int n, bool loop) noexcept
        {
            if (buf == nullptr || buf->length <= 0)
            {
                std::fill(outL, outL + n, 0.f);
                std::fill(outR, outR + n, 0.f);
                bufId_ = 0;
                return false;
            }
            if (buf->id != bufId_)            // новый файл: с начала, короткое появление (без щелчка)
            {
                bufId_ = buf->id;
                pos_ = 0; fade_ = 0.f; ended_ = false;
            }
            const int len = buf->length;
            const int xf = std::min(len / 4, std::max(1, (int) (buf->sampleRate * 0.01)));   // 10 мс
            const float* l = buf->ch[0].data();
            const float* r = buf->numChannels > 1 ? buf->ch[1].data() : l;
            const float upStep = 1.f / (float) std::max(1, (int) (buf->sampleRate * 0.010));
            const float downStep = 1.f / (float) std::max(1, (int) (buf->sampleRate * 0.012));
            const int tailFade = std::max(1, (int) (buf->sampleRate * 0.005));
            bool endedNow = false;

            for (int i = 0; i < n; ++i)
            {
                if (ended_ || (target_ <= 0.f && fade_ <= 0.f)) { outL[i] = outR[i] = 0.f; continue; }

                float a = l[pos_], b = r[pos_];
                const int tailStart = len - xf;
                if (loop && pos_ >= tailStart && len > 2 * xf)
                {
                    // Конец цикла смешивается с началом: к моменту перехода начало уже отыграно.
                    const int h = pos_ - tailStart;
                    const float t = ((float) h + 0.5f) / (float) xf;
                    const float gOut = std::cos(t * 1.5707963f), gIn = std::sin(t * 1.5707963f);
                    a = a * gOut + l[h] * gIn;
                    b = b * gOut + r[h] * gIn;
                }
                else if (! loop && pos_ >= len - tailFade)          // one-shot: короткое затухание в самом конце
                {
                    const float g = (float) (len - pos_) / (float) tailFade;
                    a *= g; b *= g;
                }

                if (fade_ < target_) fade_ = std::min(target_, fade_ + upStep);
                else if (fade_ > target_) fade_ = std::max(target_, fade_ - downStep);
                outL[i] = a * fade_; outR[i] = b * fade_;

                if (++pos_ >= len)
                {
                    if (loop) pos_ = (len > 2 * xf) ? xf : 0;
                    else { ended_ = true; endedNow = true; pos_ = 0; }
                }
                if (target_ <= 0.f && fade_ <= 0.f) pos_ = 0;      // пауза: встали в начало
            }
            return endedNow;
        }

    private:
        int pos_ = 0;
        std::uint64_t bufId_ = 0;
        float fade_ = 1.f, target_ = 1.f;
        bool ended_ = false;
    };
}
