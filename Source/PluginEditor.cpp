#include "PluginEditor.h"

namespace fmrx
{
    using namespace ui;

    namespace
    {
        constexpr int kBaseW = 1180, kBaseH = 830, kMinW = 980, kMinH = 730;

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
        : AudioProcessorEditor(p), proc_(p), tuner_(p), stationEditor_(p), settings_(p, *appSettings_), help_(*appSettings_)
    {
        setLookAndFeel(&lnf_);
        auto& st = p.state();

        addAndMakeVisible(tuner_);
        for (int i = 0; i < kNumStations; ++i)
        {
            auto* row = rows_.add(new StationRow(p, i));
            row->onOpenEditor = [this](int slot) { openStationEditor(slot); };
            addAndMakeVisible(row);
        }
        addChildComponent(stationEditor_);
        stationEditor_.onClose = [this] { closeStationEditor(); };
        p.stations().onChange = [this]
        {
            for (auto* r : rows_) r->refresh();
            if (stationEditor_.isVisible()) stationEditor_.refreshTexts();
        };

        // --- шапка: пресеты, стерео, AFC, настройки ---
        presetPrev_.setButtonText(juce::String::fromUTF8("\xe2\x97\x80"));
        presetNext_.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6"));
        presetPrev_.onClick = [this] { proc_.presets().step(-1); };
        presetNext_.onClick = [this] { proc_.presets().step(+1); };
        presetName_.onClick = [this] { showPresetMenu(); };
        for (auto* b : { &presetPrev_, &presetName_, &presetNext_ }) addAndMakeVisible(*b);

        stMode_.addItemList({ "STEREO", "MONO" }, 1);
        addAndMakeVisible(stMode_);
        stModeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, params::stMode, stMode_);

        afc_.setButtonText("AFC");
        afc_.setClickingTogglesState(true);
        addAndMakeVisible(afc_);
        afcAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(st, params::afc, afc_);

        gear_.setButtonText(juce::String::fromUTF8("\xe2\x9a\x99"));
        gear_.onClick = [this] { openSettings(! settings_.isVisible()); };
        addAndMakeVisible(gear_);

        // --- полоса управления ---
        searchDn_.onClick = [this] { proc_.sendCommand(RadioCmd::SearchDown); };
        searchUp_.onClick = [this] { proc_.sendCommand(RadioCmd::SearchUp); };
        scan_.onClick     = [this] { proc_.sendCommand(RadioCmd::Scan); };
        stop_.onClick     = [this] { onStop(); };
        lock_.onClick     = [this] { proc_.sendCommand(proc_.control().locked() ? RadioCmd::Unlock : RadioCmd::Lock); };
        auto step = [this](float d)
        {
            proc_.tuneTo(juce::jlimit(87.5f, 108.f, std::round((proc_.displayFreq() + d) * 20.f) / 20.f));
        };
        stepDown_.setButtonText(juce::String::fromUTF8("\xe2\x97\x80"));
        stepUp_.setButtonText(juce::String::fromUTF8("\xe2\x96\xb6"));
        stepDown_.onClick = [step] { step(-0.05f); };
        stepUp_.onClick = [step] { step(0.05f); };
        mute_.setClickingTogglesState(true);
        muteAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(st, params::mute, mute_);
        for (auto* b : { &searchDn_, &stepDown_, &stepUp_, &searchUp_, &scan_, &stop_, &lock_, &mute_ }) addAndMakeVisible(*b);

        tuneSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
        tuneSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(tuneSlider_);
        tuneAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(st, params::tune, tuneSlider_);

        // --- строка «ПОИСК» ---
        auto rowSlider = [this, &st](RowSlider& r, const char* id, const char* key, const char* tip)
        {
            r.key = key; r.tip = tip;
            r.slider.setSliderStyle(juce::Slider::LinearHorizontal);
            r.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
            addAndMakeVisible(r.slider);
            r.name.setFont(font(11.f, true));
            r.name.setJustificationType(juce::Justification::centredRight);
            r.name.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(r.name);
            r.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(st, id, r.slider);
        };
        rowSlider(seekSpeed_, params::seekSpeed, "row_speed", "tip_speed");
        rowSlider(seekSens_, params::seekSens, "row_sens", "tip_sens");
        rowSlider(clickVol_, params::clickVol, "row_clicks", "tip_clicks");

        // --- эфир (две строки) и приёмник ---
        addKnob(noise_, params::noise, "k_noise", "t_noise", true);
        addKnob(fading_, params::fading, "k_fading", "t_fading", true);
        addKnob(fadeRate_, params::fadeRate, "k_faderate", "t_faderate", true);
        addKnob(multipath_, params::multipath, "k_multipath", "t_multipath", true);
        addKnob(impulse_, params::impulse, "k_impulse", "t_impulse", true);
        addKnob(drift_, params::drift, "k_drift", "t_drift", true);
        addKnob(clean_, params::cleanZone, "k_clean", "t_clean", true);
        addKnob(noiseZone_, params::noiseZone, "k_noisezone", "t_noisezone", true);
        addKnob(ifBw_, params::ifBw, "k_ifbw", "t_ifbw", false);
        addKnob(blend_, params::blend, "k_blend", "t_blend", false);
        addKnob(softMute_, params::softMute, "k_softmute", "t_softmute", false);
        addKnob(hiCut_, params::hiCut, "k_hicut", "t_hicut", false);
        addKnob(speaker_, params::speaker, "k_speaker", "t_speaker", false);
        addKnob(output_, params::output, "k_output", "t_output", false);

        demod_.addItemList({ "QUAD", "PLL" }, 1);
        deemph_.addItemList({ juce::String::fromUTF8("50 \xc2\xb5s"), juce::String::fromUTF8("75 \xc2\xb5s"), "Off" }, 1);
        for (auto* c : { &demod_, &deemph_ }) addAndMakeVisible(*c);
        demodAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, params::demod, demod_);
        deemphAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, params::deemph, deemph_);

        addChildComponent(settings_);
        addChildComponent(help_);
        settings_.onHelp = [this] { openHelp(); };
        appSettings_->addChangeListener(this);

        applyTheme();
        applyTexts();

        setResizable(true, true);
        setResizeLimits(kMinW, kMinH, 1800, 1250);
        const int w = juce::jlimit(kMinW, 1800, (int) st.state.getProperty("uiW", kBaseW));
        const int h = juce::jlimit(kMinH, 1250, (int) st.state.getProperty("uiH", kBaseH));
        setSize(w, h);
        startTimerHz(10);

        // Первый запуск: помощь, пока не отмечено «Больше не показывать» (не чаще раза за запуск DAW).
        if (appSettings_->showHelpOnStart() && ! ui::AppSettings::helpShownThisSession())
        {
            ui::AppSettings::helpShownThisSession() = true;
            juce::Component::SafePointer<FmRxEditor> safe(this);
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->openHelp(); });
        }
    }

    FmRxEditor::~FmRxEditor()
    {
        appSettings_->removeChangeListener(this);
        proc_.stations().onChange = nullptr;
        setLookAndFeel(nullptr);
    }

    void FmRxEditor::openStationEditor(int slot)
    {
        for (auto* r : rows_) r->setVisible(false);
        stationEditor_.open(slot);
        repaint();
    }

    void FmRxEditor::closeStationEditor()
    {
        stationEditor_.close();
        for (auto* r : rows_) r->setVisible(true);
        repaint();
    }

    std::array<FmRxEditor::Knob*, 14> FmRxEditor::allKnobs()
    {
        return { &noise_, &fading_, &fadeRate_, &multipath_, &impulse_, &drift_, &clean_, &noiseZone_,
                 &ifBw_, &blend_, &softMute_, &hiCut_, &speaker_, &output_ };
    }

    void FmRxEditor::addKnob(Knob& k, const char* paramId, const char* labelKey, const char* tipKey, bool air)
    {
        k.labelKey = labelKey; k.tipKey = tipKey; k.air = air;
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 16);
        addAndMakeVisible(k.slider);
        k.name.setJustificationType(juce::Justification::centred);
        k.name.setFont(font(11.f, true));
        k.name.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(k.name);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.state(), paramId, k.slider);
    }

    void FmRxEditor::applyTheme()
    {
        lnf_.applyTheme();
        for (auto* k : allKnobs())
        {
            k->slider.setColour(juce::Slider::rotarySliderFillColourId, k->air ? col::cyan : col::amber);
            k->slider.setColour(juce::Slider::textBoxTextColourId, col::dim);
            k->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            k->name.setColour(juce::Label::textColourId, col::text);
        }
        tuneSlider_.setColour(juce::Slider::trackColourId, col::line2);
        tuneSlider_.setColour(juce::Slider::backgroundColourId, col::line);
        tuneSlider_.setColour(juce::Slider::thumbColourId, col::amber);
        for (auto* r : { &seekSpeed_, &seekSens_, &clickVol_ })
        {
            r->slider.setColour(juce::Slider::trackColourId, col::amber);
            r->slider.setColour(juce::Slider::backgroundColourId, col::line);
            r->slider.setColour(juce::Slider::thumbColourId, col::amber);
            r->slider.setColour(juce::Slider::textBoxTextColourId, col::dim);
            r->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            r->name.setColour(juce::Label::textColourId, col::text);
        }
        for (auto* r : rows_) r->applyTheme();
        stationEditor_.applyTheme();
        settings_.refresh();
        help_.refresh();
        sendLookAndFeelChange();
        repaint();
    }

    void FmRxEditor::applyTexts()
    {
        searchDn_.setButtonText(juce::String::fromUTF8("\xe2\x97\x80 ") + tr("search"));
        searchUp_.setButtonText(tr("search") + juce::String::fromUTF8(" \xe2\x96\xb6"));
        scan_.setButtonText(tr("scan"));
        stop_.setButtonText(tr("stop"));
        lock_.setButtonText(tr("lock"));
        mute_.setButtonText(tr("mute"));
        searchDn_.setTooltip(tr("tip_search_dn"));
        searchUp_.setTooltip(tr("tip_search_up"));
        scan_.setTooltip(tr("tip_scan"));
        stop_.setTooltip(tr("tip_stop"));
        lock_.setTooltip(tr("tip_lock"));
        mute_.setTooltip(tr("tip_mute"));
        stepDown_.setTooltip(tr("tip_step_dn"));
        stepUp_.setTooltip(tr("tip_step_up"));
        tuneSlider_.setTooltip(tr("tip_tune"));
        afc_.setTooltip(tr("tip_afc"));
        stMode_.setTooltip(tr("tip_stmode"));
        gear_.setTooltip(tr("tip_gear"));
        presetName_.setTooltip(tr("preset"));
        demod_.setTooltip(tr("t_demod"));
        deemph_.setTooltip(tr("t_deemph"));
        for (auto* r : { &seekSpeed_, &seekSens_, &clickVol_ })
        {
            r->name.setText(tr(r->key), juce::dontSendNotification);
            r->slider.setTooltip(tr(r->tip));
        }
        for (auto* k : allKnobs())
        {
            k->name.setText(tr(k->labelKey), juce::dontSendNotification);
            k->slider.setTooltip(tr(k->tipKey));
        }
        for (auto* r : rows_) r->applyTexts();
        stationEditor_.applyTexts();
        settings_.refresh();
        help_.refresh();
        timerCallback();
        repaint();
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

    // Двойной STOP (в пределах 500 мс) = REWIND: все станции с начала, настройка на 87.5 МГц.
    void FmRxEditor::onStop()
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (lastStopMs_ != 0 && now - lastStopMs_ <= 500)
        {
            proc_.sendCommand(RadioCmd::Rewind);
            lastStopMs_ = 0;
        }
        else
        {
            proc_.sendCommand(RadioCmd::Stop);
            lastStopMs_ = now == 0 ? 1 : now;
        }
    }

    void FmRxEditor::showPresetMenu()
    {
        auto& pm = proc_.presets();
        pm.refresh();
        const auto& e = pm.entries();
        const int cur = pm.currentIndex();
        juce::PopupMenu menu;
        menu.addSectionHeader(tr("preset_factory"));
        for (int i = 0; i < (int) e.size(); ++i)
            if (e[(size_t) i].factory) menu.addItem(1 + i, e[(size_t) i].name, true, i == cur);
        bool header = false;
        for (int i = 0; i < (int) e.size(); ++i)
            if (! e[(size_t) i].factory)
            {
                if (! header) { menu.addSectionHeader(tr("preset_user")); header = true; }
                menu.addItem(1 + i, e[(size_t) i].name, true, i == cur);
            }
        menu.addSeparator();
        menu.addItem(1000, tr("preset_save"));
        menu.addItem(1001, tr("preset_delete"), cur >= 0 && ! e[(size_t) cur].factory);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetName_),
                           [safe = juce::Component::SafePointer<FmRxEditor>(this), cur](int r)
                           {
                               if (safe == nullptr || r == 0) return;
                               auto& pm2 = safe->proc_.presets();
                               if (r == 1000) safe->savePresetDialog();
                               else if (r == 1001) pm2.deleteUser(cur);
                               else pm2.load(r - 1);
                               safe->timerCallback();
                           });
    }

    void FmRxEditor::savePresetDialog()
    {
        saveDialog_ = std::make_unique<juce::AlertWindow>(tr("preset_save"), tr("preset_name"), juce::MessageBoxIconType::NoIcon, this);
        saveDialog_->addTextEditor("name", proc_.presets().currentName(), {});
        saveDialog_->addButton(tr("save"), 1, juce::KeyPress(juce::KeyPress::returnKey));
        saveDialog_->addButton(tr("cancel"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
        saveDialog_->enterModalState(true, juce::ModalCallbackFunction::create(
            [safe = juce::Component::SafePointer<FmRxEditor>(this)](int result)
            {
                if (safe == nullptr || safe->saveDialog_ == nullptr) return;
                if (result == 1) safe->proc_.presets().saveUser(safe->saveDialog_->getTextEditorContents("name"));
                safe->saveDialog_.reset();
                safe->timerCallback();
            }), false);
    }

    void FmRxEditor::timerCallback()
    {
        const auto& ctl = proc_.control();
        const int tuned = proc_.tunedStation();
        for (int i = 0; i < rows_.size(); ++i)
        {
            rows_[i]->setTuned(i == tuned);
            const auto bit = 1u << i;
            rows_[i]->setPlayState((ctl.doneMask() & bit) ? 2 : ((ctl.parkedMask() & bit) ? 1 : 0));
        }
        lock_.setToggleState(ctl.locked(), juce::dontSendNotification);
        using S = RadioControl::State;
        const int s = ctl.state();
        scan_.setToggleState(s == (int) S::Scanning || s == (int) S::Dwell, juce::dontSendNotification);
        const bool searching = s == (int) S::Searching || s == (int) S::Settling || ctl.endSearchActive();
        searchUp_.setToggleState(searching, juce::dontSendNotification);
        searchDn_.setToggleState(false, juce::dontSendNotification);

        auto name = proc_.presets().currentName();
        if (name.isEmpty()) name = tr("preset_none");
        else if (proc_.presets().isModified()) name += " *";
        presetName_.setButtonText(name);
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
        g.drawText(tr("subtitle") + juce::String::fromUTF8("  \xc2\xb7  by NUWMUN"), top.removeFromLeft(330), juce::Justification::centredLeft);

        g.setColour(col::panel);
        g.fillRoundedRectangle(searchRow_.toFloat(), 8.f);
        g.setColour(col::line);
        g.drawRoundedRectangle(searchRow_.toFloat().reduced(0.5f), 8.f, 1.f);
        g.setFont(font(11.f, true));
        g.setColour(col::dim);
        g.drawText(tr("row_search"), searchRow_.withTrimmedLeft(14).withWidth(80), juce::Justification::centredLeft);

        paintPanel(g, stationsArea_, tr("stations"), stationEditor_.isVisible() ? juce::String() : tr("stations_hint"));
        paintPanel(g, airArea_, tr("air"), tr("air_hint"));
        paintPanel(g, rxArea_, tr("receiver"), {});
    }

    void FmRxEditor::resized()
    {
        proc_.state().state.setProperty("uiW", getWidth(), nullptr);
        proc_.state().state.setProperty("uiH", getHeight(), nullptr);
        settings_.setBounds(getLocalBounds());
        help_.setBounds(getLocalBounds());

        auto r = getLocalBounds().reduced(20, 16);
        auto header = r.removeFromTop(30);
        gear_.setBounds(header.removeFromRight(36).reduced(0, 1));
        header.removeFromRight(8);
        afc_.setBounds(header.removeFromRight(60).reduced(0, 2));
        header.removeFromRight(8);
        stMode_.setBounds(header.removeFromRight(96).reduced(0, 2));
        header.removeFromRight(16);
        presetNext_.setBounds(header.removeFromRight(30).reduced(0, 2));
        presetName_.setBounds(header.removeFromRight(juce::jlimit(150, 240, header.getWidth() - 440)).reduced(2, 2));
        presetPrev_.setBounds(header.removeFromRight(30).reduced(0, 2));
        r.removeFromTop(12);

        auto tunerArea = r.removeFromTop(juce::jmax(210, r.getHeight() * 30 / 100));
        tuner_.setBounds(tunerArea);
        r.removeFromTop(8);
        auto strip = r.removeFromTop(34);
        mute_.setBounds(strip.removeFromRight(74).reduced(2));
        strip.removeFromRight(8);
        lock_.setBounds(strip.removeFromRight(64).reduced(2));
        stop_.setBounds(strip.removeFromRight(70).reduced(2));
        scan_.setBounds(strip.removeFromRight(70).reduced(2));
        strip.removeFromRight(8);
        searchDn_.setBounds(strip.removeFromLeft(104).reduced(2));
        stepDown_.setBounds(strip.removeFromLeft(38).reduced(2));
        searchUp_.setBounds(strip.removeFromRight(104).reduced(2));
        stepUp_.setBounds(strip.removeFromRight(38).reduced(2));
        tuneSlider_.setBounds(strip.reduced(8, 4));
        r.removeFromTop(8);

        searchRow_ = r.removeFromTop(30);
        {
            auto row = searchRow_.reduced(10, 3).withTrimmedLeft(84);
            const int cell = row.getWidth() / 3;
            const int nameW[3] = { 80, 130, 120 };
            int i = 0;
            for (auto* rs : { &seekSpeed_, &seekSens_, &clickVol_ })
            {
                auto c = row.removeFromLeft(cell);
                rs->name.setBounds(c.removeFromLeft(nameW[i++]));
                c.removeFromLeft(8);
                rs->slider.setBounds(c.withTrimmedRight(16));
            }
        }
        r.removeFromTop(10);

        stationsArea_ = r.removeFromLeft(r.getWidth() * 57 / 100);
        r.removeFromLeft(14);
        airArea_ = r.removeFromTop(r.getHeight() * 54 / 100);
        r.removeFromTop(14);
        rxArea_ = r;

        auto sa = stationsArea_.reduced(10, 10).withTrimmedTop(24);
        const int rowH = sa.getHeight() / kNumStations;
        for (int i = 0; i < rows_.size(); ++i)
            rows_[i]->setBounds(sa.getX(), sa.getY() + i * rowH, sa.getWidth(), rowH);
        stationEditor_.setBounds(stationsArea_.reduced(14, 10).withTrimmedTop(26));

        auto air = airArea_.reduced(12, 10).withTrimmedTop(24);
        auto airTop = air.removeFromTop(air.getHeight() / 2);
        layoutKnobs({ &noise_, &fading_, &fadeRate_, &multipath_ }, airTop);
        layoutKnobs({ &impulse_, &drift_, &clean_, &noiseZone_ }, air);

        auto rx = rxArea_.reduced(12, 10);
        auto rxHead = rx.removeFromTop(20);
        rxHead.removeFromLeft(100);
        deemph_.setBounds(rxHead.removeFromRight(84));
        rxHead.removeFromRight(8);
        demod_.setBounds(rxHead.removeFromRight(80));
        rx.removeFromTop(8);
        layoutKnobs({ &ifBw_, &blend_, &softMute_, &hiCut_, &speaker_, &output_ }, rx);
    }
}
