#pragma once
// «Эфир»: что происходит с сигналом между передатчиками и приёмником.
//  * RayleighFader - рэлеевские замирания (по одному на станцию, частота управления = частота хоста)
//  * AirChannel    - многолучёвость, гауссов шум, импульсные помехи (на частоте эфира, для суммы станций)
//  * DriftGenerator - медленный уход частоты гетеродина
// По мотивам radiofm.channel (Henrik Forssell, MIT).
#include "Biquad.h"
#include "FmCommon.h"
#include "Rng.h"
#include <vector>

namespace fmrx::dsp
{
    // Приближённо гауссово число (сумма 4 равномерных, дисперсия 1): дёшево и без log/sqrt.
    inline float gaussian(Rng& rng) noexcept
    {
        return (rng.nextBipolar() + rng.nextBipolar() + rng.nextBipolar() + rng.nextBipolar()) * 0.8660254f;
    }

    // Однополюсный ФНЧ с состоянием в double: годится для очень низких частот среза (сотые доли Гц).
    struct OnePole
    {
        double k = 0.0, y = 0.0;
        void set(double fcHz, double fs) { k = 1.0 - std::exp(-kTwoPi * fcHz / fs); }
        float process(float x) noexcept { y += k * ((double) x - y); return (float) y; }
    };

    class RayleighFader
    {
    public:
        void prepare(double hostFs, std::uint32_t seed)
        {
            fs_ = hostFs;
            rng_.reseed(seed);
            rate_ = -1.f;
            setRate(1.0f);
            reset();
        }
        void reset() { for (auto& f : lp_) for (auto& b : f) b.y = 0.0; }

        void setRate(float hz)
        {
            hz = std::clamp(hz, 0.02f, 40.0f);
            if (std::fabs(hz - rate_) < 1e-4f) return;
            rate_ = hz;
            for (auto& f : lp_) for (auto& b : f) b.set(hz, fs_);
            // Два одинаковых полюса: ENBW = (pi/4) fc. Нормировка до единичной мощности каждой компоненты,
            // итого E|g|^2 = 2 * 0.5 = 1 (каждая компонента с дисперсией 1/2).
            norm_ = (float) (std::sqrt(0.5) / std::sqrt(2.0 / fs_ * (kPi / 4.0) * hz));
        }

        // Комплексный коэффициент канала: depth = 0 - без замираний, depth = 1 - чистый Рэлей (E|g|^2 = 1).
        Cplx next(float depth) noexcept
        {
            const float i = lp_[0][1].process(lp_[0][0].process(gaussian(rng_))) * norm_;
            const float q = lp_[1][1].process(lp_[1][0].process(gaussian(rng_))) * norm_;
            const float d = std::sqrt(depth);      // смешиваем мощности: прямой луч + рассеянная часть
            const float los = std::sqrt(1.f - depth);
            return { los + d * i, d * q };
        }

    private:
        double fs_ = 48000.0;
        float rate_ = -1.f, norm_ = 1.f;
        Rng rng_;
        OnePole lp_[2][2];
    };

    class DriftGenerator
    {
    public:
        void prepare(double hostFs, std::uint32_t seed)
        {
            rng_.reseed(seed);
            lp_.set(0.08, hostFs);
            lp_.y = 0.0;
            // Дисперсия на выходе: (2/fs) * (pi/2) * 0.08 -> нормируем к единице.
            norm_ = (float) (1.0 / std::sqrt(2.0 / hostFs * (kPi / 2.0) * 0.08));
        }
        // Отстройка в Гц при amount 0..1 (порядка ±10 кГц, предел ±25 кГц).
        float next(float amount) noexcept
        {
            const float v = lp_.process(gaussian(rng_)) * norm_;   // ~N(0, 1), медленно меняется
            return std::clamp(v * 0.4f, -1.f, 1.f) * amount * 25000.f;
        }

    private:
        Rng rng_;
        OnePole lp_;
        float norm_ = 1.f;
    };

    class AirChannel
    {
    public:
        void prepare(double airFs, std::uint32_t seed)
        {
            fs_ = airFs;
            rng_.reseed(seed);
            delay_.assign(kDelaySize, {});
            dpos_ = 0;
            impulseDecay_ = (float) std::exp(-1.0 / (30e-6 * airFs));
            mpLfoInc_ = phaseIncrement(0.13, airFs);
            reset();
        }
        void reset()
        {
            std::fill(delay_.begin(), delay_.end(), Cplx{});
            impulse_ = {};
        }

        // noiseDb: мощность шума в полосе 200 кГц относительно несущей с амплитудой 1.
        void setParams(float noiseDb, float multipath, float impulse) noexcept
        {
            const double p200 = std::pow(10.0, noiseDb / 10.0);
            noiseSigma_ = (float) std::sqrt(p200 * fs_ / 200000.0 * 0.5);   // на каждую из компонент I/Q
            multipath_ = std::clamp(multipath, 0.f, 1.f);
            impulseRate_ = std::clamp(impulse, 0.f, 1.f);
            impulseRate_ = impulseRate_ * impulseRate_ * 40.0f / (float) fs_;   // до 40 щелчков в секунду
        }

        Cplx process(Cplx x) noexcept
        {
            // Многолучёвость: отражённая копия с задержкой 3..20 мкс, задержка медленно «плывёт».
            delay_[(size_t) dpos_] = x;
            if (multipath_ > 0.f)
            {
                mpLfo_ += mpLfoInc_;
                const float lfo = 0.5f + 0.5f * SineTable::get().sin(mpLfo_);
                const float dSec = (3e-6f + 17e-6f * multipath_) * (0.85f + 0.3f * lfo);
                const float d = std::min((float) (kDelaySize - 2), dSec * (float) fs_);
                const int di = (int) d;
                const float fr = d - (float) di;
                const Cplx a = delay_[(size_t) ((dpos_ - di) & (kDelaySize - 1))];
                const Cplx b = delay_[(size_t) ((dpos_ - di - 1) & (kDelaySize - 1))];
                const Cplx echo = a * (1.f - fr) + b * fr;
                // Отражение с поворотом фазы: даёт и провалы, и искажения стерео.
                x += echo * Cplx{ -0.55f * multipath_, 0.35f * multipath_ };
            }
            dpos_ = (dpos_ + 1) & (kDelaySize - 1);

            // Импульсные помехи: короткие затухающие всплески (зажигание, бытовые приборы).
            if (impulseRate_ > 0.f && rng_.nextUnit() < impulseRate_)
            {
                const float a = 0.5f + 3.5f * rng_.nextUnit();
                const float ang = (float) kTwoPi * rng_.nextUnit();
                impulse_ = { a * std::cos(ang), a * std::sin(ang) };
            }
            x += impulse_;
            impulse_ = impulse_ * impulseDecay_;

            // Гауссов шум.
            x.re += noiseSigma_ * gaussian(rng_);
            x.im += noiseSigma_ * gaussian(rng_);
            return x;
        }

        float noiseSigma() const noexcept { return noiseSigma_; }

    private:
        static constexpr int kDelaySize = 64;
        double fs_ = 384000.0;
        Rng rng_;
        std::vector<Cplx> delay_;
        int dpos_ = 0;
        Phase mpLfo_ = 0, mpLfoInc_ = 0;
        Cplx impulse_ {};
        float impulseDecay_ = 0.9f, noiseSigma_ = 0.f, multipath_ = 0.f, impulseRate_ = 0.f;
    };
}
