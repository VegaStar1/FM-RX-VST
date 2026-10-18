#pragma once
// ID и диапазоны параметров VST3. ID стабильны: их нельзя менять после выпуска (автоматизация, пресеты).
#include "Engine/FmBand.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace fmrx::params
{
    // приёмник
    inline constexpr const char* tune     = "tune";
    inline constexpr const char* afc      = "afc";
    inline constexpr const char* ifBw     = "ifbw";
    inline constexpr const char* demod    = "demod";
    inline constexpr const char* deemph   = "deemph";
    inline constexpr const char* stMode   = "stmode";
    inline constexpr const char* blend    = "blend";
    inline constexpr const char* softMute = "softmute";
    inline constexpr const char* hiCut    = "hicut";
    inline constexpr const char* speaker  = "speaker";
    inline constexpr const char* output   = "output";
    // эфир
    inline constexpr const char* noise     = "noise";
    inline constexpr const char* fading    = "fading";
    inline constexpr const char* fadeRate  = "faderate";
    inline constexpr const char* multipath = "multipath";
    inline constexpr const char* impulse   = "impulse";
    inline constexpr const char* drift     = "drift";
    inline constexpr const char* cleanZone = "cleanzone";
    inline constexpr const char* noiseZone = "noisezone";
    // управление
    inline constexpr const char* mute      = "mute";
    inline constexpr const char* clickVol  = "clickvol";
    inline constexpr const char* seekSpeed = "seekspeed";
    inline constexpr const char* seekSens  = "seeksens";
    // переход после 1-SHOT и автоматизация
    inline constexpr const char* hoDelay   = "handoverdelay";
    inline constexpr const char* hoNoise   = "handovernoise";
    inline constexpr const char* keepAuto  = "keepauto";
    // цвет шума
    inline constexpr const char* nTone     = "noisetone";
    inline constexpr const char* nFreq     = "noisefreq";
    inline constexpr const char* nReso     = "noisereso";

    enum class Source { File = 0, Input = 1 };
    enum class Mode { Loop = 0, OneShot = 1 };

    // Станции: s1_on, s1_freq, s1_level, s1_stereo, s1_src, s1_mode ... s8_*
    inline juce::String st(int slot, const char* name) { return "s" + juce::String(slot + 1) + "_" + name; }

    inline constexpr double kDefaultFreq[kNumStations] = { 88.4, 90.1, 92.7, 95.3, 98.6, 101.2, 104.0, 106.8 };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        auto pct = [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; };

        auto tuneRange = NormalisableRange<float>((float) dsp::kBandLowMHz, (float) dsp::kBandHighMHz, 0.01f);
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { tune, 1 }, "Tune", tuneRange, 98.6f,
            AudioParameterFloatAttributes().withLabel("MHz")
                .withStringFromValueFunction([](float v, int) { return String(v, 2) + " MHz"; })));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID { afc, 1 }, "AFC", true));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ifBw, 1 }, "IF Bandwidth",
            NormalisableRange<float>(40.f, 200.f, 1.f), 150.f,
            AudioParameterFloatAttributes().withLabel("kHz")
                .withStringFromValueFunction([](float v, int) { return String(juce::roundToInt(v)) + " kHz"; })));
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID { demod, 1 }, "Demodulator", StringArray { "QUAD", "PLL" }, 0));
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID { deemph, 1 }, "De-emphasis", StringArray { "50 us", "75 us", "Off" }, 0));
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID { stMode, 1 }, "Stereo", StringArray { "Auto", "Mono" }, 0));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { blend, 1 }, "Stereo Blend", NormalisableRange<float>(0.f, 1.f), 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { softMute, 1 }, "Soft Mute", NormalisableRange<float>(0.f, 1.f), 0.55f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { hiCut, 1 }, "Hi-Cut", NormalisableRange<float>(0.f, 1.f), 0.4f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { speaker, 1 }, "Speaker", NormalisableRange<float>(0.f, 1.f), 0.f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { output, 1 }, "Output", NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")
                .withStringFromValueFunction([](float v, int) { return String(v, 1) + " dB"; })));

        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { noise, 1 }, "Noise",
            NormalisableRange<float>(-100.f, -20.f, 0.5f), -60.f,
            AudioParameterFloatAttributes().withLabel("dB")
                .withStringFromValueFunction([](float v, int) { return String(v, 1) + " dB"; })));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { fading, 1 }, "Fading", NormalisableRange<float>(0.f, 1.f), 0.2f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { fadeRate, 1 }, "Fade Rate",
            NormalisableRange<float>(0.05f, 10.f, 0.01f, 0.4f), 1.f,
            AudioParameterFloatAttributes().withLabel("Hz")
                .withStringFromValueFunction([](float v, int) { return String(v, 2) + " Hz"; })));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { multipath, 1 }, "Multipath", NormalisableRange<float>(0.f, 1.f), 0.f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { impulse, 1 }, "Impulse", NormalisableRange<float>(0.f, 1.f), 0.f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { drift, 1 }, "Drift", NormalisableRange<float>(0.f, 1.f), 0.f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

        auto khz = [](float v, int) { return v >= 1000.f ? String(v / 1000.f, 2) + " MHz" : String(juce::roundToInt(v)) + " kHz"; };
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { cleanZone, 1 }, "Clean Zone",
            NormalisableRange<float>(0.f, 1500.f, 1.f, 0.4f), 60.f,
            AudioParameterFloatAttributes().withLabel("kHz").withStringFromValueFunction(khz)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { noiseZone, 1 }, "Noise Zone",
            NormalisableRange<float>(20.f, 3000.f, 1.f, 0.4f), 350.f,
            AudioParameterFloatAttributes().withLabel("kHz").withStringFromValueFunction(khz)));

        layout.add(std::make_unique<AudioParameterBool>(ParameterID { mute, 1 }, "Mute", false));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { clickVol, 1 }, "Button Clicks", NormalisableRange<float>(0.f, 1.f), 0.6f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { seekSpeed, 1 }, "Search Speed", NormalisableRange<float>(0.f, 1.f), 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { seekSens, 1 }, "Search Sensitivity", NormalisableRange<float>(0.f, 1.f), 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { hoDelay, 1 }, "Hand-over Delay",
            NormalisableRange<float>(0.f, 5.f, 0.01f, 0.5f), 0.45f,
            AudioParameterFloatAttributes().withLabel("s")
                .withStringFromValueFunction([](float v, int) { return String(v, 2) + " s"; })));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { hoNoise, 1 }, "Hand-over Noise", NormalisableRange<float>(0.f, 1.f), 0.6f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID { keepAuto, 1 }, "Keep Tune Automation", false));

        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { nTone, 1 }, "Noise Tone", NormalisableRange<float>(-1.f, 1.f), 0.f,
            AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int)
            {
                const int p = juce::roundToInt(v * 100.f);
                return p == 0 ? String("0") : (p > 0 ? "+" + String(p) : String(p));
            })));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { nFreq, 1 }, "Noise Frequency",
            NormalisableRange<float>(200.f, 10000.f, 1.f, 0.35f), 2500.f,
            AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction([](float v, int)
            {
                return v >= 1000.f ? String(v / 1000.f, 1) + " kHz" : String(juce::roundToInt(v)) + " Hz";
            })));
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { nReso, 1 }, "Noise Sharpness", NormalisableRange<float>(0.f, 1.f), 0.f,
            AudioParameterFloatAttributes().withStringFromValueFunction(pct)));

        for (int i = 0; i < kNumStations; ++i)
        {
            const String n = "Station " + String(i + 1) + " ";
            layout.add(std::make_unique<AudioParameterBool>(ParameterID { st(i, "on"), 1 }, n + "On", true));
            layout.add(std::make_unique<AudioParameterFloat>(ParameterID { st(i, "freq"), 1 }, n + "Frequency",
                NormalisableRange<float>((float) dsp::kBandLowMHz, (float) dsp::kBandHighMHz, 0.05f), (float) kDefaultFreq[i],
                AudioParameterFloatAttributes().withLabel("MHz")
                    .withStringFromValueFunction([](float v, int) { return String(v, 2) + " MHz"; })));
            layout.add(std::make_unique<AudioParameterFloat>(ParameterID { st(i, "level"), 1 }, n + "TX Level",
                NormalisableRange<float>(-60.f, 0.f, 0.5f), -6.f,
                AudioParameterFloatAttributes().withLabel("dB")
                    .withStringFromValueFunction([](float v, int) { return String(v, 1) + " dB"; })));
            layout.add(std::make_unique<AudioParameterBool>(ParameterID { st(i, "stereo"), 1 }, n + "Stereo", true));
            layout.add(std::make_unique<AudioParameterChoice>(ParameterID { st(i, "src"), 1 }, n + "Source",
                StringArray { "File", "Input" }, 0));
            layout.add(std::make_unique<AudioParameterChoice>(ParameterID { st(i, "mode"), 1 }, n + "Playback",
                StringArray { "Loop", "One-shot" }, 0));
        }
        return layout;
    }
}
