#pragma once
// Приёмник: ПЧ-фильтр -> ЧМ-демодулятор (квадратурный или ФАПЧ) -> стереодекодер с ФАПЧ по пилоту
// -> децимация до частоты хоста -> деэмфазис, бленд в моно, soft mute, hi-cut.
// Демодуляторы - по мотивам radiofm.demod (умножение на сопряжённый отсчёт) и radiofm.demod2 (PLL),
// Henrik Forssell, MIT; стереодекодер - radiofm.decstereo (пилот, удвоение частоты), но на ФАПЧ.
#include "Biquad.h"
#include "FirstOrder.h"
#include "FmCommon.h"
#include "Polyphase.h"
#include <atomic>

namespace fmrx::dsp
{
    enum class DemodType { Quadrature = 0, Pll = 1 };

    struct ReceiverParams
    {
        float ifBandwidthHz = 150000.f;   // полная ширина ПЧ-фильтра
        DemodType demod = DemodType::Quadrature;
        double deemphasisTau = 50e-6;      // 0 = выкл
        bool forceMono = false;
        float blend = 0.5f;                // 0..1: насколько рано стерео уходит в моно на слабом сигнале
        float softMute = 0.4f;             // 0..1
        float hiCut = 0.5f;                // 0..1
        float noiseSigma = 0.f;            // СКО шума эфира на компоненту (для оценки SNR)
    };

    struct ReceiverMeters
    {
        float rssiDb = -120.f;     // мощность в ПЧ относительно несущей с амплитудой 1
        float snrDb = 0.f;         // оценка отношения сигнал/шум в полосе ПЧ
        float stereo = 0.f;        // 0..1, захват пилота
        float blend = 0.f;         // фактическая доля стерео
        float freqOffsetHz = 0.f;  // средняя отстройка (для AFC и индикатора)
    };

    class Receiver
    {
    public:
        void prepare(double hostFs, int oversampling)
        {
            hostFs_ = hostFs;
            os_ = oversampling;
            airFs_ = hostFs * oversampling;
            ifBw_ = -1.f;
            setIfBandwidth(150000.f);

            // ФНЧ звука 6-го порядка (Баттерворт) на частоте эфира + режекция пилота в L+R.
            const double qs[3] = { 0.5176, 0.7071, 1.9319 };
            for (int i = 0; i < 3; ++i)
            {
                const auto c = BiquadCoeffs::lowpass(airFs_, kAudioBwHz, qs[i]);
                lpM_[i].setCoeffs(c); lpS_[i].setCoeffs(c);
            }
            notch19_.setCoeffs(notch(airFs_, kPilotHz, 4.0));
            pilotBp_.setCoeffs(BiquadCoeffs::bandpass(airFs_, kPilotHz, 8.0));

            const double dc = std::min(16000.0, 0.42 * hostFs);
            decM_.prepare(os_, 32, dc, airFs_);
            decS_.prepare(os_, 32, dc, airFs_);

            // ФАПЧ пилота: полоса ~30 Гц, ζ = 0.707.
            const double wnT = kTwoPi * 30.0 / airFs_;
            pilotKp_ = 2.0 * 0.707 * wnT * kRadToPhase;
            pilotKi_ = wnT * wnT * kRadToPhase;
            pilotInc_ = (double) phaseIncrement(kPilotHz, airFs_);
            pilotAmpK_ = 1.0f - (float) std::exp(-kTwoPi * 8.0 / airFs_);

            // ФАПЧ демодулятора: широкая петля, чтобы следить за девиацией 75 кГц.
            const double wd = kTwoPi * 40000.0 / airFs_;
            pllKp_ = (float) (2.0 * 0.85 * wd);
            pllKi_ = (float) (wd * wd);

            rssiK_  = 1.0f - (float) std::exp(-1.0 / (0.03 * airFs_));
            afcK_   = 1.0f - (float) std::exp(-1.0 / (0.25 * airFs_));
            offFastK_ = 1.0f - (float) std::exp(-1.0 / (0.015 * airFs_));
            hostK_  = 1.0f - (float) std::exp(-1.0 / (0.05 * hostFs_));
            reset();
        }

        void reset()
        {
            for (auto& b : ifI_) b.reset();
            for (auto& b : ifQ_) b.reset();
            for (auto& b : lpM_) b.reset();
            for (auto& b : lpS_) b.reset();
            notch19_.reset(); pilotBp_.reset();
            decM_.reset(); decS_.reset();
            for (int c = 0; c < 2; ++c) { deemph_[c].reset(); hiCut1_[c].reset(); hiCut2_[c].reset(); }
            prev_ = { 1.f, 0.f };
            pilotPhase_ = 0; pilotFreq_ = 0.0; pilotAmp_ = 0.f;
            pllPhase_ = 0.f; pllFreq_ = 0.f;
            rssi_ = 1e-12f; afc_ = 0.f; offFast_ = 0.f; blendNow_ = 0.f; muteNow_ = 1.f;
        }

        void setParams(const ReceiverParams& p)
        {
            p_ = p;
            setIfBandwidth(p.ifBandwidthHz);
            if (std::fabs(p.deemphasisTau - tau_) > 1e-9)
            {
                tau_ = p.deemphasisTau;
                for (auto& d : deemph_)
                    if (tau_ > 0) d.setDeemphasis(tau_, hostFs_);
                    else          d.setBypass();
            }
        }

        // Один отсчёт эфира (комплексная полоса вокруг частоты настройки).
        void processAir(Cplx x) noexcept
        {
            // 1. ПЧ-фильтр: комплексный ФНЧ = одинаковые вещественные фильтры по I и Q.
            for (auto& b : ifI_) x.re = b.process(x.re);
            for (auto& b : ifQ_) x.im = b.process(x.im);
            rssi_ += rssiK_ * (x.norm2() - rssi_);

            // 2. ЧМ-демодуляция -> MPX (1.0 = девиация 75 кГц).
            float mpx;
            if (p_.demod == DemodType::Quadrature)
            {
                const Cplx d = x * prev_.conj();             // приращение фазы за один отсчёт
                prev_ = x;
                mpx = fastAtan2(d.im, d.re) * demodScale_;
            }
            else
            {
                const auto& T = SineTable::get();
                const Cplx lo = T.expj((Phase) (std::int64_t) (pllPhase_ * kRadToPhase));
                const Cplx e = x * lo.conj();
                const float err = fastAtan2(e.im, e.re);
                pllFreq_ = std::clamp(pllFreq_ + pllKi_ * err, -2.5f, 2.5f);
                const float w = pllFreq_ + pllKp_ * err;
                pllPhase_ += w;
                if (pllPhase_ >  (float) kPi) pllPhase_ -= (float) kTwoPi;
                if (pllPhase_ < -(float) kPi) pllPhase_ += (float) kTwoPi;
                mpx = w * demodScale_;
            }
            afc_ += afcK_ * (mpx - afc_);
            offFast_ += offFastK_ * (mpx - offFast_);         // быстрая (~15 мс) оценка отстройки
            mpx = std::clamp(mpx, -1.3f, 1.3f);              // без несущей ЧМ-детектор выдаёт громкий шум

            // 3. ФАПЧ по пилоту 19 кГц.
            const auto& T = SineTable::get();
            const float bp = pilotBp_.process(mpx);
            const Phase theta = pilotPhase_;                 // фаза для ЭТОГО отсчёта (до приращения)
            const float ps = T.sin(theta), pc = T.cos(theta);
            pilotAmp_ += pilotAmpK_ * (2.f * bp * ps - pilotAmp_);
            const float err = std::clamp(2.f * bp * pc / std::max(0.02f, std::fabs(pilotAmp_)), -1.f, 1.f);
            pilotFreq_ = std::clamp(pilotFreq_ + pilotKi_ * err, -pilotInc_ * 0.01, pilotInc_ * 0.01);
            pilotPhase_ += (Phase) (std::int64_t) (pilotInc_ + pilotFreq_ + pilotKp_ * err);

            // 4. Стереодекодер: L+R = MPX без пилота, L-R = MPX * 2 sin(2θ).
            const float mRaw = notch19_.process(mpx - pilotAmp_ * ps);
            const float sRaw = mpx * 2.f * T.sin(theta << 1);
            float m = mRaw, s = sRaw;
            for (auto& b : lpM_) m = b.process(m);
            for (auto& b : lpS_) s = b.process(s);
            decM_.push(m);
            decS_.push(s);
        }

        // После os вызовов processAir: один стерео-отсчёт на частоте хоста.
        void processHost(float& outL, float& outR) noexcept
        {
            const float m = decM_.output() * (1.f / 0.9f);
            const float s = decS_.output() * (1.f / 0.9f);

            // Оценки качества приёма.
            const float noiseIf = 2.f * p_.noiseSigma * p_.noiseSigma * (ifBw_ / (float) airFs_);
            const float sigP = std::max(1e-12f, rssi_ - noiseIf);
            snrDb_ = 10.f * std::log10(sigP / std::max(1e-14f, noiseIf));
            snrDb_ = std::clamp(snrDb_, -20.f, 90.f);
            const float stereoLock = std::clamp((pilotAmp_ - 0.04f) / 0.05f, 0.f, 1.f);

            // Бленд в моно: чем выше blend, тем раньше (при большем SNR) стерео сужается.
            const float full = 18.f + 30.f * p_.blend, mono = full - 14.f;
            const float bySnr = std::clamp((snrDb_ - mono) / (full - mono), 0.f, 1.f);
            const float bTarget = p_.forceMono ? 0.f : stereoLock * bySnr;
            blendNow_ += hostK_ * (bTarget - blendNow_);

            float l = m + blendNow_ * s, r = m - blendNow_ * s;
            l = deemph_[0].process(l);
            r = deemph_[1].process(r);

            // Детектор отстройки: расстроенная сильная станция звучит искажённо, хотя SNR высокий.
            const float offHz = std::fabs(offFast_) * (float) kDeviationHz;
            const float detune = std::clamp((offHz - 20000.f) / 40000.f, 0.f, 1.f);

            // Hi-cut: на слабом или расстроенном сигнале срез опускается до 2.5 кГц.
            const float q = std::clamp((snrDb_ - 6.f) / 30.f, 0.f, 1.f) * (1.f - 0.7f * detune);
            const float fc = 16000.f * std::pow(2500.f / 16000.f, p_.hiCut * (1.f - q));
            if (std::fabs(fc - hiCutFc_) > 20.f)
            {
                hiCutFc_ = fc;
                for (int c = 0; c < 2; ++c) { hiCut1_[c].setLowpass(fc, hostFs_); hiCut2_[c].setLowpass(fc, hostFs_); }
            }
            l = hiCut2_[0].process(hiCut1_[0].process(l));
            r = hiCut2_[1].process(hiCut1_[1].process(r));

            // Soft mute: при SNR ниже ~20 дБ или сильной отстройке звук приглушается (до -30 дБ при softMute = 1).
            const float t = std::clamp((snrDb_ - 6.f) / 16.f, 0.f, 1.f);
            const float smooth = t * t * (3.f - 2.f * t);
            const float badness = std::max(1.f - smooth, detune);
            const float muteTarget = dbToGain(-30.f * p_.softMute * badness);
            muteNow_ += (muteTarget < muteNow_ ? 4.f * hostK_ : hostK_) * (muteTarget - muteNow_);   // приглушаем быстро, открываем плавно

            outL = l * muteNow_;
            outR = r * muteNow_;

            meters_.rssiDb = 10.f * std::log10(std::max(1e-12f, rssi_));
            meters_.snrDb = snrDb_;
            meters_.stereo = stereoLock;
            meters_.blend = blendNow_;
            meters_.freqOffsetHz = afc_ * (float) kDeviationHz;
        }

        const ReceiverMeters& meters() const noexcept { return meters_; }
        double latencyHostSamples() const noexcept { return decM_.latencyOut(); }

    private:
        static constexpr double kRadToPhase = 4294967296.0 / kTwoPi;

        static BiquadCoeffs notch(double fs, double f0, double q)
        {
            const double w0 = kTwoPi * f0 / fs, cw = std::cos(w0), alpha = std::sin(w0) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            BiquadCoeffs c;
            c.b0 = 1.0 / a0; c.b1 = -2.0 * cw / a0; c.b2 = 1.0 / a0;
            c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0;
            return c;
        }

        void setIfBandwidth(float bw)
        {
            bw = std::clamp(bw, 40000.f, 200000.f);
            if (std::fabs(bw - ifBw_) < 1.f) return;
            ifBw_ = bw;
            const double qs[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };   // Баттерворт 8-го порядка
            for (int i = 0; i < 4; ++i)
            {
                const auto c = BiquadCoeffs::lowpass(airFs_, 0.5 * bw, qs[i]);
                ifI_[i].setCoeffs(c); ifQ_[i].setCoeffs(c);
            }
            demodScale_ = (float) (airFs_ / (kTwoPi * kDeviationHz));
        }

        double hostFs_ = 48000.0, airFs_ = 384000.0, tau_ = -1.0;
        int os_ = 8;
        ReceiverParams p_;
        ReceiverMeters meters_;

        Biquad ifI_[4], ifQ_[4], lpM_[3], lpS_[3], notch19_, pilotBp_;
        PolyphaseDecimator decM_, decS_;
        FirstOrder deemph_[2], hiCut1_[2], hiCut2_[2];
        float ifBw_ = -1.f, demodScale_ = 1.f, hiCutFc_ = -1.f;

        Cplx prev_ { 1.f, 0.f };
        float pllPhase_ = 0.f, pllFreq_ = 0.f, pllKp_ = 0.f, pllKi_ = 0.f;

        Phase pilotPhase_ = 0;
        double pilotFreq_ = 0.0, pilotKp_ = 0.0, pilotKi_ = 0.0, pilotInc_ = 0.0;
        float pilotAmp_ = 0.f, pilotAmpK_ = 0.f;

        float rssi_ = 1e-12f, rssiK_ = 0.f, afc_ = 0.f, afcK_ = 0.f, offFast_ = 0.f, offFastK_ = 0.f;
        float snrDb_ = 0.f, blendNow_ = 0.f, muteNow_ = 1.f, hostK_ = 0.f;
    };
}
