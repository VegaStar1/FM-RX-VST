#pragma once
#include "Engine/FmBand.h"
#include "Parameters.h"
#include "Stations/LoopPlayer.h"
#include "Stations/StationManager.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace fmrx
{
    class FmRxProcessor : public juce::AudioProcessor, private juce::Timer
    {
    public:
        FmRxProcessor();
        ~FmRxProcessor() override;

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported(const BusesLayout&) const override;
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using AudioProcessor::processBlock;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return JucePlugin_Name; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}

        void getStateInformation(juce::MemoryBlock&) override;
        void setStateInformation(const void*, int) override;

        // --- для UI (message thread) ---
        juce::AudioProcessorValueTreeState& state() { return apvts_; }
        StationManager& stations() { return stations_; }
        BandMeters meters() const { return band_.meters(); }
        float stationFreq(int slot) const;
        bool  stationAudible(int slot) const;      // включена и есть звук (файл или вход)
        int   tunedStation() const;                // станция под стрелкой (-1 - нет)
        void  seek(int direction);                 // к соседней станции
        void  tuneTo(float mhz);

    private:
        void timerCallback() override { stations_.reclaim(); }
        void readParams(BandParams& band, std::array<StationParams, kNumStations>& st, std::array<int, kNumStations>& src) const;

        juce::AudioProcessorValueTreeState apvts_;
        StationManager stations_;
        FmBand band_;
        std::array<LoopPlayer, kNumStations> players_;

        // Кэш указателей на параметры (чтение в аудиопотоке без поиска по строкам).
        struct StationParamPtrs { std::atomic<float>* on; std::atomic<float>* freq; std::atomic<float>* level; std::atomic<float>* stereo; std::atomic<float>* src; };
        std::array<StationParamPtrs, kNumStations> sp_ {};
        std::atomic<float>* pTune_ = nullptr, * pAfc_ = nullptr, * pIfBw_ = nullptr, * pDemod_ = nullptr, * pDeemph_ = nullptr,
                          * pStMode_ = nullptr, * pBlend_ = nullptr, * pSoftMute_ = nullptr, * pHiCut_ = nullptr, * pSpeaker_ = nullptr,
                          * pOutput_ = nullptr, * pNoise_ = nullptr, * pFading_ = nullptr, * pFadeRate_ = nullptr, * pMultipath_ = nullptr,
                          * pImpulse_ = nullptr, * pDrift_ = nullptr;

        // Рабочие буферы (выделяются в prepareToPlay).
        int scratchSize_ = 0;
        std::array<std::array<std::vector<float>, 2>, kNumStations> stScratch_;
        std::array<std::vector<float>, 2> inCopy_, out_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FmRxProcessor)
    };
}
