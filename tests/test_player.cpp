// Тесты подготовки файлов и плеера станций (чистый C++, без JUCE).
#include "Stations/LoopPlayer.h"
#include "Stations/StationPrep.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace fmrx;

static int g_failed = 0, g_passed = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_passed; } else { ++g_failed; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main()
{
    // Подготовка: 1 с синуса 440 Гц на 44.1 кГц -> 48 кГц, нормализация.
    const int n = 44100;
    std::vector<float> l(n), r(n);
    for (int i = 0; i < n; ++i) { l[i] = 0.1f * (float) std::sin(2 * 3.14159265 * 440 * i / 44100.0); r[i] = -l[i]; }
    const float* ch[2] = { l.data(), r.data() };
    PrepareOptions opt; opt.targetRate = 48000.0;
    auto buf = prepareStation(ch, 2, n, 44100.0, opt);
    CHECK(buf != nullptr, "prepared");
    CHECK(buf && std::abs(buf->length - 48000) <= 2, "resampled length %d", buf ? buf->length : -1);
    CHECK(buf && buf->numChannels == 2, "stereo kept");

    // Плеер: непрерывность на стыке цикла (нет скачков сильнее, чем у самого синуса).
    LoopPlayer p;
    std::vector<float> oL(512), oR(512);
    float prev = 0.f, maxJump = 0.f;
    bool firstSample = true;
    for (int b = 0; b < 300; ++b)        // 3.2 с: два прохода через стык
    {
        p.render(buf.get(), oL.data(), oR.data(), 512);
        for (int i = 0; i < 512; ++i)
        {
            if (! firstSample && b > 2) maxJump = std::max(maxJump, std::fabs(oL[i] - prev));
            prev = oL[i]; firstSample = false;
        }
    }
    const float sinePeak = 0.1f * buf->normalizeGain;
    const float sineStep = sinePeak * 2.f * 3.14159265f * 440.f / 48000.f;
    std::printf("  max step %.4f (sine step %.4f)\n", maxJump, sineStep);
    CHECK(maxJump < sineStep * 2.0f, "loop seam is smooth");

    // Пустой слот - тишина; новый буфер - мягкое появление с начала.
    p.render(nullptr, oL.data(), oR.data(), 512);
    bool silent = true;
    for (float v : oL) silent = silent && v == 0.f;
    CHECK(silent, "null buffer renders silence");
    p.render(buf.get(), oL.data(), oR.data(), 64);
    CHECK(std::fabs(oL[0]) < 1e-3f, "fade-in after switching buffers");

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
