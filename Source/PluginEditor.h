#pragma once
#include "PluginProcessor.h"
#include "UI/AppSettings.h"
#include "UI/SettingsPanel.h"
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

        // шапка
        juce::TextButton presetPrev_, presetName_, presetNext_, afc_, gear_;
        juce::ComboBox stMode_;
        // полоса управления
        juce::TextButton searchDn_, stepDown_, stepUp_, searchUp_, scan_, stop_, lock_, mute_;
        juce::Slider tuneSlider_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneAtt_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> afcAtt_, muteAtt_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> demodAtt_, deemphAtt_, stModeAtt_;

        Knob noise_, fading_, fadeRate_, multipath_, impulse_, drift_, clean_, noiseZone_;
        Knob ifBw_, blend_, softMute_, hiCut_, speaker_, output_;
        juce::ComboBox demod_, deemph_;
        juce::TooltipWindow tooltips_ { this, 600 };
        ui::SettingsPanel settings_;
        std::unique_ptr<juce::AlertWindow> saveDialog_;

        juce::Rectangle<int> stationsArea_, airArea_, rxArea_;
        juce::uint32 lastStopMs_ = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FmRxEditor)
    };
}
