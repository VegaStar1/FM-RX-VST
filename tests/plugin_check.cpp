// Консольная проверка плагина без DAW: звук, SEARCH, one-shot с автопереходом, MUTE и щелчки, пресеты,
// сохранение/восстановление, снимки интерфейса (скины, языки, окно настроек).
// Сборка: -DFMRX_BUILD_CHECK=ON, запуск: FmRxCheck <station.wav> <out_dir>
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

// Блоки аудио + «таймер» процессора (запись результата поиска в параметр) через цикл сообщений.
static double runBlocks(FmRxProcessor& p, int blocks, bool& finite, float* peak = nullptr)
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
                if (! std::isfinite(v) || std::fabs(v) > 1.6f) finite = false;
                if (peak) *peak = std::max(*peak, std::fabs(v));
                sum += (double) v * v; ++cnt;
            }
        if (b % 4 == 3) juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
    }
    return std::sqrt(sum / (double) std::max(1LL, cnt));
}

static void snapshot(juce::AudioProcessorEditor& ed, const juce::File& file)
{
    const auto img = ed.createComponentSnapshot(ed.getLocalBounds(), true, 1.5f);
    file.deleteFile();
    juce::FileOutputStream out(file);
    juce::PNGImageFormat().writeImageToStream(img, out);
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File wav(argc > 1 ? juce::String(argv[1]) : juce::String());
    const juce::File outDir(argc > 2 ? juce::String(argv[2]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    auto pump = [](int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); };

    auto proc = std::make_unique<FmRxProcessor>();
    proc->setPlayConfigDetails(2, 2, 48000.0, 512);
    proc->prepareToPlay(48000.0, 512);
    std::printf("latency %d samples\n", proc->getLatencySamples());

    bool finite = true;
    const double silentRms = runBlocks(*proc, 50, finite);
    CHECK(finite && silentRms < 0.5, "empty band: finite output");

    proc->stations().loadFileAsync(0, wav);
    proc->stations().loadFileAsync(4, wav);
    pump(1500);
    CHECK(proc->stations().hasAudio(0), "file station loaded");
    proc->stations().setName(0, "NIGHT DRIVE");
    proc->stations().setName(4, "FIELD REC");
    setParam(*proc, params::st(4, "level"), -18.f);
    setParam(*proc, params::st(1, "src"), 1.f);               // станция 2 - вход дорожки
    setParam(*proc, params::st(2, "on"), 0.f);
    setParam(*proc, params::fading, 0.f);

    setParam(*proc, params::tune, 88.4f);
    const double tunedRms = runBlocks(*proc, 200, finite);
    std::printf("tuned rms %.3f, tuned station %d, SNR %.1f dB\n", tunedRms, proc->tunedStation(), proc->meters().snrDb);
    CHECK(finite && tunedRms > 0.02, "station audible when tuned");
    CHECK(proc->tunedStation() == 0, "tuned station detected");

    // SEARCH: от 88.4 вверх -> станция 2 (вход, 90.1).
    proc->sendCommand(RadioCmd::SearchUp);
    runBlocks(*proc, 400, finite);
    pump(200);
    runBlocks(*proc, 20, finite);
    const float afterSearch = proc->param(params::tune);
    std::printf("search up -> %.2f MHz (knob), display %.2f\n", afterSearch, proc->displayFreq());
    CHECK(std::fabs(afterSearch - 90.1f) < 0.02f, "SEARCH finds the next station and writes the knob");

    // MUTE: тишина + щелчок.
    setParam(*proc, params::tune, 88.4f);
    runBlocks(*proc, 60, finite);
    setParam(*proc, params::mute, 1.f);
    float clickPeak = 0.f;
    runBlocks(*proc, 10, finite, &clickPeak);
    const double mutedRms = runBlocks(*proc, 40, finite);
    std::printf("mute: click peak %.2f, muted rms %.5f\n", clickPeak, mutedRms);
    CHECK(clickPeak > 0.05f, "MUTE makes a click");
    CHECK(mutedRms < 1e-4, "MUTE silences the receiver");
    setParam(*proc, params::mute, 0.f);
    runBlocks(*proc, 20, finite);

    // One-shot: станция 1 доигрывает и приёмник сам уходит к следующей.
    setParam(*proc, params::st(0, "mode"), 1.f);
    setParam(*proc, params::tune, 88.4f);
    runBlocks(*proc, 40, finite);
    runBlocks(*proc, (int) (34.0 * 48000 / 512), finite);    // файл ~32 с
    for (int k = 0; k < 30; ++k) { runBlocks(*proc, 20, finite); pump(10); }
    std::printf("one-shot end -> %.2f MHz, done mask %u\n", proc->param(params::tune), proc->control().doneMask());
    CHECK(std::fabs(proc->param(params::tune) - 90.1f) < 0.02f, "finished one-shot hands over to the next station");

    // Пресеты.
    auto& pm = proc->presets();
    CHECK(pm.entries().size() >= 12, "factory presets present");
    CHECK(pm.load(4), "load preset");
    CHECK(std::fabs(proc->param(params::noise) - preset::kFactory[4].v[0]) < 0.6f && pm.currentName() == preset::kFactory[4].name, "preset applied");
    CHECK(! pm.isModified(), "freshly loaded preset is not modified");
    setParam(*proc, params::noise, -20.f);
    CHECK(pm.isModified(), "changed parameter marks preset modified");
    pm.step(+1);
    CHECK(pm.currentName() == preset::kFactory[5].name, "next preset");

    // Состояние: сохранить, загрузить в новый экземпляр.
    setParam(*proc, params::tune, 98.6f);
    juce::MemoryBlock state;
    proc->getStateInformation(state);
    auto proc2 = std::make_unique<FmRxProcessor>();
    proc2->setPlayConfigDetails(2, 2, 48000.0, 512);
    proc2->prepareToPlay(48000.0, 512);
    proc2->setStateInformation(state.getData(), (int) state.getSize());
    pump(1500);
    CHECK(std::fabs(proc2->param(params::tune) - 98.6f) < 0.01f, "tune restored");
    CHECK(proc2->stations().meta(0).name == "NIGHT DRIVE" && proc2->stations().hasAudio(0), "station name and file restored");
    CHECK((int) proc2->param(params::st(0, "mode").toRawUTF8()) == 1, "station mode restored");
    CHECK(proc2->presets().currentName() == preset::kFactory[5].name, "preset name restored");

    // Снимки интерфейса.
    setParam(*proc2, params::st(0, "mode"), 0.f);
    setParam(*proc2, params::fading, 0.2f);
    runBlocks(*proc2, 120, finite);
    {
        juce::SharedResourcePointer<ui::AppSettings> app;
        app->setLanguage(ui::Lang::Ru);
        app->setSkin(0);
        auto* raw = proc2->createEditor();
        std::unique_ptr<FmRxEditor> ed(dynamic_cast<FmRxEditor*>(raw));
        ed->setSize(1180, 800);
        for (int i = 0; i < 20; ++i) { runBlocks(*proc2, 4, finite); pump(30); }
        snapshot(*ed, outDir.getChildFile("ui_ru_graphite.png"));
        app->setLanguage(ui::Lang::En);
        for (int s = 1; s < ui::kNumSkins; ++s)
        {
            app->setSkin(s);
            pump(60);
            snapshot(*ed, outDir.getChildFile(juce::String("ui_en_") + ui::kSkins[s].id + ".png"));
        }
        app->setSkin(0);
        app->setLanguage(ui::Lang::Ru);
        ed->openSettings(true);
        pump(60);
        snapshot(*ed, outDir.getChildFile("ui_settings_ru.png"));
        CHECK(outDir.getChildFile("ui_settings_ru.png").getSize() > 1000, "UI snapshots written");
        ed->openSettings(false);
        ed->setSize(980, 700);
        const auto small = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        CHECK(small.isValid(), "UI lays out at minimum size");
    }
    proc2->releaseResources();
    proc->releaseResources();

    std::printf("%s\n", g_failed == 0 ? "ALL OK" : "FAILURES");
    return g_failed == 0 ? 0 : 1;
}
