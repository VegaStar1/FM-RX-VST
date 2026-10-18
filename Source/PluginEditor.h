#pragma once
#include "PluginProcessor.h"
#include "UI/StationRow.h"
#include "UI/Theme.h"
#include "UI/TunerDisplay.h"

namespace fmrx
{
    class FmRxEditor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit FmRxEditor(FmRxProcessor&);
        ~FmRxEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        struct Knob
        {
            juce::Slider slider;
            juce::Label name;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        };

        void timerCallback() override;
        void addKnob(Knob& k, const char* paramId, const juce::String& label, juce::Colour colour, const juce::String& tip);
        void layoutKnobs(std::initializer_list<Knob*> knobs, juce::Rectangle<int> area);

        FmRxProcessor& proc_;
        ui::FmLookAndFeel lnf_;
        ui::TunerDisplay tuner_;
        juce::OwnedArray<ui::StationRow> rows_;

        juce::TextButton seekDown_, stepDown_, stepUp_, seekUp_;
        juce::Slider tuneSlider_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneAtt_;

        Knob noise_, fading_, fadeRate_, multipath_, impulse_, drift_;
        Knob ifBw_, blend_, softMute_, hiCut_, speaker_, output_;
        juce::ComboBox demod_, deemph_, stMode_;
        juce::TextButton afc_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> demodAtt_, deemphAtt_, stModeAtt_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> afcAtt_;
        juce::TooltipWindow tooltips_ { this, 600 };

        juce::Rectangle<int> stationsArea_, airArea_, rxArea_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FmRxEditor)
    };
}
