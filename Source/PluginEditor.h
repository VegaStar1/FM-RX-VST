#pragma once
#include "PluginProcessor.h"
#include "UI/AppSettings.h"
#include "UI/HelpPanel.h"
#include "UI/SettingsPanel.h"
#include "UI/StationEditor.h"
#include "UI/StationRow.h"
#include "UI/Strings.h"
#include "UI/Theme.h"
#include "UI/TunerDisplay.h"

namespace fmrx
{
    class FmRxEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener
    {
    public:
        explicit FmRxEditor(FmRxProcessor&);
        ~FmRxEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

        // Для консольной проверки (снимки UI).
        void openSettings(bool open) { settings_.setVisible(open); if (open) settings_.toFront(false); }
        void openHelp(int section = 0) { settings_.setVisible(false); help_.open(section); }
        bool helpVisible() const { return help_.isVisible(); }
        void closeHelp() { help_.dismiss(); }
        void openStationEditor(int slot);
        void closeStationEditor();

    private:
        struct Knob
        {
            juce::Slider slider;
            juce::Label name;
            const char* labelKey = "";
            const char* tipKey = "";
            bool air = true;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        };

        void timerCallback() override;
        void changeListenerCallback(juce::ChangeBroadcaster*) override { applyTheme(); applyTexts(); }
        void addKnob(Knob& k, const char* paramId, const char* labelKey, const char* tipKey, bool air);
        void layoutKnobs(std::initializer_list<Knob*> knobs, juce::Rectangle<int> area);
        void applyTheme();
        void applyTexts();
        void showPresetMenu();
        void savePresetDialog();
        void onStop();
        std::array<Knob*, 14> allKnobs();

        FmRxProcessor& proc_;
        juce::SharedResourcePointer<ui::AppSettings> appSettings_;
        ui::FmLookAndFeel lnf_;
        ui::TunerDisplay tuner_;
        juce::OwnedArray<ui::StationRow> rows_;
        ui::StationEditor stationEditor_;

        // шапка
        juce::TextButton presetPrev_, presetName_, presetNext_, afc_, gear_;
        juce::ComboBox stMode_;
        // полоса управления
        juce::TextButton searchDn_, stepDown_, stepUp_, searchUp_, scan_, stop_, lock_, mute_;
        juce::Slider tuneSlider_;
        // строка «ПОИСК»: скорость, чувствительность, щелчки
        struct RowSlider { juce::Slider slider; juce::Label name; const char* key = ""; const char* tip = "";
                           std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att; };
        RowSlider seekSpeed_, seekSens_, clickVol_;
        // строка «ПЕРЕХОД» (после 1-SHOT): пауза, шум перехода, защита автоматизации
        RowSlider hoDelay_, hoNoise_;
        juce::TextButton keepAuto_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keepAutoAtt_;
        // строка «ЦВЕТ ШУМА»: тон, частота, резкость
        RowSlider nTone_, nFreq_, nReso_;
        juce::Rectangle<int> searchRow_, handoverRow_, colourRow_;
        std::array<RowSlider*, 8> rowSliders() { return { &seekSpeed_, &seekSens_, &clickVol_, &hoDelay_, &hoNoise_, &nTone_, &nFreq_, &nReso_ }; }
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneAtt_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> afcAtt_, muteAtt_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> demodAtt_, deemphAtt_, stModeAtt_;

        Knob noise_, fading_, fadeRate_, multipath_, impulse_, drift_, clean_, noiseZone_;
        Knob ifBw_, blend_, softMute_, hiCut_, speaker_, output_;
        juce::ComboBox demod_, deemph_;
        juce::TooltipWindow tooltips_ { this, 600 };
        ui::SettingsPanel settings_;
        ui::HelpPanel help_;
        std::unique_ptr<juce::AlertWindow> saveDialog_;

        juce::Rectangle<int> stationsArea_, airArea_, rxArea_;
        juce::uint32 lastStopMs_ = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FmRxEditor)
    };
}
