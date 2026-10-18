#pragma once
// Синтез механических щелчков кнопок приёмника (без сэмплов): короткий шумовой удар -> два резонатора
// корпуса + низкий «тук». Кнопка-фиксатор (MUTE) даёт двойной щелчок: нажатие и защёлка.
#include "Biquad.h"
#include "FmCommon.h"
#include "Rng.h"
#include <array>

namespace fmrx::dsp
{
    enum class ClickKind { Button = 0, LatchOn = 1, LatchOff = 2 };

    class ClickGen
    {
    public:
        void prepare(double fs)
        {
            fs_ = fs;
            rng_.reseed(0xC11Cu);
            reset();
        }
        void reset()
        {
            for (auto& b : res1_) b.reset();
            for (auto& b : res2_) b.reset();
            excite_ = 0.f; thump_ = 0.f; thumpPhase_ = 0;
            for (auto& e : queue_) e.delay = -1;
        }

        // Из аудиопотока: запустить щелчок (уровень 0..1).
        void trigger(ClickKind kind, float level) noexcept
        {
            if (level <= 0.f) return;
            switch (kind)
            {
                case ClickKind::Button:
                    schedule(0, { 2400.f, 6.f, 5200.f, 8.f, 190.f, 0.006f, 0.30f, 0.35f * level });
                    schedule((int) (0.018 * fs_), { 2900.f, 6.f, 6100.f, 8.f, 220.f, 0.004f, 0.15f, 0.16f * level });
                    break;
                case ClickKind::LatchOn:
                    schedule(0, { 1500.f, 5.f, 3800.f, 7.f, 120.f, 0.012f, 0.60f, 0.50f * level });
                    schedule((int) (0.035 * fs_), { 2300.f, 6.f, 5000.f, 8.f, 170.f, 0.006f, 0.30f, 0.30f * level });
                    break;
                case ClickKind::LatchOff:
                    schedule(0, { 1800.f, 5.f, 4400.f, 7.f, 140.f, 0.009f, 0.45f, 0.42f * level });
                    schedule((int) (0.028 * fs_), { 2600.f, 6.f, 5600.f, 8.f, 200.f, 0.005f, 0.20f, 0.22f * level });
                    break;
            }
        }

        bool active() const noexcept
        {
            if (std::fabs(excite_) > 1e-6f || std::fabs(thump_) > 1e-5f || tail_ > 0) return true;
            for (const auto& e : queue_) if (e.delay >= 0) return true;
            return false;
        }

        // Следующий сэмпл щелчка (моно).
        float next() noexcept
        {
            for (auto& e : queue_)
                if (e.delay >= 0 && e.delay-- == 0) start(e.v);

            float y = 0.f;
            if (excite_ > 1e-6f || tail_ > 0)
            {
                const float x = excite_ * (0.6f * rng_.nextBipolar() + (firstSample_ ? 1.f : 0.f));
                firstSample_ = false;
                excite_ *= exciteDecay_;
                y += res1_[0].process(res1_[1].process(x)) * 0.8f + res2_[0].process(res2_[1].process(x)) * 0.5f;
                if (tail_ > 0) --tail_;
            }
            if (std::fabs(thump_) > 1e-5f)
            {
                thumpPhase_ += thumpInc_;
                y += thump_ * SineTable::get().sin(thumpPhase_);
                thump_ *= thumpDecay_;
            }
            return y * gain_;
        }

    private:
        struct Voice { float f1, q1, f2, q2, thumpHz, thumpTau, thumpAmp, amp; };
        struct Pending { int delay = -1; Voice v {}; };

        void schedule(int delay, const Voice& v) noexcept
        {
            for (auto& e : queue_)
                if (e.delay < 0) { e.delay = delay; e.v = v; return; }
        }
        void start(const Voice& v) noexcept
        {
            const auto c1 = BiquadCoeffs::bandpass(fs_, v.f1, v.q1);
            const auto c2 = BiquadCoeffs::bandpass(fs_, v.f2, v.q2);
            for (auto& b : res1_) b.setCoeffs(c1);
            for (auto& b : res2_) b.setCoeffs(c2);
            excite_ = 1.0f;
            firstSample_ = true;
            exciteDecay_ = (float) std::exp(-1.0 / (0.0004 * fs_));
            tail_ = (int) (0.03 * fs_);                       // резонаторы звенят ещё ~30 мс
            thump_ = v.thumpAmp;
            thumpInc_ = phaseIncrement(v.thumpHz, fs_);
            thumpPhase_ = 0;
            thumpDecay_ = (float) std::exp(-1.0 / (v.thumpTau * fs_));
            gain_ = v.amp * 2.4f;                             // резонаторы с Q 5..8 сильно гасят уровень удара
        }

        double fs_ = 48000.0;
        Rng rng_;
        Biquad res1_[2], res2_[2];
        float excite_ = 0.f, exciteDecay_ = 0.9f, thump_ = 0.f, thumpDecay_ = 0.99f, gain_ = 1.f;
        Phase thumpPhase_ = 0, thumpInc_ = 0;
        int tail_ = 0;
        bool firstSample_ = false;
        std::array<Pending, 4> queue_ {};
    };
}
