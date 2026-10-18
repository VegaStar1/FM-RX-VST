#pragma once
#include "AppSettings.h"
#include "PluginProcessor.h"
#include "Strings.h"
#include "Theme.h"

namespace fmrx::ui
{
    // Окно настроек поверх плагина: только язык и скин (глобально, для всех проектов).
    // Клик мимо карточки или × закрывает.
    class SettingsPanel : public juce::Component
    {
    public:
        SettingsPanel(FmRxProcessor&, AppSettings& s) : settings_(s)
        {
            close_.setButtonText(juce::String::fromUTF8("\xc3\x97"));
            close_.onClick = [this] { setVisible(false); };
            addAndMakeVisible(close_);

            en_.setButtonText("English");
            ru_.setButtonText(juce::String::fromUTF8("Русский"));
            en_.onClick = [this] { settings_.setLanguage(Lang::En); };
            ru_.onClick = [this] { settings_.setLanguage(Lang::Ru); };
            addAndMakeVisible(en_);
            addAndMakeVisible(ru_);

            for (int i = 0; i < kNumSkins; ++i)
            {
                auto* b = skins_.add(new SkinButton(i));
                b->onClick = [this, i] { settings_.setSkin(i); };
                addAndMakeVisible(b);
            }

            refresh();
        }

        void refresh()
        {
            en_.setToggleState(settings_.language() == Lang::En, juce::dontSendNotification);
            ru_.setToggleState(settings_.language() == Lang::Ru, juce::dontSendNotification);
            for (int i = 0; i < skins_.size(); ++i) skins_[i]->setToggleState(i == settings_.skin(), juce::dontSendNotification);
            close_.setTooltip(tr("close"));
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.55f));
            const auto c = card().toFloat();
            g.setColour(col::panel);
            g.fillRoundedRectangle(c, 12.f);
            g.setColour(col::line2);
            g.drawRoundedRectangle(c.reduced(0.5f), 12.f, 1.f);

            auto r = card().reduced(24, 18);
            g.setFont(font(15.f, true));
            g.setColour(col::text);
            g.drawText(tr("set_title"), r.removeFromTop(24), juce::Justification::left);

            g.setFont(font(12.f, true));
            g.setColour(col::dim);
            for (auto& row : labelRows_)
                g.drawText(tr(row.key), row.area, juce::Justification::centredLeft);

            g.setFont(font(11.f));
            g.setColour(col::faint);
            g.drawText(tr("set_note"), noteArea_, juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto c = card();
            close_.setBounds(c.getRight() - 44, c.getY() + 12, 30, 30);
            auto r = c.reduced(24, 18);
            r.removeFromTop(36);
            labelRows_.clear();
            const int labelW = 190;

            auto row = [&](const char* key, int h) {
                auto a = r.removeFromTop(h);
                labelRows_.push_back({ key, a.removeFromLeft(labelW) });
                r.removeFromTop(12);
                return a;
            };

            auto lang = row("set_lang", 34);
            en_.setBounds(lang.removeFromLeft(130).reduced(0, 2));
            lang.removeFromLeft(8);
            ru_.setBounds(lang.removeFromLeft(130).reduced(0, 2));

            auto skin = row("set_skin", 64);
            const int sw = juce::jmin(110, (skin.getWidth() - 8 * (kNumSkins - 1)) / kNumSkins);
            for (auto* b : skins_) { b->setBounds(skin.removeFromLeft(sw)); skin.removeFromLeft(8); }

            noteArea_ = r.removeFromTop(20);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (! card().contains(e.getPosition())) setVisible(false);
        }

    private:
        struct LabelRow { const char* key; juce::Rectangle<int> area; };

        // Кнопка скина: образец цветов + название на текущем языке.
        class SkinButton : public juce::Button
        {
        public:
            explicit SkinButton(int i) : juce::Button("skin"), index_(i) { setClickingTogglesState(false); }
            void paintButton(juce::Graphics& g, bool over, bool) override
            {
                const auto& k = kSkins[index_];
                auto r = getLocalBounds().toFloat().reduced(1.f);
                g.setColour(juce::Colour(k.bg));
                g.fillRoundedRectangle(r, 8.f);
                auto sw = r.reduced(8.f).removeFromTop(r.getHeight() * 0.45f);
                const float w = sw.getWidth() / 3.f;
                g.setColour(juce::Colour(k.lcd));     g.fillRoundedRectangle(sw.removeFromLeft(w).reduced(2.f), 3.f);
                g.setColour(juce::Colour(k.accent));  g.fillRoundedRectangle(sw.removeFromLeft(w).reduced(2.f), 3.f);
                g.setColour(juce::Colour(k.accent2)); g.fillRoundedRectangle(sw.reduced(2.f), 3.f);
                g.setColour(juce::Colour(k.text));
                g.setFont(font(10.5f, true));
                g.drawFittedText(juce::String::fromUTF8(currentLang() == Lang::Ru ? k.nameRu : k.nameEn),
                                 getLocalBounds().reduced(6, 4).removeFromBottom(18), juce::Justification::centred, 1);
                const bool sel = getToggleState();
                g.setColour(sel ? juce::Colour(k.accent) : (over ? col::line2 : col::line));
                g.drawRoundedRectangle(r, 8.f, sel ? 2.f : 1.f);
            }
        private:
            int index_;
        };

        juce::Rectangle<int> card() const
        {
            return getLocalBounds().withSizeKeepingCentre(juce::jmin(720, getWidth() - 40), juce::jmin(220, getHeight() - 40));
        }

        AppSettings& settings_;
        juce::TextButton close_, en_, ru_;
        juce::OwnedArray<SkinButton> skins_;
        std::vector<LabelRow> labelRows_;
        juce::Rectangle<int> noteArea_;
    };
}
