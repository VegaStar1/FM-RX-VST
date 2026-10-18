#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

namespace fmrx::ui
{
    // Строка станции: номер (настроиться), имя RDS, источник, частота, мощность, стерео, вкл, файл.
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
            num_.setTooltip(juce::String::fromUTF8("Настроиться на станцию"));
            num_.onClick = [this] { proc_.tuneTo(proc_.stationFreq(slot_)); };
            addAndMakeVisible(num_);

            name_.setEditable(false, true, false);
            name_.setFont(font(13.f, true));
            name_.setColour(juce::Label::textColourId, col::text);
            name_.setTooltip(juce::String::fromUTF8("Двойной клик - переименовать (RDS)"));
            name_.onTextChange = [this] { proc_.stations().setName(slot_, name_.getText().toUpperCase().substring(0, 16)); };
            addAndMakeVisible(name_);

            detail_.setFont(font(11.f));
            detail_.setColour(juce::Label::textColourId, col::dim);
            detail_.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(detail_);

            src_.addItemList({ "File", "Input" }, 1);
            src_.setTooltip(juce::String::fromUTF8("Источник звука станции: файл или вход дорожки"));
            addAndMakeVisible(src_);
            srcAtt_ = std::make_unique<ComboAtt>(st, params::st(slot, "src"), src_);
            src_.onChange = [this] { refresh(); };

            load_.setButtonText("LOAD");
            load_.setTooltip(juce::String::fromUTF8("Выбрать аудиофайл (WAV, AIFF, MP3, FLAC, OGG) - или перетащите его на строку"));
            load_.onClick = [this] { chooseFile(); };
            addAndMakeVisible(load_);

            clear_.setButtonText(juce::String::fromUTF8("\xc3\x97"));
            clear_.setTooltip(juce::String::fromUTF8("Очистить слот"));
            clear_.onClick = [this] { proc_.stations().clearSlot(slot_); };
            addAndMakeVisible(clear_);

            for (auto* s : { &freq_, &level_ })
            {
                s->setSliderStyle(juce::Slider::LinearBar);
                s->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 0, 0);
                s->setColour(juce::Slider::trackColourId, col::cyan.withAlpha(0.25f));
                s->setColour(juce::Slider::textBoxTextColourId, col::text);
                s->setColour(juce::Slider::textBoxOutlineColourId, col::line2);
                addAndMakeVisible(*s);
            }
            freq_.setTooltip(juce::String::fromUTF8("Частота станции (тащите; Shift - точнее; двойной клик - ввести)"));
            level_.setTooltip(juce::String::fromUTF8("Мощность передатчика: слабая станция шумит, но не тише (это FM)"));
            freqAtt_ = std::make_unique<SliderAtt>(st, params::st(slot, "freq"), freq_);
            levelAtt_ = std::make_unique<SliderAtt>(st, params::st(slot, "level"), level_);

            for (auto* b : { &stereo_, &on_ }) { b->setClickingTogglesState(true); addAndMakeVisible(*b); }
            stereo_.setButtonText("ST");
            stereo_.setTooltip(juce::String::fromUTF8("Стерео-вещание (пилот 19 кГц)"));
            on_.setButtonText("ON");
            on_.setTooltip(juce::String::fromUTF8("Станция в эфире"));
            stereoAtt_ = std::make_unique<ButtonAtt>(st, params::st(slot, "stereo"), stereo_);
            onAtt_ = std::make_unique<ButtonAtt>(st, params::st(slot, "on"), on_);
            on_.onStateChange = [this] { repaint(); };
            refresh();
        }

        void setTuned(bool t) { if (t != tuned_) { tuned_ = t; repaint(); } }

        void refresh()
        {
            const auto& m = proc_.stations().meta(slot_);
            const bool input = src_.getSelectedItemIndex() == 1;
            name_.setText(m.name.isNotEmpty() ? m.name : (input ? "INPUT" : juce::String::fromUTF8("\xe2\x80\x94 empty \xe2\x80\x94")),
                          juce::dontSendNotification);
            juce::String d;
            if (input)             d = juce::String::fromUTF8("Вход дорожки");
            else if (m.loading)    d = juce::String::fromUTF8("загрузка\xe2\x80\xa6");
            else if (m.missing)    d = juce::String::fromUTF8("файл не найден: ") + m.file.getFileName();
            else if (m.loadFailed) d = juce::String::fromUTF8("не удалось прочитать: ") + m.file.getFileName();
            else if (m.loaded)     d = m.file.getFileName() + "  " + juce::String(m.durationSec, 1) + " s";
            else                   d = juce::String::fromUTF8("перетащите файл или LOAD");
            detail_.setText(d, juce::dontSendNotification);
            detail_.setColour(juce::Label::textColourId, (m.missing || m.loadFailed) ? col::warn : col::dim);
            load_.setEnabled(! input);
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(0.5f);
            if (tuned_)
            {
                g.setColour(juce::Colour(0xff24211a));
                g.fillRoundedRectangle(r, 7.f);
                g.setColour(juce::Colour(0xff5a4826));
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
            level_.setBounds(r.removeFromRight(juce::jmax(70, r.getWidth() / 7)).reduced(0, 6));
            r.removeFromRight(6);
            freq_.setBounds(r.removeFromRight(juce::jmax(84, r.getWidth() / 6)).reduced(0, 6));
            r.removeFromRight(6);
            clear_.setBounds(r.removeFromRight(26).reduced(1, 5));
            r.removeFromRight(2);
            load_.setBounds(r.removeFromRight(50).reduced(1, 5));
            r.removeFromRight(4);
            src_.setBounds(r.removeFromRight(72).reduced(0, 6));
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
        void chooseFile()
        {
            chooser_ = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("Файл для станции ") + juce::String(slot_ + 1),
                                                           juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
            chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safe = juce::Component::SafePointer<StationRow>(this)](const juce::FileChooser& fc)
                                  {
                                      if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadFile(fc.getResult());
                                  });
        }

        void loadFile(const juce::File& f)
        {
            if (src_.getSelectedItemIndex() != 0) src_.setSelectedItemIndex(0, juce::sendNotification);
            proc_.stations().meta(slot_).name = {};          // имя возьмётся из файла
            proc_.stations().loadFileAsync(slot_, f);
        }

        FmRxProcessor& proc_;
        const int slot_;
        bool tuned_ = false, dragOver_ = false;
        juce::TextButton num_, load_, clear_, stereo_, on_;
        juce::Label name_, detail_;
        juce::ComboBox src_;
        juce::Slider freq_, level_;
        std::unique_ptr<SliderAtt> freqAtt_, levelAtt_;
        std::unique_ptr<ButtonAtt> stereoAtt_, onAtt_;
        std::unique_ptr<ComboAtt> srcAtt_;
        std::unique_ptr<juce::FileChooser> chooser_;
    };
}
