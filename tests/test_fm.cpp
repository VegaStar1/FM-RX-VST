// Тесты FM-движка (чистый C++, без JUCE):
//   g++ -std=c++17 -O2 -ISource tests/test_fm.cpp Source/Engine/FmBand.cpp -o fm_tests && ./fm_tests
#include "Engine/FmBand.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace fmrx;

static int g_failed = 0, g_passed = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_passed; } else { ++g_failed; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

namespace
{
    struct Signal
    {
        std::vector<float> l, r;
    };

    // Синусоида на одном или обоих каналах.
    Signal tone(double fs, double seconds, double hz, float ampL, float ampR)
    {
        Signal s;
        const int n = (int) (fs * seconds);
        s.l.resize((size_t) n); s.r.resize((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const float v = (float) std::sin(2.0 * dsp::kPi * hz * i / fs);
            s.l[(size_t) i] = ampL * v; s.r[(size_t) i] = ampR * v;
        }
        return s;
    }

    struct Run
    {
        Signal out;
        BandMeters meters;
        double seconds = 0, wall = 0;
    };

    // Прогон движка: станции получают свои сигналы (nullptr = тишина), блоками по 512.
    Run run(double fs, const BandParams& band, const std::array<StationParams, kNumStations>& st,
            const std::array<const Signal*, kNumStations>& inputs, double seconds)
    {
        FmBand eng;
        eng.prepare(fs, 512);
        const int n = (int) (fs * seconds);
        Run res;
        res.out.l.resize((size_t) n); res.out.r.resize((size_t) n);
        const float* chans[kNumStations][2];
        const float* const* perStation[kNumStations];
        const auto t0 = std::chrono::steady_clock::now();
        for (int pos = 0; pos < n; pos += 512)
        {
            const int len = std::min(512, n - pos);
            for (int i = 0; i < kNumStations; ++i)
            {
                const auto* sig = inputs[(size_t) i];
                if (sig != nullptr && (int) sig->l.size() >= pos + len)
                {
                    chans[i][0] = sig->l.data() + pos; chans[i][1] = sig->r.data() + pos;
                    perStation[i] = chans[i];
                }
                else perStation[i] = nullptr;
            }
            eng.process(perStation, res.out.l.data() + pos, res.out.r.data() + pos, len, band, st);
        }
        res.wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        res.seconds = seconds;
        res.meters = eng.meters();
        return res;
    }

    // Подгонка синусоиды известной частоты (МНК) на отрезке: амплитуда и остаток (THD+N).
    struct Fit { double amp = 0, residualRms = 0, thdn = 1; };
    Fit fitSine(const std::vector<float>& x, double fs, double hz, double fromSec, double toSec)
    {
        const int a = (int) (fromSec * fs), b = std::min((int) x.size(), (int) (toSec * fs));
        double ss = 0, sc = 0, cc = 0, xs = 0, xc = 0, s1 = 0, c1 = 0, x1 = 0;
        const int n = b - a;
        for (int i = a; i < b; ++i)
        {
            const double w = 2.0 * dsp::kPi * hz * i / fs, s = std::sin(w), c = std::cos(w), v = x[(size_t) i];
            ss += s * s; sc += s * c; cc += c * c; xs += v * s; xc += v * c; s1 += s; c1 += c; x1 += v;
        }
        // Нормальные уравнения 3x3 для v ≈ A sin + B cos + D.
        const double M[3][4] = { { ss, sc, s1, xs }, { sc, cc, c1, xc }, { s1, c1, (double) n, x1 } };
        double m[3][4];
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j) m[i][j] = M[i][j];
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r)
                if (r != c)
                {
                    const double f = m[r][c] / m[c][c];
                    for (int k = 0; k < 4; ++k) m[r][k] -= f * m[c][k];
                }
        const double A = m[0][3] / m[0][0], B = m[1][3] / m[1][1], D = m[2][3] / m[2][2];
        double res = 0;
        for (int i = a; i < b; ++i)
        {
            const double w = 2.0 * dsp::kPi * hz * i / fs;
            const double e = x[(size_t) i] - (A * std::sin(w) + B * std::cos(w) + D);
            res += e * e;
        }
        Fit f;
        f.amp = std::sqrt(A * A + B * B);
        f.residualRms = std::sqrt(res / std::max(1, n));
        f.thdn = f.amp > 1e-9 ? f.residualRms / (f.amp / std::sqrt(2.0)) : 1.0;
        return f;
    }

    double rms(const std::vector<float>& x, double fs, double fromSec, double toSec)
    {
        const int a = (int) (fromSec * fs), b = std::min((int) x.size(), (int) (toSec * fs));
        double s = 0;
        for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt(s / std::max(1, b - a));
    }

    bool allFinite(const Signal& s, float limit)
    {
        for (size_t i = 0; i < s.l.size(); ++i)
            if (! std::isfinite(s.l[i]) || ! std::isfinite(s.r[i]) || std::fabs(s.l[i]) > limit || std::fabs(s.r[i]) > limit)
                return false;
        return true;
    }

    BandParams cleanBand(double tune)
    {
        BandParams b;
        b.tuneMHz = tune;
        b.noiseDb = -95.f;
        b.fading = 0.f;
        b.afc = false;
        b.rx.softMute = 0.f;
        b.rx.hiCut = 0.f;
        b.rx.blend = 0.f;
        return b;
    }

    std::array<StationParams, kNumStations> oneStation(double freq, float levelDb, bool stereo)
    {
        std::array<StationParams, kNumStations> st {};
        for (auto& s : st) { s.on = false; }
        st[0].on = true; st[0].freqMHz = freq; st[0].levelDb = levelDb; st[0].stereo = stereo;
        return st;
    }

    double dB(double ratio) { return 20.0 * std::log10(std::max(1e-12, ratio)); }
}

static void testZone()
{
    std::printf("[reception zone]\n");
    CHECK(FmBand::zoneGainDb(0.0, 60000.0, 300000.0) == 0.f, "clean zone is full strength");
    CHECK(FmBand::zoneGainDb(59000.0, 60000.0, 300000.0) == 0.f, "inside clean zone");
    const float mid = FmBand::zoneGainDb(210000.0, 60000.0, 300000.0);
    CHECK(mid < -30.f && mid > -50.f, "noise zone middle %.1f dB", mid);
    CHECK(FmBand::zoneGainDb(400000.0, 60000.0, 300000.0) <= FmBand::kZoneOffDb, "outside the zone");
    CHECK(FmBand::zoneGainDb(-210000.0, 60000.0, 300000.0) == mid, "symmetric");
    CHECK(dsp::chooseOversampling(44100) == 9 && dsp::chooseOversampling(48000) == 8 && dsp::chooseOversampling(96000) == 4,
          "oversampling choice");

    // Широкая зона: станция слышна далеко от центра, но тонет в шуме по мере удаления.
    const double fs = 48000;
    const auto sig = tone(fs, 1.5, 1000.0, 0.5f, 0.5f);
    auto band = cleanBand(98.0);
    band.noiseDb = -60.f; band.cleanZoneHz = 200000.f; band.noiseZoneHz = 800000.f;
    auto inClean = run(fs, band, oneStation(98.15, -6.f, true), { &sig }, 1.5);
    band.tuneMHz = 98.0 - 0.55;                         // середина шумовой зоны
    auto inNoise = run(fs, band, oneStation(98.0, -6.f, true), { &sig }, 1.5);
    band.tuneMHz = 98.0 - 1.2;                          // за пределами зоны
    auto outside = run(fs, band, oneStation(98.0, -6.f, true), { &sig }, 1.5);
    const auto a = fitSine(inClean.out.l, fs, 1000.0, 0.8, 1.4), b = fitSine(inNoise.out.l, fs, 1000.0, 0.8, 1.4);
    const auto c = fitSine(outside.out.l, fs, 1000.0, 0.8, 1.4);
    std::printf("  wide zone: 150 kHz off THD+N %.2f%% | noise zone SNR %.1f dB THD+N %.1f%% | outside amp %.3f\n",
                a.thdn * 100, inNoise.meters.snrDb, b.thdn * 100, c.amp);
    CHECK(a.thdn < 0.05, "wide clean zone keeps it clean 150 kHz away");
    CHECK(b.thdn > a.thdn * 3.0 && b.amp > 0.1, "noise zone: station emerges from the hiss");
    CHECK(c.amp < 0.05, "outside the zone: no station");
}

static void testMute()
{
    std::printf("[mute]\n");
    const double fs = 48000;
    const auto sig = tone(fs, 1.0, 1000.0, 0.5f, 0.5f);
    auto band = cleanBand(100.0); band.noiseDb = -40.f; band.mute = true;
    auto r = run(fs, band, oneStation(98.0, 0.f, true), { &sig }, 1.0);
    CHECK(rms(r.out.l, fs, 0.1, 1.0) == 0.0, "mute silences everything, hiss included");
}

static void testCleanMono(double fs)
{
    std::printf("[clean tone, fs=%.0f]\n", fs);
    const auto sig = tone(fs, 1.5, 1000.0, 0.5f, 0.5f);
    auto r = run(fs, cleanBand(98.0), oneStation(98.0, 0.f, true), { &sig }, 1.5);
    const auto fl = fitSine(r.out.l, fs, 1000.0, 0.8, 1.4);
    const auto fr = fitSine(r.out.r, fs, 1000.0, 0.8, 1.4);
    std::printf("  amp L %.3f R %.3f, THD+N L %.2f%% (%.1f dB), SNR meter %.1f dB, stereo %.2f\n",
                fl.amp, fr.amp, fl.thdn * 100, dB(fl.thdn), r.meters.snrDb, r.meters.stereo);
    CHECK(fl.amp > 0.40 && fl.amp < 0.62, "level preserved (amp %.3f)", fl.amp);
    CHECK(std::fabs(fl.amp - fr.amp) < 0.03, "L/R balance");
    CHECK(fl.thdn < 0.02, "THD+N under 2%% (%.2f%%)", fl.thdn * 100);
    CHECK(r.meters.stereo > 0.8f, "pilot locked (%.2f)", r.meters.stereo);
    CHECK(allFinite(r.out, 1.01f), "finite and bounded");
}

static void testSeparation()
{
    std::printf("[stereo separation]\n");
    const double fs = 48000;
    const auto sig = tone(fs, 1.5, 1000.0, 0.5f, 0.0f);
    auto r = run(fs, cleanBand(98.0), oneStation(98.0, 0.f, true), { &sig }, 1.5);
    const auto fl = fitSine(r.out.l, fs, 1000.0, 0.8, 1.4);
    const auto fr = fitSine(r.out.r, fs, 1000.0, 0.8, 1.4);
    const double sep = dB(fl.amp / std::max(1e-9, fr.amp));
    std::printf("  L %.3f R %.4f -> separation %.1f dB\n", fl.amp, fr.amp, sep);
    CHECK(sep > 25.0, "separation over 25 dB (%.1f)", sep);

    auto rm = run(fs, cleanBand(98.0), oneStation(98.0, 0.f, false), { &sig }, 1.5);
    const auto ml = fitSine(rm.out.l, fs, 1000.0, 0.8, 1.4);
    const auto mr = fitSine(rm.out.r, fs, 1000.0, 0.8, 1.4);
    std::printf("  mono station: L %.3f R %.3f, stereo %.2f\n", ml.amp, mr.amp, rm.meters.stereo);
    CHECK(std::fabs(ml.amp - mr.amp) < 0.02 && rm.meters.stereo < 0.2f, "mono station decodes as mono");

    auto band = cleanBand(98.0); band.rx.forceMono = true;
    auto rf = run(fs, band, oneStation(98.0, 0.f, true), { &sig }, 1.5);
    const auto fl2 = fitSine(rf.out.l, fs, 1000.0, 0.8, 1.4), fr2 = fitSine(rf.out.r, fs, 1000.0, 0.8, 1.4);
    CHECK(std::fabs(fl2.amp - fr2.amp) < 0.02, "MONO switch collapses stereo");
}

static void testDetuneAndNoise()
{
    std::printf("[detune, weak signal, no station]\n");
    const double fs = 48000;
    const auto sig = tone(fs, 1.5, 1000.0, 0.5f, 0.5f);
    auto exact = run(fs, cleanBand(98.0), oneStation(98.0, 0.f, true), { &sig }, 1.5);
    auto off   = run(fs, cleanBand(98.07), oneStation(98.0, 0.f, true), { &sig }, 1.5);
    const auto fe = fitSine(exact.out.l, fs, 1000.0, 0.8, 1.4), fo = fitSine(off.out.l, fs, 1000.0, 0.8, 1.4);
    std::printf("  exact THD+N %.2f%%, detuned 70 kHz THD+N %.2f%%\n", fe.thdn * 100, fo.thdn * 100);
    CHECK(fo.thdn > fe.thdn * 2.0, "detuning degrades the sound");

    BandParams noisy = cleanBand(98.0); noisy.noiseDb = -40.f;
    auto strong = run(fs, noisy, oneStation(98.0, 0.f, true), { &sig }, 1.5);
    auto weak   = run(fs, noisy, oneStation(98.0, -32.f, true), { &sig }, 1.5);
    const auto fs1 = fitSine(strong.out.l, fs, 1000.0, 0.8, 1.4), fw = fitSine(weak.out.l, fs, 1000.0, 0.8, 1.4);
    std::printf("  CNR 40 dB: THD+N %.2f%% (SNR meter %.1f) | CNR 8 dB: THD+N %.1f%% (SNR meter %.1f)\n",
                fs1.thdn * 100, strong.meters.snrDb, fw.thdn * 100, weak.meters.snrDb);
    CHECK(fw.thdn > fs1.thdn * 3.0, "weak station is noisier");
    CHECK(fw.amp > 0.2, "FM: weak station keeps its loudness (amp %.3f)", fw.amp);
    CHECK(strong.meters.snrDb > weak.meters.snrDb + 15.f, "SNR meter follows the signal");

    BandParams b = cleanBand(100.0); b.noiseDb = -60.f;
    auto empty = run(fs, b, oneStation(98.0, 0.f, true), { &sig }, 1.0);
    const double hiss = rms(empty.out.l, fs, 0.5, 1.0);
    b.rx.softMute = 1.f;
    auto muted = run(fs, b, oneStation(98.0, 0.f, true), { &sig }, 1.0);
    const double hissMuted = rms(muted.out.l, fs, 0.5, 1.0);
    std::printf("  between stations: hiss %.3f rms, with soft mute %.3f rms\n", hiss, hissMuted);
    CHECK(hiss > 0.05, "empty band hisses");
    CHECK(hissMuted < hiss * 0.25, "soft mute tames the hiss");
    CHECK(allFinite(empty.out, 1.01f), "empty band finite and bounded");
}

static void testPllAndChannel()
{
    std::printf("[PLL demod, fading, multipath, impulses, AFC]\n");
    const double fs = 48000;
    const auto sig = tone(fs, 1.5, 1000.0, 0.5f, 0.5f);
    auto b = cleanBand(98.0); b.rx.demod = dsp::DemodType::Pll;
    auto p = run(fs, b, oneStation(98.0, 0.f, true), { &sig }, 1.5);
    const auto fp = fitSine(p.out.l, fs, 1000.0, 0.8, 1.4);
    std::printf("  PLL: amp %.3f THD+N %.2f%%\n", fp.amp, fp.thdn * 100);
    CHECK(fp.amp > 0.4 && fp.thdn < 0.05, "PLL demodulates (THD+N %.2f%%)", fp.thdn * 100);

    auto rough = cleanBand(98.0);
    rough.noiseDb = -50.f; rough.fading = 1.f; rough.fadeRateHz = 3.f; rough.multipath = 1.f; rough.impulse = 1.f; rough.drift = 1.f;
    auto rr = run(fs, rough, oneStation(98.0, -6.f, true), { &sig }, 2.0);
    std::printf("  rough channel: finite=%d\n", (int) allFinite(rr.out, 1.01f));
    CHECK(allFinite(rr.out, 1.01f), "rough channel stays finite and bounded");

    auto afc = cleanBand(98.03); afc.afc = true; afc.noiseDb = -70.f;
    auto ra = run(fs, afc, oneStation(98.0, 0.f, true), { &sig }, 4.0);
    std::printf("  AFC from 30 kHz off: correction %.1f kHz\n", ra.meters.afcHz / 1000.0);
    CHECK(ra.meters.afcHz < -20000.f, "AFC pulls towards the station (%.1f kHz)", ra.meters.afcHz / 1000.0);
}

static void testTwoStationsAndCpu()
{
    std::printf("[two stations, CPU]\n");
    const double fs = 48000;
    const auto a = tone(fs, 3.0, 1000.0, 0.5f, 0.5f);
    const auto c = tone(fs, 3.0, 1700.0, 0.5f, 0.5f);
    auto st = oneStation(98.0, 0.f, true);
    st[1].on = true; st[1].freqMHz = 98.1; st[1].levelDb = 0.f; st[1].stereo = true;
    auto r1 = run(fs, cleanBand(98.0), st, { &a, &c }, 1.5);
    const auto f1 = fitSine(r1.out.l, fs, 1000.0, 0.8, 1.4), f2 = fitSine(r1.out.l, fs, 1700.0, 0.8, 1.4);
    auto narrow = cleanBand(98.0); narrow.rx.ifBandwidthHz = 80000.f;
    auto r2 = run(fs, narrow, st, { &a, &c }, 1.5);
    const auto g2 = fitSine(r2.out.l, fs, 1700.0, 0.8, 1.4);
    std::printf("  tuned to A, B 100 kHz away: A %.3f, B leak %.4f (IF 150k) -> %.4f (IF 80k)\n", f1.amp, f2.amp, g2.amp);
    CHECK(f1.amp > 0.35 && f2.amp < f1.amp * 0.3, "capture: the tuned station dominates");
    CHECK(g2.amp < f2.amp * 0.5, "narrow IF rejects the neighbour better");
    auto st200 = st; st200[1].freqMHz = 98.2;
    auto r3 = run(fs, cleanBand(98.0), st200, { &a, &c }, 1.5);
    CHECK(fitSine(r3.out.l, fs, 1700.0, 0.8, 1.4).amp < 0.005, "station 200 kHz away is silent");

    // Производительность: 3 станции в полосе эфира, 3 секунды.
    auto st3 = st; st3[2].on = true; st3[2].freqMHz = 97.95; st3[2].levelDb = -10.f;
    BandParams band = cleanBand(98.03); band.noiseDb = -60.f; band.fading = 0.5f; band.multipath = 0.5f;
    auto perf = run(fs, band, st3, { &a, &c, &a }, 3.0);
    const double load = perf.wall / perf.seconds * 100.0;
    std::printf("  3 active stations: %.1f%% of one core in real time\n", load);
#if defined(__SANITIZE_ADDRESS__) || ! defined(__OPTIMIZE__)
    (void) load;                                   // под санитайзерами/без оптимизации - только информация
#else
    CHECK(load < 60.0, "CPU budget (%.1f%%)", load);
#endif
}

int main()
{
    testZone();
    testMute();
    testCleanMono(48000);
    testCleanMono(44100);
    testCleanMono(96000);
    testSeparation();
    testDetuneAndNoise();
    testPllAndChannel();
    testTwoStationsAndCpu();
    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
