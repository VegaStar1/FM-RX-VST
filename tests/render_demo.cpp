// Демо-рендер: три синтезированные станции и проход ручкой TUNE по диапазону -> WAV.
//   g++ -std=c++17 -O2 -ISource tests/render_demo.cpp Source/Engine/FmBand.cpp -o render_demo && ./render_demo out.wav
#include "Engine/FmBand.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace fmrx;

namespace
{
    constexpr double kFs = 48000.0;

    double note(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

    // Простой «инструмент»: несколько гармоник с затуханием.
    float pluck(double t, double f, double decay)
    {
        if (t < 0) return 0.f;
        const double env = std::exp(-t * decay) * std::min(1.0, t * 400.0);
        double v = 0;
        for (int h = 1; h <= 4; ++h) v += std::sin(2.0 * dsp::kPi * f * h * t) / (h * h);
        return (float) (v * env);
    }

    void writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r)
    {
        FILE* f = std::fopen(path, "wb");
        if (! f) return;
        const std::uint32_t n = (std::uint32_t) l.size(), dataBytes = n * 4, rate = (std::uint32_t) kFs;
        auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
        auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
        std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
        u32(16); u16(1); u16(2); u32(rate); u32(rate * 4); u16(4); u16(16);
        std::fwrite("data", 1, 4, f); u32(dataBytes);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const std::int16_t a = (std::int16_t) std::lround(std::clamp(l[i], -1.f, 1.f) * 32767.f);
            const std::int16_t b = (std::int16_t) std::lround(std::clamp(r[i], -1.f, 1.f) * 32767.f);
            std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
        }
        std::fclose(f);
    }
}

int main(int argc, char** argv)
{
    const double seconds = 32.0;
    const int n = (int) (kFs * seconds);
    std::vector<float> s1l(n), s1r(n), s2l(n), s2r(n), s3l(n), s3r(n);

    const int arp[8] = { 60, 64, 67, 72, 67, 64, 62, 65 };
    const int chordA[3] = { 57, 60, 64 }, chordB[3] = { 53, 57, 60 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kFs;
        // Станция 1: стерео-арпеджио, ноты по очереди слева и справа.
        const double step = 0.25;
        const int k = (int) (t / step);
        const double tt = t - k * step;
        const float a = 0.35f * pluck(tt, note(arp[k % 8]), 6.0);
        s1l[i] = (k % 2 == 0) ? a : 0.3f * a;
        s1r[i] = (k % 2 == 0) ? 0.3f * a : a;
        // Станция 2: медленный пэд с панорамой.
        const int* ch = ((int) (t / 2.0) % 2 == 0) ? chordA : chordB;
        double pad = 0;
        for (int j = 0; j < 3; ++j) pad += std::sin(2 * dsp::kPi * note(ch[j]) * t) + 0.3 * std::sin(2 * dsp::kPi * note(ch[j] + 12) * t * 1.003);
        const float pan = (float) (0.5 + 0.5 * std::sin(2 * dsp::kPi * 0.15 * t));
        s2l[i] = (float) (0.12 * pad) * (1.f - pan);
        s2r[i] = (float) (0.12 * pad) * pan;
        // Станция 3: бас и «бочка» в моно.
        const double beat = 0.5;
        const double tb = t - std::floor(t / beat) * beat;
        const float kick = (float) (std::sin(2 * dsp::kPi * (50 + 120 * std::exp(-tb * 30)) * tb) * std::exp(-tb * 9));
        const float bass = 0.4f * pluck(std::fmod(t, 1.0), note(36 + ((int) t % 4 == 3 ? 5 : 0)), 2.0);
        s3l[i] = s3r[i] = 0.5f * kick + bass;
    }

    std::array<StationParams, kNumStations> st {};
    for (auto& s : st) s.on = false;
    st[0] = { true, 96.0, -6.f, true };
    st[1] = { true, 98.0, -14.f, true };
    st[2] = { true, 100.4, -24.f, false };

    BandParams band;
    band.noiseDb = -52.f; band.fading = 0.35f; band.fadeRateHz = 0.7f; band.multipath = 0.25f; band.impulse = 0.15f;
    band.afc = true;
    band.rx.softMute = 0.55f; band.rx.hiCut = 0.4f; band.rx.blend = 0.5f;

    // Ручка TUNE: стоянки на станциях, медленные переходы между ними.
    struct Key { double t, f; };
    const Key keys[] = { { 0, 95.5 }, { 2, 96.0 }, { 8, 96.0 }, { 13, 98.0 }, { 19, 98.0 }, { 24, 100.4 }, { 30, 100.4 }, { 32, 101.0 } };

    FmBand eng;
    eng.prepare(kFs, 512);
    std::vector<float> outL(n), outR(n);
    for (int pos = 0; pos < n; pos += 512)
    {
        const int len = std::min(512, n - pos);
        const double t = pos / kFs;
        for (size_t k = 0; k + 1 < sizeof(keys) / sizeof(keys[0]); ++k)
            if (t >= keys[k].t && t < keys[k + 1].t)
            {
                const double u = (t - keys[k].t) / (keys[k + 1].t - keys[k].t);
                band.tuneMHz = keys[k].f + (keys[k + 1].f - keys[k].f) * u;
            }
        const float* c1[2] = { s1l.data() + pos, s1r.data() + pos };
        const float* c2[2] = { s2l.data() + pos, s2r.data() + pos };
        const float* c3[2] = { s3l.data() + pos, s3r.data() + pos };
        const float* const* in[kNumStations] = { c1, c2, c3, nullptr, nullptr, nullptr, nullptr, nullptr };
        eng.process(in, outL.data() + pos, outR.data() + pos, len, band, st);
    }
    writeWav(argc > 1 ? argv[1] : "fm_demo.wav", outL, outR);
    std::printf("written %d samples\n", n);
    return 0;
}
