#pragma once
// Воспроизведение отрезка файла станции (StationRegion): LOOP (по кругу, равномощностный кроссфейд на стыке)
// или ONE-SHOT (один раз с fade-out в конце, затем тишина). fade-in - при каждом старте с НАЧАЛА.
// park() - затухание ~12 мс и пауза на НАЧАЛЕ; restart() - с НАЧАЛА. Без JUCE, без аллокаций.
#include "StationBuffer.h"
#include <algorithm>
#include <cmath>

namespace fmrx
{
    class LoopPlayer
    {
    public:
        void reset() noexcept { pos_ = -1; bufId_ = 0; fade_ = 0.f; target_ = 1.f; ended_ = false; inPos_ = 0; }

        void restart() noexcept { pos_ = -1; fade_ = 0.f; target_ = 1.f; ended_ = false; }   // -1: с НАЧАЛА отрезка
        void park() noexcept { target_ = 0.f; }
        bool parked() const noexcept { return target_ <= 0.f && fade_ <= 0.f; }
        bool ended() const noexcept { return ended_; }
        int position() const noexcept { return pos_; }                 // для индикатора (аудиопоток)

        // Пишет n сэмплов. Возвращает true, если в этом блоке one-shot дошёл до КОНЦА отрезка.
        bool render(const StationBuffer* buf, const StationRegion& regionIn, float* outL, float* outR, int n, bool loop) noexcept
        {
            if (buf == nullptr || buf->length <= 0)
            {
                std::fill(outL, outL + n, 0.f);
                std::fill(outR, outR + n, 0.f);
                bufId_ = 0;
                return false;
            }
            const StationRegion rg = regionIn.clampedTo(buf->length);
            if (buf->id != bufId_)                                     // новый файл: с начала отрезка, мягко
            {
                bufId_ = buf->id;
                pos_ = -1; fade_ = 0.f; ended_ = false;
            }
            if (pos_ < 0 || pos_ < rg.start || pos_ >= rg.end) { pos_ = rg.start; inPos_ = 0; }   // отрезок сдвинули

            const int len = rg.end - rg.start;
            const int xf = std::min(len / 4, std::max(1, (int) (buf->sampleRate * 0.01)));     // 10 мс
            const float* l = buf->ch[0].data();
            const float* r = buf->numChannels > 1 ? buf->ch[1].data() : l;
            const float upStep = 1.f / (float) std::max(1, (int) (buf->sampleRate * 0.010));
            const float downStep = 1.f / (float) std::max(1, (int) (buf->sampleRate * 0.012));
            const int fadeOut = std::max(rg.fadeOut, (int) (buf->sampleRate * 0.005));
            bool endedNow = false;

            for (int i = 0; i < n; ++i)
            {
                if (ended_ || (target_ <= 0.f && fade_ <= 0.f)) { outL[i] = outR[i] = 0.f; continue; }

                float a = l[pos_], b = r[pos_];
                if (loop && len > 2 * xf && pos_ >= rg.end - xf)
                {
                    // Конец отрезка смешивается с его началом: к моменту перехода начало уже отыграно.
                    const int h = pos_ - (rg.end - xf);
                    const float t = ((float) h + 0.5f) / (float) xf;
                    const float gOut = std::cos(t * 1.5707963f), gIn = std::sin(t * 1.5707963f);
                    a = a * gOut + l[rg.start + h] * gIn;
                    b = b * gOut + r[rg.start + h] * gIn;
                }
                else if (! loop && pos_ >= rg.end - fadeOut)               // one-shot: fade-out к КОНЦУ
                {
                    const float g = (float) (rg.end - pos_) / (float) fadeOut;
                    a *= g; b *= g;
                }
                if (rg.fadeIn > 0 && inPos_ < rg.fadeIn)                    // fade-in от НАЧАЛА
                {
                    const float g = (float) inPos_ / (float) rg.fadeIn;
                    a *= g; b *= g;
                }

                if (fade_ < target_) fade_ = std::min(target_, fade_ + upStep);
                else if (fade_ > target_) fade_ = std::max(target_, fade_ - downStep);
                outL[i] = a * fade_; outR[i] = b * fade_;

                ++inPos_;
                if (++pos_ >= rg.end)
                {
                    if (loop) pos_ = (len > 2 * xf) ? rg.start + xf : rg.start;
                    else { ended_ = true; endedNow = true; pos_ = rg.start; inPos_ = 0; }
                }
                if (target_ <= 0.f && fade_ <= 0.f) { pos_ = rg.start; inPos_ = 0; }   // пауза: встали на НАЧАЛО
            }
            return endedNow;
        }

    private:
        int pos_ = -1, inPos_ = 0;
        std::uint64_t bufId_ = 0;
        float fade_ = 1.f, target_ = 1.f;
        bool ended_ = false;
    };
}
