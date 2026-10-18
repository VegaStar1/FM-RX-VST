#pragma once
// Полифазные FIR-интерполятор (×L) и дециматор (÷L) для перехода «частота хоста <-> частота эфира».
// Коэффициенты считаются в prepare() (вне аудиопотока), обработка - без аллокаций.
#include "FmCommon.h"
#include <algorithm>
#include <vector>

namespace fmrx::dsp
{
    namespace detail
    {
        inline double besselI0(double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k)
            {
                const double t = x / (2.0 * k);
                term *= t * t;
                sum += term;
                if (term < 1e-12 * sum) break;
            }
            return sum;
        }

        // Прототип ФНЧ: оконный sinc (Кайзер), длина n, срез fc (Гц) на частоте fs. Сумма = 1.
        inline std::vector<float> kaiserLowpass(int n, double fc, double fs, double beta)
        {
            std::vector<double> h((size_t) n);
            const double m = (n - 1) * 0.5, wc = fc / fs, i0b = besselI0(beta);
            double sum = 0;
            for (int i = 0; i < n; ++i)
            {
                const double x = i - m;
                const double s = std::fabs(x) < 1e-9 ? 2.0 * wc : std::sin(kTwoPi * wc * x) / (kPi * x);
                const double r = m > 0 ? x / m : 0.0;
                const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0b;
                h[(size_t) i] = s * w;
                sum += h[(size_t) i];
            }
            std::vector<float> out((size_t) n);
            for (int i = 0; i < n; ++i) out[(size_t) i] = (float) (h[(size_t) i] / sum);
            return out;
        }
    }

    // Интерполятор: на каждый входной сэмпл выдаёт L выходных (фазы 0..L-1).
    class PolyphaseInterpolator
    {
    public:
        void prepare(int factor, int tapsPerPhase, double cutoffHz, double outRate)
        {
            L_ = std::max(1, factor);
            T_ = std::max(2, tapsPerPhase);
            const auto proto = detail::kaiserLowpass(L_ * T_, cutoffHz, outRate, 9.0);
            phases_.assign((size_t) (L_ * T_), 0.f);
            for (int p = 0; p < L_; ++p)
                for (int k = 0; k < T_; ++k)
                    phases_[(size_t) (p * T_ + k)] = proto[(size_t) (k * L_ + p)] * (float) L_;
            hist_.assign((size_t) (2 * T_), 0.f);
            pos_ = 0;
        }
        void reset() { std::fill(hist_.begin(), hist_.end(), 0.f); pos_ = 0; }

        void push(float x) noexcept
        {
            pos_ = (pos_ == 0 ? T_ : pos_) - 1;           // самый новый сэмпл - по меньшему индексу
            hist_[(size_t) pos_] = x;
            hist_[(size_t) (pos_ + T_)] = x;               // дубль: окно всегда непрерывно
        }
        // Выход фазы p для последнего push.
        float phase(int p) const noexcept
        {
            const float* h = &phases_[(size_t) (p * T_)];
            const float* x = &hist_[(size_t) pos_];
            float acc = 0.f;
            for (int k = 0; k < T_; ++k) acc += h[k] * x[k];
            return acc;
        }
        // Групповая задержка в сэмплах входа (хоста).
        double latencyIn() const noexcept { return (double) (L_ * T_ - 1) * 0.5 / (double) L_; }
        int factor() const noexcept { return L_; }

    private:
        int L_ = 1, T_ = 2, pos_ = 0;
        std::vector<float> phases_, hist_;
    };

    // Дециматор: принимает L входных сэмплов, затем выдаёт один выходной.
    class PolyphaseDecimator
    {
    public:
        void prepare(int factor, int tapsPerPhase, double cutoffHz, double inRate)
        {
            L_ = std::max(1, factor);
            N_ = L_ * std::max(2, tapsPerPhase);
            h_ = detail::kaiserLowpass(N_, cutoffHz, inRate, 9.0);
            hist_.assign((size_t) (2 * N_), 0.f);
            pos_ = 0;
        }
        void reset() { std::fill(hist_.begin(), hist_.end(), 0.f); pos_ = 0; }

        void push(float x) noexcept
        {
            pos_ = (pos_ == 0 ? N_ : pos_) - 1;
            hist_[(size_t) pos_] = x;
            hist_[(size_t) (pos_ + N_)] = x;
        }
        float output() const noexcept
        {
            const float* x = &hist_[(size_t) pos_];
            float acc = 0.f;
            for (int k = 0; k < N_; ++k) acc += h_[(size_t) k] * x[k];
            return acc;
        }
        double latencyOut() const noexcept { return (double) (N_ - 1) * 0.5 / (double) L_; }

    private:
        int L_ = 1, N_ = 2, pos_ = 0;
        std::vector<float> h_, hist_;
    };
}
