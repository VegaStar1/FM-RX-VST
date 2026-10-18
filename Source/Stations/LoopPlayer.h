#pragma once
// Воспроизведение файла станции по кругу с равномощностным кроссфейдом на стыке. Без JUCE, без аллокаций.
#include "StationBuffer.h"
#include <algorithm>
#include <cmath>

namespace fmrx
{
    class LoopPlayer
    {
    public:
        void reset() noexcept { pos_ = 0; bufId_ = 0; fadeIn_ = 0.f; }

        // Пишет n сэмплов в outL/outR. buf == nullptr -> тишина.
        void render(const StationBuffer* buf, float* outL, float* outR, int n) noexcept
        {
            if (buf == nullptr || buf->length <= 0)
            {
                std::fill(outL, outL + n, 0.f);
                std::fill(outR, outR + n, 0.f);
                bufId_ = 0;
                return;
            }
            if (buf->id != bufId_)            // новый файл: с начала, короткое появление (без щелчка)
            {
                bufId_ = buf->id;
                pos_ = 0;
                fadeIn_ = 0.f;
            }
            const int len = buf->length;
            const int xf = std::min(len / 4, std::max(1, (int) (buf->sampleRate * 0.01)));   // 10 мс
            const float* l = buf->ch[0].data();
            const float* r = buf->numChannels > 1 ? buf->ch[1].data() : l;
            const float fadeStep = 1.f / (float) std::max(1, (int) (buf->sampleRate * 0.01));

            for (int i = 0; i < n; ++i)
            {
                float a = l[pos_], b = r[pos_];
                const int tailStart = len - xf;
                if (pos_ >= tailStart && len > 2 * xf)
                {
                    // Конец цикла смешивается с началом: к моменту перехода начало уже отыграно.
                    const int h = pos_ - tailStart;
                    const float t = ((float) h + 0.5f) / (float) xf;
                    const float gOut = std::cos(t * 1.5707963f), gIn = std::sin(t * 1.5707963f);
                    a = a * gOut + l[h] * gIn;
                    b = b * gOut + r[h] * gIn;
                }
                if (fadeIn_ < 1.f) { fadeIn_ = std::min(1.f, fadeIn_ + fadeStep); a *= fadeIn_; b *= fadeIn_; }
                outL[i] = a; outR[i] = b;
                if (++pos_ >= len) pos_ = (len > 2 * xf) ? xf : 0;
            }
        }

    private:
        int pos_ = 0;
        std::uint64_t bufId_ = 0;
        float fadeIn_ = 1.f;
    };
}
