// Тесты управления (SEARCH/SCAN/LOCK/STOP/REWIND/one-shot), плеера станций и щелчков. Чистый C++.
//   g++ -std=c++17 -O2 -ISource tests/test_control.cpp -o control_tests && ./control_tests
#include "Dsp/ClickGen.h"
#include "Engine/RadioControl.h"
#include "Stations/LoopPlayer.h"
#include "Stations/Waveform.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace fmrx;

static int g_failed = 0, g_passed = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_passed; } else { ++g_failed; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

namespace
{
    constexpr double kFs = 48000.0;
    constexpr int kBlock = 512;

    std::array<ControlStation, kNumStations> stations(std::initializer_list<std::pair<double, float>> list, bool oneShot = false)
    {
        std::array<ControlStation, kNumStations> st {};
        int i = 0;
        for (auto& p : list) { st[(size_t) i].usable = true; st[(size_t) i].freqMHz = p.first; st[(size_t) i].levelDb = p.second; st[(size_t) i].oneShot = oneShot; ++i; }
        return st;
    }

    // Прогон управления: param - ручка (её «пишет» эмулятор таймера, как в плагине).
    struct Sim
    {
        RadioControl c;
        ControlParams p;
        std::array<ControlStation, kNumStations> st {};
        Sim() { c.prepare(kFs); p.noiseDb = -60.f; p.cleanZoneHz = 60000; p.noiseZoneHz = 300000; p.speed = 0.6f; }

        void blocks(int n)
        {
            for (int i = 0; i < n; ++i)
            {
                c.beginBlock(p, st, kBlock);
                // таймер: результат движка в параметр, когда поиск не едет
                const int s = c.state();
                if (c.owned() && s != (int) RadioControl::State::Searching && s != (int) RadioControl::State::Scanning)
                    p.tuneParam = c.displayFreq();
            }
        }
        void seconds(double sec) { blocks((int) (sec * kFs / kBlock) + 1); }
    };
}

static void testSearch()
{
    std::printf("[search]\n");
    Sim s;
    s.st = stations({ { 90.0, -6.f }, { 95.0, -45.f }, { 101.0, -10.f } });   // 95.0 слишком слабая
    s.p.tuneParam = 89.0f;
    s.blocks(2);
    s.c.post(RadioCmd::SearchUp);
    s.seconds(6.0);
    std::printf("  from 89.0 up -> %.3f MHz (state %d)\n", s.c.tuneTarget(), s.c.state());
    CHECK(std::fabs(s.c.tuneTarget() - 90.0f) < 0.002f && s.c.state() == 0, "search stops on the next station and settles");
    CHECK(std::fabs(s.p.tuneParam - 90.0f) < 0.002f, "result written back to the knob");

    s.c.post(RadioCmd::SearchUp);
    s.seconds(10.0);
    std::printf("  next up -> %.3f MHz (weak 95.0 skipped)\n", s.c.tuneTarget());
    CHECK(std::fabs(s.c.tuneTarget() - 101.0f) < 0.002f, "weak station skipped, next strong found");

    s.c.post(RadioCmd::SearchUp);
    s.seconds(10.0);
    CHECK(std::fabs(s.c.tuneTarget() - 90.0f) < 0.002f, "search wraps around the band edge");

    s.c.post(RadioCmd::SearchDown);
    s.seconds(10.0);
    CHECK(std::fabs(s.c.tuneTarget() - 101.0f) < 0.002f, "search down");

    s.p.sensitivity = 1.0f;                 // высокая чувствительность ловит и слабую
    s.c.post(RadioCmd::SearchDown);
    s.seconds(10.0);
    CHECK(std::fabs(s.c.tuneTarget() - 95.0f) < 0.002f, "high sensitivity stops on the weak station");

    // Ручка во время поиска прерывает его.
    s.c.post(RadioCmd::SearchUp);
    s.blocks(5);
    s.p.tuneParam = 104.2f;
    s.blocks(3);
    CHECK(s.c.state() == 0 && std::fabs(s.c.tuneTarget() - 104.2f) < 0.001f, "moving the knob cancels search");

    // STOP останавливает поиск на месте.
    s.c.post(RadioCmd::SearchUp);
    s.blocks(20);
    const float mid = s.c.tuneTarget();
    s.c.post(RadioCmd::Stop);
    s.seconds(1.0);
    CHECK(s.c.state() == 0 && std::fabs(s.c.tuneTarget() - mid) < 0.03f, "STOP halts the search where it is");

    // Пустой диапазон: полный круг и возврат.
    Sim e;
    e.st = stations({ { 100.0, -70.f } });
    e.p.tuneParam = 92.f; e.blocks(2);
    e.c.post(RadioCmd::SearchUp);
    e.seconds(30.0);
    CHECK(e.c.state() == 0 && std::fabs(e.c.tuneTarget() - 92.f) < 0.01f, "nothing receivable: back to start after a full circle");
}

static void testScanLock()
{
    std::printf("[scan, lock]\n");
    Sim s;
    s.st = stations({ { 90.0, -6.f }, { 96.0, -6.f } });
    s.p.scanDwellSec = 1.0f;
    s.p.tuneParam = 88.f; s.blocks(2);
    s.c.post(RadioCmd::Scan);
    s.seconds(1.5);
    CHECK(std::fabs(s.c.tuneTarget() - 90.f) < 0.002f && s.c.state() == (int) RadioControl::State::Dwell, "scan dwells on a station");
    s.seconds(3.0);
    CHECK(std::fabs(s.c.tuneTarget() - 96.f) < 0.002f, "scan moves on to the next");

    s.c.post(RadioCmd::Lock);
    s.blocks(2);
    const float f = s.c.tuneTarget();
    s.p.tuneParam = 100.f; s.blocks(3);
    s.c.post(RadioCmd::SearchUp); s.seconds(2.0);
    CHECK(s.c.locked() && std::fabs(s.c.tuneTarget() - f) < 1e-4f, "LOCK ignores knob and search");
    CHECK(std::fabs(s.p.tuneParam - f) < 1e-4f, "while locked the knob shows the locked frequency");
    s.c.post(RadioCmd::Unlock); s.blocks(2);
    s.p.tuneParam = 100.f; s.blocks(2);
    CHECK(! s.c.locked() && std::fabs(s.c.tuneTarget() - 100.f) < 1e-4f, "UNLOCK returns control to the knob");
}

static void testStopRewind()
{
    std::printf("[stop, rewind]\n");
    Sim s;
    s.st = stations({ { 90.0, -6.f }, { 96.0, -6.f } });
    s.p.tuneParam = 90.f; s.blocks(3);
    CHECK(s.c.slotState(0) == RadioControl::Slot::Playing, "loop station plays");
    s.c.post(RadioCmd::Stop); s.blocks(2);
    CHECK(s.c.slotState(0) == RadioControl::Slot::Parked && s.c.slotState(1) == RadioControl::Slot::Playing,
          "STOP parks only the audible station");
    s.blocks(20);
    CHECK(s.c.slotState(0) == RadioControl::Slot::Parked, "stays parked while still tuned");
    s.p.tuneParam = 93.f; s.blocks(3);
    s.p.tuneParam = 90.f; s.blocks(3);
    CHECK(s.c.slotState(0) == RadioControl::Slot::Playing && s.c.consumeRestart(0), "re-tuning starts it from the beginning");

    s.c.post(RadioCmd::Rewind); s.blocks(2);
    CHECK(std::fabs(s.c.tuneTarget() - 87.5f) < 1e-4f && s.c.consumeRestart(0) && s.c.consumeRestart(1),
          "REWIND: everything from the start, tuner to 87.5");
}

static void testOneShot()
{
    std::printf("[one-shot]\n");
    Sim s;
    s.st = stations({ { 90.0, -6.f }, { 96.0, -6.f }, { 103.0, -6.f } }, true);
    s.p.tuneParam = 96.f; s.blocks(3);
    CHECK(s.c.slotState(1) == RadioControl::Slot::Playing && s.c.consumeRestart(1), "tuned one-shot starts from the beginning");
    CHECK(s.c.slotState(0) == RadioControl::Slot::Parked, "untuned one-shot waits at the start");

    s.c.notifyEnded(1);
    s.blocks(2);
    CHECK(s.c.slotState(1) == RadioControl::Slot::Done && s.c.carrierOn(1), "finished one-shot keeps a silent carrier (no hiss burst)");
    CHECK(s.c.handoverActive(), "hand-over pause is active right after the end");
    s.seconds(8.0);
    std::printf("  after end: %.3f MHz\n", s.c.tuneTarget());
    CHECK(std::fabs(s.c.tuneTarget() - 103.f) < 0.002f, "auto-search finds the next station up");
    CHECK(s.c.slotState(2) == RadioControl::Slot::Playing, "and it starts playing");
    CHECK(s.c.slotState(1) == RadioControl::Slot::Parked, "the finished one waits again once we left it");

    s.c.notifyEnded(2);
    s.seconds(10.0);
    CHECK(std::fabs(s.c.tuneTarget() - 90.f) < 0.002f, "auto-search wraps to the next one");

    // Пауза перед автопоиском регулируется.
    for (float delay : { 0.f, 2.f })
    {
        Sim d;
        d.p.endDelaySec = delay;
        d.st = stations({ { 90.0, -6.f }, { 96.0, -6.f } }, true);
        d.p.tuneParam = 90.f; d.blocks(3);
        d.c.notifyEnded(0);
        int blocksBeforeMove = 0;
        while (blocksBeforeMove < 2000 && std::fabs(d.c.tuneTarget() - 90.f) < 1e-4f) { d.blocks(1); ++blocksBeforeMove; }
        const double waited = blocksBeforeMove * (double) kBlock / kFs;
        std::printf("  hand-over delay %.1f s -> dial starts moving after %.2f s\n", delay, waited);
        CHECK(std::fabs(waited - delay) < 0.05, "hand-over delay %.1f s respected", delay);
        d.seconds(6.0);
        CHECK(std::fabs(d.c.tuneTarget() - 96.f) < 0.002f && ! d.c.handoverActive(), "hand-over finishes on the next station");
    }

    Sim solo;
    solo.st = stations({ { 99.0, -6.f } }, true);
    solo.p.tuneParam = 99.f; solo.blocks(3);
    solo.c.consumeRestart(0);
    solo.c.notifyEnded(0);
    solo.seconds(1.5);
    CHECK(solo.c.slotState(0) == RadioControl::Slot::Playing && solo.c.consumeRestart(0), "no other stations: the same one repeats");
}

static void testPlayer()
{
    std::printf("[player modes]\n");
    StationBuffer b;
    b.id = 7; b.numChannels = 1; b.length = 4800; b.sampleRate = kFs;
    b.ch[0].assign(4800, 0.5f);
    LoopPlayer pl;
    std::vector<float> l(512), r(512);
    int ends = 0;
    for (int i = 0; i < 20; ++i) ends += pl.render(&b, StationRegion {}, l.data(), r.data(), 512, false) ? 1 : 0;
    CHECK(ends == 1 && pl.ended() && l[100] == 0.f, "one-shot ends once and then is silent");
    pl.restart();
    pl.render(&b, StationRegion {}, l.data(), r.data(), 512, false);
    CHECK(l[511] > 0.4f, "restart plays again");
    pl.park();
    pl.render(&b, StationRegion {}, l.data(), r.data(), 512, false);
    pl.render(&b, StationRegion {}, l.data(), r.data(), 512, false);
    CHECK(pl.parked() && l[511] == 0.f, "park fades out and stops");
    ends = 0;
    pl.restart();
    for (int i = 0; i < 40; ++i) ends += pl.render(&b, StationRegion {}, l.data(), r.data(), 512, true) ? 1 : 0;
    CHECK(ends == 0 && l[300] > 0.4f, "loop never ends");
}

static void testRegion()
{
    std::printf("[region, waveform, auto-trim]\n");
    // Файл: 0.2 с тишины, 0.6 с синуса, 0.2 с тишины.
    StationBuffer b;
    b.id = 9; b.numChannels = 1; b.sampleRate = kFs; b.length = (int) kFs;
    b.ch[0].assign((size_t) b.length, 0.f);
    for (int i = (int) (0.2 * kFs); i < (int) (0.8 * kFs); ++i) b.ch[0][(size_t) i] = 0.5f * (float) std::sin(2 * 3.14159265 * 300 * i / kFs);

    const auto t = findSilenceTrim(b, -50.f);
    std::printf("  auto-trim: %.3f .. %.3f s\n", t.startSec, t.endSec);
    CHECK(t.found && std::fabs(t.startSec - 0.19) < 0.005 && std::fabs(t.endSec - 0.81) < 0.005, "silence trimmed with 10 ms margin");

    const auto pk = computePeaks(b, 100);
    CHECK(pk->buckets == 100 && pk->mx[0][5] < 0.01f && pk->mx[0][50] > 0.45f && pk->mn[0][50] < -0.45f, "peaks follow the waveform");

    const int z = snapToZeroCrossing(b, (int) (0.5 * kFs) + 7, 96);
    CHECK(std::fabs(b.ch[0][(size_t) z]) < 0.02f, "marker snaps to a zero crossing");

    // One-shot по отрезку: начинается с НАЧАЛА, кончается на КОНЦЕ.
    StationRegion rg; rg.start = (int) (0.2 * kFs); rg.end = (int) (0.5 * kFs); rg.fadeIn = 0; rg.fadeOut = 0;
    LoopPlayer pl;
    std::vector<float> l(512), r(512);
    int rendered = 0, endBlock = -1;
    for (int k = 0; k < 60 && endBlock < 0; ++k)
    {
        if (pl.render(&b, rg, l.data(), r.data(), 512, false)) endBlock = k;
        rendered += 512;
    }
    const double playedSec = (endBlock + 1) * 512 / kFs;
    std::printf("  one-shot region 0.3 s -> ended after ~%.2f s\n", playedSec);
    CHECK(endBlock >= 0 && std::fabs(playedSec - 0.3) < 0.02, "one-shot plays exactly the region");

    pl.restart();
    pl.render(&b, rg, l.data(), r.data(), 64, true);
    CHECK(pl.position() == rg.start + 64, "restart begins at the START marker");
    pl.park();
    for (int k = 0; k < 4; ++k) pl.render(&b, rg, l.data(), r.data(), 512, true);
    CHECK(pl.parked() && pl.position() == rg.start, "park returns to the START marker");

    // Loop по отрезку никогда не выходит за его границы.
    pl.restart();
    bool inside = true;
    for (int k = 0; k < 100; ++k)
    {
        pl.render(&b, rg, l.data(), r.data(), 512, true);
        inside = inside && pl.position() >= rg.start && pl.position() < rg.end;
    }
    CHECK(inside, "loop stays inside the region");

    // Сдвиг отрезка во время игры: позиция переходит в новый отрезок.
    StationRegion rg2 = rg; rg2.start = (int) (0.6 * kFs); rg2.end = (int) (0.7 * kFs);
    pl.render(&b, rg2, l.data(), r.data(), 16, true);
    CHECK(pl.position() >= rg2.start && pl.position() < rg2.end, "moving the markers moves playback");

    StationRegion bad; bad.start = 5000; bad.end = 10;
    const auto c = bad.clampedTo(b.length);
    CHECK(c.start == 0 && c.end == b.length, "invalid region falls back to the whole file");
}

static void testClicks()
{
    std::printf("[clicks]\n");
    dsp::ClickGen c;
    c.prepare(kFs);
    c.trigger(dsp::ClickKind::LatchOn, 1.f);
    float peak = 0.f; int len = 0;
    for (int i = 0; i < 9600; ++i)
    {
        const float v = c.next();
        peak = std::max(peak, std::fabs(v));
        if (std::fabs(v) > 1e-3f) len = i;
    }
    std::printf("  latch click: peak %.2f, audible %.1f ms\n", peak, len / kFs * 1000.0);
    CHECK(peak > 0.1f && peak < 1.0f, "audible but not clipping");
    CHECK(len > (int) (0.03 * kFs) && len < (int) (0.15 * kFs), "double click lasts tens of ms");
    CHECK(! c.active(), "click finishes");
    c.trigger(dsp::ClickKind::Button, 0.f);
    CHECK(! c.active(), "zero level = no click");
}

int main()
{
    testSearch();
    testScanLock();
    testStopRewind();
    testOneShot();
    testPlayer();
    testRegion();
    testClicks();
    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
