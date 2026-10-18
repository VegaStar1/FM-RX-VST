#pragma once
// Фильтры первого порядка через билинейное преобразование: преэмфазис / деэмфазис, однополюсный ФНЧ.
#include "FmCommon.h"
#include <algorithm>

namespace fmrx::dsp
{
    class FirstOrder
    {
    public:
        // H(s) = (nb0 + nb1*s) / (na0 + na1*s)
        void setAnalog(double nb0, double nb1, double na0, double na1, double fs) noexcept
        {
            const double K = 2.0 * fs;
            const double a0 = na0 + na1 * K, a1 = na0 - na1 * K;
            b0_ = (nb0 + nb1 * K) / a0;
            b1_ = (nb0 - nb1 * K) / a0;
            a1_ = a1 / a0;
        }
        // Преэмфазис τ (с полкой на fShelfHz, чтобы подъём не уходил в бесконечность).
        void setPreemphasis(double tauSec, double fShelfHz, double fs) noexcept
        {
            setAnalog(1.0, tauSec, 1.0, 1.0 / (kTwoPi * fShelfHz), fs);
        }
        void setDeemphasis(double tauSec, double fs) noexcept { setAnalog(1.0, 0.0, 1.0, tauSec, fs); }
        void setLowpass(double fcHz, double fs) noexcept
        {
            // Предыскажение частоты среза, чтобы fc совпадала после билинейного преобразования.
            const double fcw = fs / kPi * std::tan(kPi * std::min(fcHz, 0.45 * fs) / fs);
            setAnalog(1.0, 0.0, 1.0, 1.0 / (kTwoPi * fcw), fs);
        }
        void setBypass() noexcept { b0_ = 1.0; b1_ = 0.0; a1_ = 0.0; }
        void reset() noexcept { x1_ = y1_ = 0.0; }

        float process(float xf) noexcept
        {
            const double x = xf;
            const double y = b0_ * x + b1_ * x1_ - a1_ * y1_;
            x1_ = x; y1_ = y;
            return (float) y;
        }

    private:
        double b0_ = 1.0, b1_ = 0.0, a1_ = 0.0;
        double x1_ = 0.0, y1_ = 0.0;
    };
}
