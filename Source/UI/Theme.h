#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Палитра текущего скина. Цвета - изменяемые глобальные (только message-поток): applySkin() переключает
// их для всех открытых окон, после чего окна перерисовываются.
namespace fmrx::ui::col
{
    inline juce::Colour bg, panel, raised, line, line2, text, dim, faint;
    inline juce::Colour amber, amberDk, lcd, lcdLine, lcdText, lcdDim, lcdDeep, lcdEdge;
    inline juce::Colour cyan, band, noiseFloor, warn, rowHot, rowHotEdge, onFill;
}

namespace fmrx::ui
{
    struct Skin
    {
        const char* id; const char* nameEn; const char* nameRu;
        juce::uint32 bg, panel, raised, line, line2, text, dim, faint;
        juce::uint32 accent, accentDk, lcd, lcdLine, lcdText, lcdDim, lcdDeep, lcdEdge;
        juce::uint32 accent2, band, noiseFloor, warn, rowHot, rowHotEdge, onFill;
    };

    inline const Skin kSkins[] = {
        { "graphite", "Graphite Amber", "Графит и янтарь",
          0xff121315, 0xff1a1c1f, 0xff222428, 0xff2a2d31, 0xff34383d, 0xffe8e4da, 0xff8b8f96, 0xff5d6168,
          0xfff2a541, 0xff4a402b, 0xff17150f, 0xff3a3220, 0xfff2d39a, 0xffb8893a, 0xff0f0d09, 0xff2e281b,
          0xff5bc8d6, 0xff0f1012, 0xff3c5f66, 0xffd9635a, 0xff24211a, 0xff5a4826, 0xff2a2416 },
        { "midnight", "Midnight Blue", "Полночный синий",
          0xff0d1120, 0xff141a2c, 0xff1b2337, 0xff253049, 0xff2f3b57, 0xffe3e9f5, 0xff8a96ad, 0xff5a6580,
          0xff7cc4ff, 0xff2a3d57, 0xff0b1222, 0xff22344f, 0xffcfe8ff, 0xff6f9bc4, 0xff070c18, 0xff1b2a42,
          0xffff9e6b, 0xff0a0e1a, 0xff3a4f72, 0xffff6b6b, 0xff16223a, 0xff2e5a8a, 0xff15263e },
        { "vfd", "VFD Green", "Зелёный VFD",
          0xff0c0f0d, 0xff131815, 0xff1a201c, 0xff243029, 0xff2d3b33, 0xffdcefe4, 0xff84998c, 0xff56685d,
          0xff5ff2a8, 0xff23463a, 0xff06100b, 0xff1a3a2b, 0xffa8ffd2, 0xff3fae7c, 0xff030806, 0xff143024,
          0xfff2c94c, 0xff080b09, 0xff2e5a46, 0xffff6b5b, 0xff14241c, 0xff2c6b4f, 0xff13261d },
        { "ivory", "Ivory Retro", "Слоновая кость",
          0xffe9e3d5, 0xfff4efe4, 0xfffbf8f1, 0xffd6ccb8, 0xffc4b8a0, 0xff2a2621, 0xff6b6356, 0xff9a9081,
          0xffc4572b, 0xff6e4a35, 0xff22241f, 0xff3d3a30, 0xfff2d39a, 0xffb8893a, 0xff151612, 0xff34322a,
          0xff2f7f86, 0xffe1dac9, 0xffb5ab95, 0xffb3362b, 0xfff1e2cf, 0xffc98a5e, 0xfff5e3d4 },
        { "crimson", "Crimson Night", "Багровая ночь",
          0xff130d0e, 0xff1c1315, 0xff25191c, 0xff332327, 0xff3f2c31, 0xfff1e3e4, 0xff9e8b8e, 0xff6d5b5f,
          0xffff5a5f, 0xff4d2629, 0xff170b0c, 0xff3e1d20, 0xffffc2c4, 0xffc4585c, 0xff0c0506, 0xff2d1417,
          0xffc9b6f2, 0xff0f0a0b, 0xff5a3a48, 0xffffa64d, 0xff2a1619, 0xff7a3236, 0xff2c1518 },
    };
    inline constexpr int kNumSkins = (int) (sizeof(kSkins) / sizeof(kSkins[0]));

    inline int& currentSkin() { static int s = 0; return s; }

    inline void applySkin(int index)
    {
        index = juce::jlimit(0, kNumSkins - 1, index);
        currentSkin() = index;
        const auto& k = kSkins[index];
        using juce::Colour;
        col::bg = Colour(k.bg); col::panel = Colour(k.panel); col::raised = Colour(k.raised);
        col::line = Colour(k.line); col::line2 = Colour(k.line2); col::text = Colour(k.text);
        col::dim = Colour(k.dim); col::faint = Colour(k.faint);
        col::amber = Colour(k.accent); col::amberDk = Colour(k.accentDk); col::lcd = Colour(k.lcd);
        col::lcdLine = Colour(k.lcdLine); col::lcdText = Colour(k.lcdText); col::lcdDim = Colour(k.lcdDim);
        col::lcdDeep = Colour(k.lcdDeep); col::lcdEdge = Colour(k.lcdEdge);
        col::cyan = Colour(k.accent2); col::band = Colour(k.band); col::noiseFloor = Colour(k.noiseFloor);
        col::warn = Colour(k.warn); col::rowHot = Colour(k.rowHot); col::rowHotEdge = Colour(k.rowHotEdge);
        col::onFill = Colour(k.onFill);
    }

    inline int skinIndexForId(const juce::String& id)
    {
        for (int i = 0; i < kNumSkins; ++i) if (id == kSkins[i].id) return i;
        return 0;
    }
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
        FmLookAndFeel() { applyTheme(); }

        // Переустановить цвета компонентов JUCE из текущего скина.
        void applyTheme()
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
            auto fill = on ? onFill : panel;
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
