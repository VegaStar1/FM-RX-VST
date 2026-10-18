#pragma once
// Окно помощи поверх плагина: разделы слева, текст справа (с прокруткой), «Больше не показывать при запуске»,
// кнопка PDF-инструкции. Показывается при первом запуске и из настроек. Esc / × / клик мимо карточки - закрыть.
#include "AppSettings.h"
#include "HelpContent.h"
#include "Strings.h"
#include "Theme.h"

namespace fmrx::ui
{
    class HelpPanel : public juce::Component
    {
    public:
        explicit HelpPanel(AppSettings& s) : settings_(s)
        {
            setWantsKeyboardFocus(true);
            close_.setButtonText(juce::String::fromUTF8("\xc3\x97"));
            close_.onClick = [this] { dismiss(); };
            addAndMakeVisible(close_);

            for (int i = 0; i < help::kNumSections; ++i)
            {
                auto* b = tabs_.add(new juce::TextButton());
                b->setClickingTogglesState(false);
                b->onClick = [this, i] { showSection(i); };
                addAndMakeVisible(b);
            }

            viewport_.setViewedComponent(&body_, false);
            viewport_.setScrollBarsShown(true, false);
            viewport_.setScrollBarThickness(8);
            addAndMakeVisible(viewport_);

            dontShow_.onClick = [this] { settings_.setShowHelpOnStart(! dontShow_.getToggleState()); };
            addAndMakeVisible(dontShow_);

            pdf_.onClick = [this]
            {
                const auto f = AppSettings::findManual(settings_.language());
                if (f.existsAsFile()) f.startAsProcess();
            };
            prev_.onClick = [this] { showSection(section_ - 1); };
            next_.onClick = [this]
            {
                if (section_ + 1 < help::kNumSections) showSection(section_ + 1);
                else dismiss();
            };
            for (auto* b : { &pdf_, &prev_, &next_ }) addAndMakeVisible(*b);
            refresh();
        }

        std::function<void()> onClose;

        void open(int section = 0)
        {
            refresh();
            showSection(section);
            setVisible(true);
            toFront(true);
            grabKeyboardFocus();
        }

        void dismiss()
        {
            setVisible(false);
            if (onClose) onClose();
        }

        // Язык или скин сменились.
        void refresh()
        {
            for (int i = 0; i < tabs_.size(); ++i)
            {
                const auto& sct = help::kSections[i];
                tabs_[i]->setButtonText(juce::String(i + 1) + ".  " + juce::String::fromUTF8(lang() ? sct.titleRu : sct.titleEn));
            }
            dontShow_.setButtonText(tr("help_dont_show"));
            dontShow_.setToggleState(! settings_.showHelpOnStart(), juce::dontSendNotification);
            dontShow_.setColour(juce::ToggleButton::textColourId, col::dim);
            dontShow_.setColour(juce::ToggleButton::tickColourId, col::amber);
            dontShow_.setColour(juce::ToggleButton::tickDisabledColourId, col::faint);
            const bool hasPdf = AppSettings::findManual(settings_.language()).existsAsFile();
            pdf_.setButtonText(tr("help_pdf"));
            pdf_.setEnabled(hasPdf);
            pdf_.setTooltip(hasPdf ? tr("help_pdf_tip") : tr("help_pdf_none"));
            prev_.setButtonText(tr("help_prev"));
            close_.setTooltip(tr("close"));
            showSection(section_);
        }

        int section() const { return section_; }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black.withAlpha(0.6f));
            const auto c = card().toFloat();
            g.setColour(col::panel);
            g.fillRoundedRectangle(c, 12.f);
            g.setColour(col::line2);
            g.drawRoundedRectangle(c.reduced(0.5f), 12.f, 1.f);

            auto head = card().reduced(24, 18).removeFromTop(28);
            g.setFont(font(16.f, true));
            g.setColour(col::text);
            g.drawText(tr("help_title"), head, juce::Justification::centredLeft);
            g.setFont(font(11.f));
            g.setColour(col::faint);
            g.drawText("v" + juce::String(JucePlugin_VersionString) + juce::String::fromUTF8("  \xc2\xb7  NUWMUN"),
                       head.withTrimmedRight(46), juce::Justification::centredRight);

            g.setColour(col::line);
            g.drawVerticalLine(dividerX_, (float) contentTop_, (float) contentBottom_);
            g.drawHorizontalLine(contentBottom_ + 10, (float) card().getX() + 1.f, (float) card().getRight() - 1.f);
        }

        void resized() override
        {
            auto c = card();
            close_.setBounds(c.getRight() - 44, c.getY() + 14, 30, 30);
            auto r = c.reduced(24, 18);
            r.removeFromTop(40);

            auto footer = r.removeFromBottom(34);
            r.removeFromBottom(22);
            contentTop_ = r.getY();
            contentBottom_ = r.getBottom();

            auto left = r.removeFromLeft(juce::jmin(220, r.getWidth() / 4));
            dividerX_ = left.getRight() + 12;
            r.removeFromLeft(26);
            for (auto* b : tabs_) { b->setBounds(left.removeFromTop(34).reduced(0, 2)); left.removeFromTop(4); }
            viewport_.setBounds(r);
            layoutBody();

            next_.setBounds(footer.removeFromRight(120).reduced(0, 2));
            footer.removeFromRight(8);
            prev_.setBounds(footer.removeFromRight(110).reduced(0, 2));
            footer.removeFromRight(8);
            pdf_.setBounds(footer.removeFromRight(200).reduced(0, 2));
            dontShow_.setBounds(footer.reduced(0, 4));
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (! card().contains(e.getPosition())) dismiss();
        }

        bool keyPressed(const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::escapeKey) { dismiss(); return true; }
            if (k == juce::KeyPress::rightKey) { showSection(section_ + 1); return true; }
            if (k == juce::KeyPress::leftKey)  { showSection(section_ - 1); return true; }
            return false;
        }

    private:
        static bool lang() { return currentLang() == Lang::Ru; }

        // Текст раздела: подзаголовки, абзацы и пункты списка с висячим отступом.
        class Body : public juce::Component
        {
        public:
            void setText(const juce::String& t) { text_ = t; }
            void layout(int width)
            {
                blocks_.clear();
                const float w = (float) juce::jmax(100, width - 12);
                float y = 2.f;
                for (auto line : juce::StringArray::fromLines(text_))
                {
                    line = line.trim();
                    if (line.isEmpty()) continue;
                    Block b;
                    juce::AttributedString as;
                    as.setWordWrap(juce::AttributedString::byWord);
                    as.setLineSpacing(3.f);
                    if (line.startsWith("# "))
                    {
                        y += blocks_.empty() ? 0.f : 10.f;
                        as.append(line.substring(2).toUpperCase(), font(12.f, true), col::amber);
                        b.indent = 0.f;
                    }
                    else if (line.startsWith("- "))
                    {
                        as.append(line.substring(2), font(13.5f), col::text);
                        b.bullet = true;
                        b.indent = 18.f;
                    }
                    else
                    {
                        as.append(line, font(13.5f), col::text);
                        b.indent = 0.f;
                    }
                    b.layout.createLayout(as, w - b.indent);
                    b.y = y;
                    y += b.layout.getHeight() + (line.startsWith("# ") ? 6.f : 8.f);
                    blocks_.push_back(std::move(b));
                }
                setSize(width, (int) std::ceil(y + 6.f));
            }
            void paint(juce::Graphics& g) override
            {
                for (auto& b : blocks_)
                {
                    if (b.bullet)
                    {
                        g.setColour(col::amber);
                        g.fillEllipse(5.f, b.y + 7.f, 5.f, 5.f);
                    }
                    b.layout.draw(g, { b.indent, b.y, b.layout.getWidth(), b.layout.getHeight() });
                }
            }
        private:
            struct Block { juce::TextLayout layout; float y = 0.f, indent = 0.f; bool bullet = false; };
            juce::String text_;
            std::vector<Block> blocks_;
        };

        void showSection(int i)
        {
            section_ = juce::jlimit(0, help::kNumSections - 1, i);
            for (int k = 0; k < tabs_.size(); ++k)
                tabs_[k]->setToggleState(k == section_, juce::dontSendNotification);
            const auto& s = help::kSections[section_];
            body_.setText(juce::String::fromUTF8(lang() ? s.bodyRu : s.bodyEn));
            layoutBody();
            viewport_.setViewPosition(0, 0);
            prev_.setEnabled(section_ > 0);
            next_.setButtonText(section_ + 1 < help::kNumSections ? tr("help_next") : tr("help_done"));
            repaint();
        }

        void layoutBody()
        {
            if (viewport_.getWidth() > 0)
                body_.layout(viewport_.getWidth() - viewport_.getScrollBarThickness());
        }

        juce::Rectangle<int> card() const
        {
            return getLocalBounds().withSizeKeepingCentre(juce::jmin(900, getWidth() - 40), juce::jmin(600, getHeight() - 40));
        }

        AppSettings& settings_;
        juce::TextButton close_, pdf_, prev_, next_;
        juce::OwnedArray<juce::TextButton> tabs_;
        juce::ToggleButton dontShow_;
        juce::Viewport viewport_;
        Body body_;
        int section_ = 0, dividerX_ = 0, contentTop_ = 0, contentBottom_ = 0;
    };
}
