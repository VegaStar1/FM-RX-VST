// Консольная проверка плагина без DAW: звук через processBlock, сохранение/восстановление состояния,
// снимок интерфейса в PNG. Сборка: -DFMRX_BUILD_CHECK=ON, запуск: FmRxCheck <station.wav> <out.png>
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <cstdio>

using namespace fmrx;

static int g_failed = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++g_failed; std::printf("  FAIL: %s\n", msg); } else std::printf("  ok: %s\n", msg); } while (0)

static void setParam(FmRxProcessor& p, const juce::String& id, float value)
{
    auto* prm = p.state().getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(value));
}

static double runBlocks(FmRxProcessor& p, int blocks, bool& finite)
{
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    double sum = 0; long long cnt = 0;
    finite = true;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        p.processBlock(buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                const float v = buf.getSample(c, i);
                if (! std::isfinite(v) || std::fabs(v) > 1.01f) finite = false;
                sum += (double) v * v; ++cnt;
            }
    }
    return std::sqrt(sum / (double) std::max(1LL, cnt));
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File wav(argc > 1 ? juce::String(argv[1]) : juce::String());
    const juce::File png(argc > 2 ? juce::String(argv[2]) : juce::String("fmrx_ui.png"));

    auto proc = std::make_unique<FmRxProcessor>();
    proc->setPlayConfigDetails(2, 2, 48000.0, 512);
    proc->prepareToPlay(48000.0, 512);
    std::printf("latency %d samples\n", proc->getLatencySamples());

    bool finite = true;
    const double silentRms = runBlocks(*proc, 50, finite);
    CHECK(finite && silentRms < 0.5, "empty band: finite output");

    // Две станции из файла, одна на входе.
    proc->stations().loadFileAsync(0, wav);
    proc->stations().loadFileAsync(4, wav);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1500);
    CHECK(proc->stations().hasAudio(0), "file station loaded");
    proc->stations().setName(0, "NIGHT DRIVE");
    proc->stations().setName(4, "FIELD REC");
    setParam(*proc, params::st(4, "level"), -18.f);
    setParam(*proc, params::st(1, "src"), 1.f);               // станция 2 - вход дорожки
    setParam(*proc, params::st(2, "on"), 0.f);
    setParam(*proc, params::fading, 0.f);

    setParam(*proc, params::tune, 88.4f);
    const double tunedRms = runBlocks(*proc, 200, finite);
    std::printf("tuned rms %.3f, tuned station %d, SNR %.1f dB, stereo %.2f\n",
                tunedRms, proc->tunedStation(), proc->meters().snrDb, proc->meters().blend);
    CHECK(finite && tunedRms > 0.02, "station audible when tuned");
    CHECK(proc->tunedStation() == 0, "tuned station detected");

    proc->seek(+1);
    const float afterSeek = proc->state().getRawParameterValue(params::tune)->load();
    std::printf("seek +1 -> %.2f MHz\n", afterSeek);
    CHECK(std::fabs(afterSeek - 90.1f) < 0.01f, "seek finds the input station");

    // Состояние: сохранить, загрузить в новый экземпляр.
    setParam(*proc, params::tune, 98.6f);
    juce::MemoryBlock state;
    proc->getStateInformation(state);
    auto proc2 = std::make_unique<FmRxProcessor>();
    proc2->setPlayConfigDetails(2, 2, 48000.0, 512);
    proc2->prepareToPlay(48000.0, 512);
    proc2->setStateInformation(state.getData(), (int) state.getSize());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1500);
    CHECK(std::fabs(proc2->state().getRawParameterValue(params::tune)->load() - 98.6f) < 0.01f, "tune restored");
    CHECK(proc2->stations().meta(0).name == "NIGHT DRIVE" && proc2->stations().hasAudio(0), "station name and file restored");
    CHECK((int) proc2->state().getRawParameterValue(params::st(1, "src"))->load() == 1, "station source restored");

    // Снимок интерфейса: настроены на станцию 5, немного эфира.
    runBlocks(*proc2, 120, finite);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc2->createEditor());
        ed->setSize(1180, 780);
        for (int i = 0; i < 20; ++i)
        {
            runBlocks(*proc2, 4, finite);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        }
        const auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.5f);
        png.deleteFile();
        juce::FileOutputStream out(png);
        juce::PNGImageFormat().writeImageToStream(img, out);
        CHECK(out.getStatus().wasOk(), "UI snapshot written");
        ed->setSize(980, 690);                 // минимальный размер: раскладка не должна падать
        const auto small = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        CHECK(small.isValid(), "UI lays out at minimum size");
    }
    proc2->releaseResources();
    proc->releaseResources();

    std::printf("%s\n", g_failed == 0 ? "ALL OK" : "FAILURES");
    return g_failed == 0 ? 0 : 1;
}
