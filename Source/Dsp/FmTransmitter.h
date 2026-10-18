#pragma once
// Передатчик одной станции: стерео-звук хоста -> ФНЧ 15 кГц -> преэмфазис -> лимитер -> MPX
// (L+R, пилот 19 кГц, L-R на 38 кГц DSB-SC) -> ЧМ в комплексной полосе со сдвигом на частоту станции.
//
// Две частоты: pushHost() вызывается раз в сэмпл хоста, nextAir(p) - L раз после него (p = 0..L-1).
// По мотивам radiofm.mod / radiofm.encstereo (Henrik Forssell, MIT), переписано на C++.
#include "Biquad.h"
#include "FirstOrder.h"
#include "FmCommon.h"
#include "Polyphase.h"

namespace fmrx::dsp
{
    class FmTransmitter
    {
    public:
        void prepare(double hostFs, int oversampling, std::uint32_t seed)
        {
            hostFs_ = hostFs;
            os_ = oversampling;
            airFs_ = hostFs * oversampling;

            const double fc = std::min(kAudioBwHz, 0.40 * hostFs);
            const auto s1 = BiquadCoeffs::lowpass(hostFs, fc, 0.5412);  // Баттерворт 4-го порядка
            const auto s2 = BiquadCoeffs::lowpass(hostFs, fc, 1.3066);
            for (int c = 0; c < 2; ++c) { lp1_[c].setCoeffs(s1); lp2_[c].setCoeffs(s2); }

            interpM_.prepare(os_, 32, std::min(17000.0, 0.45 * hostFs), airFs_);
            interpS_.prepare(os_, 32, std::min(17000.0, 0.45 * hostFs), airFs_);

            pilotInc_ = phaseIncrement(kPilotHz, airFs_);
            devScale_ = (float) (kDeviationHz / airFs_ * 4294967296.0);
            pilot_ = seed * 2654435761u;        // у каждой станции своя фаза
            carrier_ = seed * 40503u;
            setEmphasis(50e-6);
            reset();
        }

        void reset()
        {
            for (int c = 0; c < 2; ++c) { lp1_[c].reset(); lp2_[c].reset(); pre_[c].reset(); }
            interpM_.reset(); interpS_.reset();
            activity_ = 0.f;
        }

        void setEmphasis(double tauSec)
        {
            for (auto& p : pre_)
                if (tauSec > 0) p.setPreemphasis(tauSec, 20000.0, hostFs_);
                else            p.setBypass();
        }

        void setStereo(bool s) noexcept { stereo_ = s; }
        // Отстройка станции от частоты настройки приёмника (Гц).
        void setOffsetHz(double hz) noexcept { carrierInc_ = phaseIncrement(hz, airFs_); }
        void setAmplitude(float a) noexcept { amp_ = a; }

        // Один сэмпл звука хоста.
        void pushHost(float l, float r) noexcept
        {
            l = limit(pre_[0].process(lp2_[0].process(lp1_[0].process(l))));
            r = limit(pre_[1].process(lp2_[1].process(lp1_[1].process(r))));
            const float m = 0.5f * (l + r);
            const float s = stereo_ ? 0.5f * (l - r) : 0.f;
            interpM_.push(m);
            interpS_.push(s);
            activity_ += 0.001f * (std::fabs(m) - activity_);
        }

        // Один сэмпл эфира (фаза интерполятора p).
        Cplx nextAir(int p) noexcept
        {
            const auto& T = SineTable::get();
            const float m = interpM_.phase(p);
            const float s = interpS_.phase(p);
            pilot_ += pilotInc_;
            float mpx = 0.9f * m;
            if (stereo_)
                mpx += 0.9f * s * T.sin(pilot_ << 1) + 0.1f * T.sin(pilot_);   // 38 кГц = удвоенная фаза пилота
            mpx = std::clamp(mpx, -1.5f, 1.5f);
            carrier_ += carrierInc_ + (Phase) (std::int32_t) (mpx * devScale_);
            return T.expj(carrier_) * amp_;
        }

        float activity() const noexcept { return activity_; }
        double latencyHostSamples() const noexcept { return interpM_.latencyIn(); }

    private:
        // Мягкий лимитер «вещательной обработки»: прозрачен до 0.8, затем плавно к 1.0.
        static float limit(float x) noexcept
        {
            const float a = std::fabs(x);
            if (a <= 0.8f) return x;
            const float y = 0.8f + 0.2f * std::tanh((a - 0.8f) * 5.0f);
            return x < 0 ? -y : y;
        }

        double hostFs_ = 48000.0, airFs_ = 384000.0;
        int os_ = 8;
        Biquad lp1_[2], lp2_[2];
        FirstOrder pre_[2];
        PolyphaseInterpolator interpM_, interpS_;
        Phase pilot_ = 0, pilotInc_ = 0, carrier_ = 0, carrierInc_ = 0;
        float devScale_ = 0.f, amp_ = 1.f, activity_ = 0.f;
        bool stereo_ = true;
    };
}
