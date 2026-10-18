#pragma once
// Общие примитивы FM-движка. Без зависимостей от JUCE.
#include <array>
#include <cmath>
#include <cstdint>

namespace fmrx::dsp
{
    inline constexpr double kPi    = 3.14159265358979323846;
    inline constexpr double kTwoPi = 2.0 * kPi;

    // Параметры вещания (как у настоящего УКВ ЧМ-стерео).
    inline constexpr double kDeviationHz  = 75000.0;  // пиковая девиация при MPX = 1
    inline constexpr double kPilotHz      = 19000.0;  // пилот-тон; поднесущая L-R = 2 * пилот
    inline constexpr double kAudioBwHz    = 15000.0;  // полоса звука станции
    inline constexpr double kStationSpreadHz = 130000.0; // половина ширины спектра станции (Карсон)
    inline constexpr double kBandLowMHz   = 87.5;
    inline constexpr double kBandHighMHz  = 108.0;

    struct Cplx
    {
        float re = 0.f, im = 0.f;
        Cplx operator+(Cplx o) const noexcept { return { re + o.re, im + o.im }; }
        Cplx operator-(Cplx o) const noexcept { return { re - o.re, im - o.im }; }
        Cplx operator*(float k) const noexcept { return { re * k, im * k }; }
        Cplx operator*(Cplx o) const noexcept { return { re * o.re - im * o.im, re * o.im + im * o.re }; }
        Cplx& operator+=(Cplx o) noexcept { re += o.re; im += o.im; return *this; }
        Cplx conj() const noexcept { return { re, -im }; }
        float norm2() const noexcept { return re * re + im * im; }
    };

    // Фаза как 32-битное беззнаковое целое: 2^32 = полный оборот, переполнение = естественный перенос.
    using Phase = std::uint32_t;

    inline Phase phaseIncrement(double freqHz, double fs) noexcept
    {
        // Отрицательная частота: оборот в обратную сторону (переполнение беззнакового - определено).
        const double turns = freqHz / fs;
        const double frac = turns - std::floor(turns);
        return (Phase) (std::uint64_t) (frac * 4294967296.0);
    }

    // Таблица синуса 4096 точек + линейная интерполяция: погрешность ~1e-7, без вызовов sin/cos в цикле.
    class SineTable
    {
    public:
        static const SineTable& get()
        {
            static const SineTable t;
            return t;
        }
        float sin(Phase p) const noexcept
        {
            const std::uint32_t idx = p >> kFracBits;
            const float frac = (float) (p & kFracMask) * kFracScale;
            const float a = table_[idx], b = table_[idx + 1];
            return a + (b - a) * frac;
        }
        float cos(Phase p) const noexcept { return sin(p + 0x40000000u); }
        Cplx  expj(Phase p) const noexcept { return { cos(p), sin(p) }; }

    private:
        static constexpr int kBits = 12;
        static constexpr int kSize = 1 << kBits;
        static constexpr int kFracBits = 32 - kBits;
        static constexpr std::uint32_t kFracMask = (1u << kFracBits) - 1u;
        static constexpr float kFracScale = 1.0f / (float) (1u << kFracBits);

        SineTable()
        {
            for (int i = 0; i <= kSize; ++i)
                table_[(size_t) i] = (float) std::sin(kTwoPi * (double) i / (double) kSize);
        }
        std::array<float, kSize + 1> table_ {};
    };

    // Быстрый atan2 (полином 9-й степени после приведения к [0, 1]): ошибка < 1e-5 рад.
    inline float fastAtan2(float y, float x) noexcept
    {
        const float ax = std::fabs(x), ay = std::fabs(y);
        const float mx = ax > ay ? ax : ay;
        if (mx < 1e-30f) return 0.f;
        const float mn = ax > ay ? ay : ax;
        const float a = mn / mx;
        const float s = a * a;
        float r = ((((0.0208351f * s - 0.0851330f) * s + 0.1801410f) * s - 0.3302995f) * s + 0.9998660f) * a;
        if (ay > ax) r = 1.57079637f - r;
        if (x < 0.f) r = 3.14159274f - r;
        return y < 0.f ? -r : r;
    }

    inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
    inline float gainToDb(float g) noexcept  { return 20.0f * std::log10(g > 1e-12f ? g : 1e-12f); }

    // Внутренняя частота «эфира»: целый коэффициент передискретизации, чтобы Fs_air >= 380 кГц
    // (44.1 -> ×9, 48 -> ×8, 88.2 -> ×5, 96 -> ×4, 192 -> ×2).
    inline int chooseOversampling(double hostFs) noexcept
    {
        int os = (int) std::ceil(380000.0 / (hostFs > 1000.0 ? hostFs : 48000.0) - 1e-9);
        return os < 2 ? 2 : (os > 16 ? 16 : os);
    }

    // Насколько далеко от частоты настройки станция ещё может участвовать, не «заворачиваясь»
    // в полосу ПЧ при комплексной дискретизации: Fs_air - спектр станции - половина макс. ПЧ - запас.
    inline double maxStationOffsetHz(double airFs) noexcept
    {
        return airFs - kStationSpreadHz - 100000.0 - 10000.0;   // ПЧ не шире 200 кГц
    }
}
