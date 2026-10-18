#pragma once
#include "PluginProcessor.h"
#include "Strings.h"
#include "Theme.h"

namespace fmrx::ui
{
    // Строка станции: номер (настроиться), имя RDS + статус, источник, файл, режим LOOP / 1-SHOT,
    // частота, мощность, стерео, вкл.
    class StationRow : public juce::Component, public juce::FileDragAndDropTarget
    {
    public:
        using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
        using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
        using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

        StationRow(FmRxProcessor& p, int slot) : proc_(p), slot_(slot)
        {
            auto& st = p.state();
            num_.setButtonText(juce::String(slot + 1));
            num_.onClick = [this] { proc_.tuneTo(proc_.stationFreq(slot_)); };
            addAndMakeVisible(num_);

            name_.setEditable(false, true, false);
            name_.setFont(font(13.f, true));
            name_.onTextChange = [this] { proc_.stations().setName(slot_, name_.getText().toUpperCase().substring(0, 16)); };
            addAndMakeVisible(name_);

            detail_.setFont(font(11.f));
            detail_.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(detail_);

            // Источник: кнопка-переключатель ФАЙЛ / ВХОД поверх параметра-выбора.
            if (auto* prm = st.getParameter(params::st(slot, "src")))
                srcAtt_ = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { input_ = v > 0.5f; applySrcLook(); refresh(); });
            src_.onClick = [this] { if (srcAtt_) srcAtt_->setValueAsCompleteGesture(input_ ? 0.f : 1.f); };
            addAndMakeVisible(src_);
            if (srcAtt_) srcAtt_->sendInitialUpdate();

            load_.setButtonText("LOAD");
            load_.onClick = [this] { chooseFile(); };
            addAndMakeVisible(load_);

            clear_.setButtonText(juce::String::fromUTF8("\xc3\x97"));
            clear_.onClick = [this] { proc_.stations().clearSlot(slot_); };
            addAndMakeVisible(clear_);

            // Режим воспроизведения: кнопка-переключатель поверх параметра-выбора.
            if (auto* prm = st.getParameter(params::st(slot, "mode")))
                modeAtt_ = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { setModeLook(v > 0.5f); });
            mode_.onClick = [this] { if (modeAtt_) modeAtt_->setValueAsCompleteGesture(oneShot_ ? 0.f : 1.f); };
            addAndMakeVisible(mode_);
            if (modeAtt_) modeAtt_->sendInitialUpdate();

            for (auto* s : { &freq_, &level_ })
            {
                s->setSliderStyle(juce::Slider::LinearBar);
                s->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 0, 0);
                addAndMakeVisible(*s);
            }
            freqAtt_ = std::make_unique<SliderAtt>(st, params::st(slot, "freq"), freq_);
            levelAtt_ = std::make_unique<SliderAtt>(st, params::st(slot, "level"), level_);

            for (auto* b : { &stereo_, &on_ }) { b->setClickingTogglesState(true); addAndMakeVisible(*b); }
            stereo_.setButtonText("ST");
            on_.setButtonText("ON");
            stereoAtt_ = std::make_unique<ButtonAtt>(st, params::st(slot, "stereo"), stereo_);
            onAtt_ = std::make_unique<ButtonAtt>(st, params::st(slot, "on"), on_);
            applyTheme();
            applyTexts();
        }

        void setTuned(bool t) { if (t != tuned_) { tuned_ = t; repaint(); } }
        // 0 играет, 1 стоп, 2 доиграла. Заодно ловим смену источника из автоматизации/состояния.
        void setPlayState(int s) { if (s != playState_) { playState_ = s; refresh(); } }

        void applyTheme()
        {
            for (auto* s : { &freq_, &level_ })
            {
                s->setColour(juce::Slider::trackColourId, col::cyan.withAlpha(0.25f));
                s->setColour(juce::Slider::textBoxTextColourId, col::text);
                s->setColour(juce::Slider::textBoxOutlineColourId, col::line2);
            }
            name_.setColour(juce::Label::textColourId, col::text);
            refresh();
        }

        void applyTexts()
        {
            num_.setTooltip(tr("st_tune"));
            name_.setTooltip(tr("st_rename"));
            src_.setTooltip(tr("st_src"));
            applySrcLook();
            load_.setTooltip(tr("st_load"));
            clear_.setTooltip(tr("st_clear"));
            mode_.setTooltip(tr("st_mode"));
            freq_.setTooltip(tr("st_freq"));
            level_.setTooltip(tr("st_level"));
            stereo_.setTooltip(tr("st_stereo"));
            on_.setTooltip(tr("st_on"));
            refresh();
        }

        void refresh()
        {
            const auto& m = proc_.stations().meta(slot_);
            const bool input = input_;
            name_.setText(m.name.isNotEmpty() ? m.name : (input ? "INPUT" : tr("st_empty")), juce::dontSendNotification);
            juce::String d;
            if (input)             d = tr("st_input");
            else if (m.loading)    d = tr("st_loading");
            else if (m.missing)    d = tr("st_missing") + m.file.getFileName();
            else if (m.loadFailed) d = tr("st_failed") + m.file.getFileName();
            else if (m.loaded)
            {
                d = m.file.getFileName() + "  " + juce::String(m.durationSec, 1) + " s";
                if (playState_ == 1) d = juce::String::fromUTF8("\xe2\x96\xa0 ") + tr("st_parked") + "  " + d;
                if (playState_ == 2) d = juce::String::fromUTF8("\xe2\x9c\x93 ") + tr("st_done") + "  " + d;
            }
            else d = tr("st_drop");
            detail_.setText(d, juce::dontSendNotification);
            detail_.setColour(juce::Label::textColourId, (m.missing || m.loadFailed) ? col::warn : col::dim);
            load_.setEnabled(! input);
            mode_.setEnabled(! input);
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(0.5f);
            if (tuned_)
            {
                g.setColour(col::rowHot);
                g.fillRoundedRectangle(r, 7.f);
                g.setColour(col::rowHotEdge);
                g.drawRoundedRectangle(r, 7.f, 1.f);
            }
            if (dragOver_)
            {
                g.setColour(col::cyan);
                g.drawRoundedRectangle(r, 7.f, 1.5f);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(6, 4);
            const int h = r.getHeight();
            num_.setBounds(r.removeFromLeft(h).reduced(1));
            r.removeFromLeft(8);
            on_.setBounds(r.removeFromRight(36).reduced(2, 3));
            r.removeFromRight(4);
            stereo_.setBounds(r.removeFromRight(36).reduced(2, 3));
            r.removeFromRight(6);
            level_.setBounds(r.removeFromRight(juce::jmax(64, r.getWidth() / 8)).reduced(0, 6));
            r.removeFromRight(6);
            freq_.setBounds(r.removeFromRight(juce::jmax(80, r.getWidth() / 6)).reduced(0, 6));
            r.removeFromRight(6);
            mode_.setBounds(r.removeFromRight(58).reduced(1, 5));
            r.removeFromRight(4);
            clear_.setBounds(r.removeFromRight(24).reduced(1, 5));
            r.removeFromRight(2);
            load_.setBounds(r.removeFromRight(48).reduced(1, 5));
            r.removeFromRight(4);
            src_.setBounds(r.removeFromRight(64).reduced(1, 5));
            r.removeFromRight(8);
            name_.setBounds(r.removeFromTop(h / 2 + 2));
            detail_.setBounds(r);
        }

        // --- перетаскивание файла ---
        bool isInterestedInFileDrag(const juce::StringArray& files) override
        {
            for (auto& f : files)
                if (juce::File(f).hasFileExtension("wav;aif;aiff;mp3;flac;ogg")) return true;
            return false;
        }
        void fileDragEnter(const juce::StringArray&, int, int) override { dragOver_ = true; repaint(); }
        void fileDragExit(const juce::StringArray&) override { dragOver_ = false; repaint(); }
        void filesDropped(const juce::StringArray& files, int, int) override
        {
            dragOver_ = false;
            for (auto& f : files)
                if (juce::File(f).hasFileExtension("wav;aif;aiff;mp3;flac;ogg")) { loadFile(juce::File(f)); break; }
            repaint();
        }

    private:
        void applySrcLook()
        {
            src_.setButtonText(input_ ? tr("src_input").toUpperCase() : tr("src_file").toUpperCase());
            src_.setToggleState(input_, juce::dontSendNotification);
        }

        void setModeLook(bool oneShot)
        {
            oneShot_ = oneShot;
            mode_.setButtonText(oneShot ? "1-SHOT" : "LOOP");
            mode_.setToggleState(oneShot, juce::dontSendNotification);
        }

        void chooseFile()
        {
            chooser_ = std::make_unique<juce::FileChooser>(tr("st_file_title") + juce::String(slot_ + 1),
                                                           juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
            chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safe = juce::Component::SafePointer<StationRow>(this)](const juce::FileChooser& fc)
                                  {
                                      if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadFile(fc.getResult());
                                  });
        }

        void loadFile(const juce::File& f)
        {
            if (input_ && srcAtt_) srcAtt_->setValueAsCompleteGesture(0.f);   // файл -> источник «файл»
            proc_.stations().meta(slot_).name = {};          // имя возьмётся из файла
            proc_.stations().loadFileAsync(slot_, f);
        }

        FmRxProcessor& proc_;
        const int slot_;
        bool tuned_ = false, dragOver_ = false, oneShot_ = false;
        int playState_ = 0;
        bool input_ = false;
        juce::TextButton num_, src_, load_, clear_, mode_, stereo_, on_;
        juce::Label name_, detail_;
        juce::Slider freq_, level_;
        std::unique_ptr<SliderAtt> freqAtt_, levelAtt_;
        std::unique_ptr<ButtonAtt> stereoAtt_, onAtt_;
        std::unique_ptr<juce::ParameterAttachment> srcAtt_, modeAtt_;
        std::unique_ptr<juce::FileChooser> chooser_;
    };
}
