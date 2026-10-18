#include "PluginEditor.h"

namespace fmrx
{
    using namespace ui;

    namespace
    {
        constexpr int kBaseW = 1180, kBaseH = 780;

        void paintPanel(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& hint)
        {
            g.setColour(col::panel);
            g.fillRoundedRectangle(r.toFloat(), 10.f);
            g.setColour(col::line);
            g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 10.f, 1.f);
            auto head = r.reduced(14, 10).removeFromTop(16);
            g.setFont(font(11.f, true));
            g.setColour(col::text);
            g.drawText(title, head, juce::Justification::left);
            g.setFont(font(11.f));
            g.setColour(col::dim);
            g.drawText(hint, head, juce::Justification::right);
        }
    }

    FmRxEditor::FmRxEditor(FmRxProcessor& p)
        : AudioProcessorEditor(p), proc_(p), tuner_(p)
    {
        setLookAndFeel(&lnf_);
        auto& st = p.state();

        addAndMakeVisible(tuner_);

        for (int i = 0; i < kNumStations; ++i)
            addAndMakeVisible(rows_.add(new StationRow(p, i)));
        p.stations().onChange = [this] { for (auto* r : rows_) r->refresh(); };

        // Полоса настройки.
        seekDown_.setButtonText(juce::String::fromUTF8("|\xe2\x97\x80"));
        stepDown_.setButtonText(juce::String::fromUTF8("\xe2\x97\x80"));
        stepUp_.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6"));
        seekUp_.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6|"));
        seekDown_.setTooltip(juce::String::fromUTF8("Предыдущая станция"));
        seekUp_.setTooltip(juce::String::fromUTF8("Следующая станция"));
        stepDown_.setTooltip("-0.05 MHz");
        stepUp_.setTooltip("+0.05 MHz");
        seekDown_.onClick = [this] { proc_.seek(-1); };
        seekUp_.onClick = [this] { proc_.seek(+1); };
        auto step = [this](float d)
        {
            const float t = proc_.state().getRawParameterValue(params::tune)->load();
            proc_.tuneTo(juce::jlimit(87.5f, 108.f, std::round((t + d) * 20.f) / 20.f));
        };
        stepDown_.onClick = [step] { step(-0.05f); };
        stepUp_.onClick = [step] { step(0.05f); };
        for (auto* b : { &seekDown_, &stepDown_, &stepUp_, &seekUp_ }) addAndMakeVisible(*b);

        tuneSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
        tuneSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        tuneSlider_.setColour(juce::Slider::trackColourId, col::line2);
        tuneSlider_.setColour(juce::Slider::backgroundColourId, col::line);
        tuneSlider_.setTooltip(juce::String::fromUTF8("TUNE - автоматизируйте его, чтобы «крутить эфир» огибающей"));
        addAndMakeVisible(tuneSlider_);
        tuneAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(st, params::tune, tuneSlider_);

        // Эфир.
        addKnob(noise_, params::noise, "NOISE", col::cyan, juce::String::fromUTF8("Уровень шума эфира"));
        addKnob(fading_, params::fading, "FADING", col::cyan, juce::String::fromUTF8("Глубина рэлеевских замираний"));
        addKnob(fadeRate_, params::fadeRate, "FADE RATE", col::cyan, juce::String::fromUTF8("Скорость замираний"));
        addKnob(multipath_, params::multipath, "MULTIPATH", col::cyan, juce::String::fromUTF8("Многолучёвость: отражённый сигнал с задержкой"));
        addKnob(impulse_, params::impulse, "IMPULSE", col::cyan, juce::String::fromUTF8("Импульсные помехи (щелчки)"));
        addKnob(drift_, params::drift, "DRIFT", col::cyan, juce::String::fromUTF8("Медленный уход частоты гетеродина"));

        // Приёмник.
        addKnob(ifBw_, params::ifBw, "IF BW", col::amber, juce::String::fromUTF8("Ширина ПЧ-фильтра: уже - меньше соседей, но больше искажений"));
        addKnob(blend_, params::blend, "BLEND", col::amber, juce::String::fromUTF8("Как рано стерео уходит в моно на слабом сигнале"));
        addKnob(softMute_, params::softMute, "SOFT MUTE", col::amber, juce::String::fromUTF8("Приглушение шума между станциями и на слабом сигнале"));
        addKnob(hiCut_, params::hiCut, "HI-CUT", col::amber, juce::String::fromUTF8("Срез верхов на слабом сигнале"));
        addKnob(speaker_, params::speaker, "SPEAKER", col::amber, juce::String::fromUTF8("Окраска маленьким динамиком"));
        addKnob(output_, params::output, "OUTPUT", col::amber, juce::String::fromUTF8("Выходной уровень"));

        demod_.addItemList({ "QUAD", "PLL" }, 1);
        deemph_.addItemList({ juce::String::fromUTF8("50 \xc2\xb5s"), juce::String::fromUTF8("75 \xc2\xb5s"), "Off" }, 1);
        stMode_.addItemList({ "STEREO", "MONO" }, 1);
        demod_.setTooltip(juce::String::fromUTF8("Демодулятор: квадратурный или ФАПЧ - по-разному звучат на расстройке"));
        deemph_.setTooltip(juce::String::fromUTF8("Деэмфазис: 50 мкс (Европа) или 75 мкс (США)"));
        stMode_.setTooltip(juce::String::fromUTF8("Принудительное моно"));
        for (auto* c : { &demod_, &deemph_, &stMode_ }) addAndMakeVisible(*c);
        demodAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, params::demod, demod_);
        deemphAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, params::deemph, deemph_);
        stModeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, params::stMode, stMode_);

        afc_.setButtonText("AFC");
        afc_.setClickingTogglesState(true);
        afc_.setTooltip(juce::String::fromUTF8("Автоподстройка частоты: «дотягивает» на станцию"));
        addAndMakeVisible(afc_);
        afcAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(st, params::afc, afc_);

        setResizable(true, true);
        setResizeLimits(960, 680, 1800, 1200);
        setSize(kBaseW, kBaseH);
        startTimerHz(10);
    }

    FmRxEditor::~FmRxEditor()
    {
        proc_.stations().onChange = nullptr;
        setLookAndFeel(nullptr);
    }

    void FmRxEditor::addKnob(Knob& k, const char* paramId, const juce::String& label, juce::Colour colour, const juce::String& tip)
    {
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 16);
        k.slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
        k.slider.setColour(juce::Slider::textBoxTextColourId, col::dim);
        k.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setTooltip(tip);
        addAndMakeVisible(k.slider);
        k.name.setText(label, juce::dontSendNotification);
        k.name.setJustificationType(juce::Justification::centred);
        k.name.setFont(font(11.f, true));
        k.name.setColour(juce::Label::textColourId, col::text);
        k.name.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(k.name);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.state(), paramId, k.slider);
    }

    void FmRxEditor::layoutKnobs(std::initializer_list<Knob*> knobs, juce::Rectangle<int> area)
    {
        const int n = (int) knobs.size();
        const int w = area.getWidth() / n;
        const int h = juce::jmin(area.getHeight(), w + 34);      // ручка не больше ширины ячейки
        area = area.withSizeKeepingCentre(area.getWidth(), h);
        int i = 0;
        for (auto* k : knobs)
        {
            auto cell = juce::Rectangle<int>(area.getX() + i * w, area.getY(), w, area.getHeight()).reduced(2, 0);
            k->name.setBounds(cell.removeFromBottom(16));
            k->slider.setBounds(cell);
            ++i;
        }
    }

    void FmRxEditor::timerCallback()
    {
        const int tuned = proc_.tunedStation();
        for (int i = 0; i < rows_.size(); ++i) rows_[i]->setTuned(i == tuned);
    }

    void FmRxEditor::paint(juce::Graphics& g)
    {
        g.fillAll(col::bg);
        auto top = getLocalBounds().reduced(20, 16).removeFromTop(30);
        g.setFont(font(22.f, true));
        g.setColour(col::text);
        g.drawText(juce::String::fromUTF8("FM\xc2\xb7RX"), top.removeFromLeft(90), juce::Justification::centredLeft);
        g.setFont(font(12.f));
        g.setColour(col::dim);
        g.drawText(juce::String::fromUTF8("8-STATION BAND RECEIVER  \xc2\xb7  by NUWMUN"), top, juce::Justification::centredLeft);

        paintPanel(g, stationsArea_, "STATIONS", juce::String::fromUTF8("перетащите файлы на строки"));
        paintPanel(g, airArea_, juce::String::fromUTF8("AIR \xc2\xb7 CHANNEL"), juce::String::fromUTF8("что происходит в эфире"));
        paintPanel(g, rxArea_, "RECEIVER", {});
    }

    void FmRxEditor::resized()
    {
        auto r = getLocalBounds().reduced(20, 16);
        auto header = r.removeFromTop(30);
        afc_.setBounds(header.removeFromRight(64).reduced(0, 2));
        header.removeFromRight(8);
        stMode_.setBounds(header.removeFromRight(96).reduced(0, 2));
        r.removeFromTop(12);

        auto tunerArea = r.removeFromTop(juce::jmax(220, r.getHeight() * 32 / 100));
        auto tuneStrip = tunerArea.removeFromBottom(34);
        tuner_.setBounds(tunerArea.withTrimmedBottom(6));
        auto ts = tuneStrip.withTrimmedLeft(juce::jmin(280, tunerArea.getWidth() * 28 / 100) + 12);
        seekDown_.setBounds(ts.removeFromLeft(42).reduced(2));
        stepDown_.setBounds(ts.removeFromLeft(42).reduced(2));
        seekUp_.setBounds(ts.removeFromRight(42).reduced(2));
        stepUp_.setBounds(ts.removeFromRight(42).reduced(2));
        tuneSlider_.setBounds(ts.reduced(8, 4));
        r.removeFromTop(14);

        stationsArea_ = r.removeFromLeft(r.getWidth() * 56 / 100);
        r.removeFromLeft(14);
        airArea_ = r.removeFromTop(r.getHeight() * 42 / 100);
        r.removeFromTop(14);
        rxArea_ = r;

        auto sa = stationsArea_.reduced(10, 10).withTrimmedTop(24);
        const int rowH = sa.getHeight() / kNumStations;
        for (int i = 0; i < rows_.size(); ++i)
            rows_[i]->setBounds(sa.getX(), sa.getY() + i * rowH, sa.getWidth(), rowH);

        layoutKnobs({ &noise_, &fading_, &fadeRate_, &multipath_, &impulse_, &drift_ }, airArea_.reduced(12, 10).withTrimmedTop(26));

        auto rx = rxArea_.reduced(12, 10);
        auto rxHead = rx.removeFromTop(20);
        rxHead.removeFromLeft(90);
        deemph_.setBounds(rxHead.removeFromRight(84));
        rxHead.removeFromRight(8);
        demod_.setBounds(rxHead.removeFromRight(80));
        rx.removeFromTop(10);
        layoutKnobs({ &ifBw_, &blend_, &softMute_, &hiCut_, &speaker_, &output_ }, rx);
    }
}
