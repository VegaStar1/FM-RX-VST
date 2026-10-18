#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Палитра из макета FM·RX: графит, тёплый off-white, янтарь (приёмник) и бирюза (эфир/станции).
namespace fmrx::ui::col
{
    inline const juce::Colour bg      { 0xff121315 };
    inline const juce::Colour panel   { 0xff1a1c1f };
    inline const juce::Colour raised  { 0xff222428 };
    inline const juce::Colour line    { 0xff2a2d31 };
    inline const juce::Colour line2   { 0xff34383d };
    inline const juce::Colour text    { 0xffe8e4da };
    inline const juce::Colour dim     { 0xff8b8f96 };
    inline const juce::Colour faint   { 0xff5d6168 };
    inline const juce::Colour amber   { 0xfff2a541 };
    inline const juce::Colour amberDk { 0xff4a402b };
    inline const juce::Colour lcd     { 0xff17150f };
    inline const juce::Colour lcdLine { 0xff3a3220 };
    inline const juce::Colour lcdText { 0xfff2d39a };
    inline const juce::Colour cyan    { 0xff5bc8d6 };
    inline const juce::Colour band    { 0xff0f1012 };
    inline const juce::Colour warn    { 0xffd9635a };
}

namespace fmrx::ui
{
    inline juce::Font font(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
    }
    inline juce::Font mono(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height,
                                            bold ? juce::Font::bold : juce::Font::plain));
    }

    class FmLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        FmLookAndFeel()
        {
            using namespace col;
            setColour(juce::ResizableWindow::backgroundColourId, bg);
            setColour(juce::Label::textColourId, text);
            setColour(juce::Slider::thumbColourId, amber);
            setColour(juce::Slider::trackColourId, amber);
            setColour(juce::Slider::backgroundColourId, line);
            setColour(juce::Slider::textBoxTextColourId, text);
            setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour(juce::TextButton::buttonColourId, raised);
            setColour(juce::TextButton::buttonOnColourId, amber);
            setColour(juce::TextButton::textColourOffId, text);
            setColour(juce::TextButton::textColourOnId, lcd);
            setColour(juce::ComboBox::backgroundColourId, raised);
            setColour(juce::ComboBox::textColourId, text);
            setColour(juce::ComboBox::outlineColourId, line2);
            setColour(juce::ComboBox::arrowColourId, dim);
            setColour(juce::PopupMenu::backgroundColourId, panel);
            setColour(juce::PopupMenu::textColourId, text);
            setColour(juce::PopupMenu::highlightedBackgroundColourId, amber);
            setColour(juce::PopupMenu::highlightedTextColourId, lcd);
            setColour(juce::TextEditor::backgroundColourId, band);
            setColour(juce::TextEditor::textColourId, lcdText);
            setColour(juce::TextEditor::outlineColourId, line2);
            setColour(juce::TextEditor::focusedOutlineColourId, amber);
            setColour(juce::TooltipWindow::backgroundColourId, panel);
            setColour(juce::TooltipWindow::textColourId, text);
            setColour(juce::TooltipWindow::outlineColourId, line2);
        }

        // Ручка: дуга значения (цвет задаётся через rotarySliderFillColourId) + указатель.
        void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                              float a0, float a1, juce::Slider& s) override
        {
            using namespace col;
            const auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.f);
            const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
            const auto c = b.getCentre();
            const float arcW = 2.5f, ar = r - arcW;
            const float ang = a0 + pos * (a1 - a0);
            const auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);

            g.setColour(raised);
            g.fillEllipse(c.x - r, c.y - r, r * 2.f, r * 2.f);
            g.setColour(line2);
            g.drawEllipse(c.x - r, c.y - r, r * 2.f, r * 2.f, 1.f);

            juce::Path val; val.addCentredArc(c.x, c.y, ar - 2.f, ar - 2.f, 0.f, a0, ang, true);
            g.setColour(s.isEnabled() ? accent : dim);
            g.strokePath(val, juce::PathStrokeType(arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path p;
            p.addRoundedRectangle(-1.f, -ar + 5.f, 2.f, ar * 0.42f, 1.f);
            g.setColour(s.isEnabled() ? text : dim);
            g.fillPath(p, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& bt, const juce::Colour&,
                                  bool highlighted, bool down) override
        {
            using namespace col;
            auto r = bt.getLocalBounds().toFloat().reduced(0.5f);
            const bool on = bt.getToggleState();
            auto fill = on ? juce::Colour(0xff2a2416) : panel;
            if (down) fill = fill.darker(0.2f);
            else if (highlighted) fill = fill.brighter(0.06f);
            g.setColour(fill);
            g.fillRoundedRectangle(r, 6.f);
            g.setColour(on ? amber : line2);
            g.drawRoundedRectangle(r, 6.f, 1.f);
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& bt, bool, bool) override
        {
            g.setFont(font(juce::jmin(13.f, (float) bt.getHeight() * 0.42f), true));
            g.setColour(bt.getToggleState() ? col::amber : (bt.isEnabled() ? col::text : col::faint));
            g.drawFittedText(bt.getButtonText(), bt.getLocalBounds().reduced(4, 0), juce::Justification::centred, 1);
        }

        juce::Font getComboBoxFont(juce::ComboBox& c) override { return font(juce::jmin(13.f, (float) c.getHeight() * 0.5f)); }
        juce::Font getPopupMenuFont() override { return font(14.f); }
        juce::Font getLabelFont(juce::Label& l) override { return font(juce::jmin(13.f, (float) l.getHeight() * 0.7f)); }
    };
}
