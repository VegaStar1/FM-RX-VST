#pragma once
// Редактор файла станции (открывается на месте списка станций): волна, маркеры НАЧАЛО / КОНЕЦ (неразрушающая
// обрезка), fade-in / fade-out, зум и обзор, авто-обрезка тишины, «▶ С НАЧАЛА», LOOP / 1-SHOT, сброс.
#include "PluginProcessor.h"
#include "Strings.h"
#include "Theme.h"
#include "WaveDraw.h"

namespace fmrx::ui
{
    class StationEditor : public juce::Component, private juce::Timer
    {
    public:
        explicit StationEditor(FmRxProcessor& p) : proc_(p)
        {
            back_.onClick = [this] { if (onClose) onClose(); };
            reset_.onClick = [this] { proc_.stations().resetTrim(slot_); syncFades(); view0_ = 0.0; view1_ = duration(); };
            autoTrim_.onClick = [this] { proc_.stations().autoTrim(slot_, (float) thr_.getValue()); };
            play_.onClick = [this] { proc_.playFromStart(slot_); };
            mode_.onClick = [this] { if (modeAtt_) modeAtt_->setValueAsCompleteGesture(oneShot_ ? 0.f : 1.f); };
            for (auto* b : { &back_, &reset_, &autoTrim_, &play_, &mode_ }) addAndMakeVisible(*b);

            title_.setFont(font(14.f, true));
            title_.setEditable(false, true, false);
            title_.onTextChange = [this] { proc_.stations().setName(slot_, title_.getText().toUpperCase().substring(0, 16)); };
            addAndMakeVisible(title_);
            info_.setFont(font(11.f));
            info_.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(info_);

            for (auto* t : { &startTime_, &endTime_ })
            {
                t->setFont(mono(12.f));
                t->setEditable(false, true, false);
                t->setJustificationType(juce::Justification::centredLeft);
                addAndMakeVisible(*t);
            }
            startTime_.onTextChange = [this] { const double v = parseTime(startTime_.getText()); if (v >= 0) commit(v, endSec()); refreshTexts(); };
            endTime_.onTextChange = [this] { const double v = parseTime(endTime_.getText()); if (v > 0) commit(startSec(), v); refreshTexts(); };

            auto setupSlider = [this](juce::Slider& s, double lo, double hi, double step, const char* suffix)
            {
                s.setSliderStyle(juce::Slider::LinearHorizontal);
                s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 18);
                s.setRange(lo, hi, step);
                s.setTextValueSuffix(suffix);
                addAndMakeVisible(s);
            };
            setupSlider(fadeIn_, 0.0, 500.0, 1.0, " ms");
            setupSlider(fadeOut_, 0.0, 500.0, 1.0, " ms");
            setupSlider(thr_, -80.0, -20.0, 1.0, " dB");
            thr_.setValue(-50.0, juce::dontSendNotification);
            fadeIn_.onValueChange = [this] { proc_.stations().setFades(slot_, (float) fadeIn_.getValue(), (float) fadeOut_.getValue()); };
            fadeOut_.onValueChange = fadeIn_.onValueChange;
            for (auto* l : { &fadeInL_, &fadeOutL_, &thrL_, &lenL_, &hintL_ })
            {
                l->setFont(font(10.5f, true));
                l->setInterceptsMouseClicks(false, false);
                addAndMakeVisible(*l);
            }
            hintL_.setFont(font(10.5f));
            applyTheme();
            applyTexts();
        }

        std::function<void()> onClose;

        void open(int slot)
        {
            slot_ = slot;
            if (auto* prm = proc_.state().getParameter(params::st(slot, "mode")))
            {
                modeAtt_ = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { oneShot_ = v > 0.5f; refreshTexts(); });
                modeAtt_->sendInitialUpdate();
            }
            view0_ = 0.0; view1_ = duration();
            syncFades();
            refreshTexts();
            setVisible(true);
            setWantsKeyboardFocus(true);
            grabKeyboardFocus();
            startTimerHz(30);
        }

        bool keyPressed(const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::escapeKey && onClose) { onClose(); return true; }
            return false;
        }
        void close() { stopTimer(); setVisible(false); modeAtt_.reset(); }
        int slot() const { return slot_; }

        void applyTheme()
        {
            for (auto* s : { &fadeIn_, &fadeOut_, &thr_ })
            {
                s->setColour(juce::Slider::trackColourId, col::amber);
                s->setColour(juce::Slider::backgroundColourId, col::line);
                s->setColour(juce::Slider::thumbColourId, col::amber);
                s->setColour(juce::Slider::textBoxTextColourId, col::dim);
                s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            }
            title_.setColour(juce::Label::textColourId, col::text);
            info_.setColour(juce::Label::textColourId, col::dim);
            startTime_.setColour(juce::Label::textColourId, juce::Colour(0xff6fdc8c));
            endTime_.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
            for (auto* l : { &fadeInL_, &fadeOutL_, &thrL_ }) l->setColour(juce::Label::textColourId, col::dim);
            lenL_.setColour(juce::Label::textColourId, col::text);
            hintL_.setColour(juce::Label::textColourId, col::faint);
            repaint();
        }

        void applyTexts()
        {
            back_.setButtonText(tr("ed_back"));
            reset_.setButtonText(tr("ed_reset"));
            autoTrim_.setButtonText(tr("ed_autotrim"));
            play_.setButtonText(tr("ed_play"));
            reset_.setTooltip(tr("tip_reset"));
            autoTrim_.setTooltip(tr("tip_autotrim"));
            play_.setTooltip(tr("tip_play"));
            mode_.setTooltip(tr("st_mode"));
            title_.setTooltip(tr("ed_rename"));
            fadeInL_.setText(tr("ed_fadein"), juce::dontSendNotification);
            fadeOutL_.setText(tr("ed_fadeout"), juce::dontSendNotification);
            thrL_.setText(tr("ed_thr"), juce::dontSendNotification);
            hintL_.setText(tr("ed_hint"), juce::dontSendNotification);
            refreshTexts();
        }

        // Метаданные станции изменились (загрузка, авто-обрезка, сброс...).
        void refreshTexts()
        {
            if (slot_ < 0) return;
            const auto& m = proc_.stations().meta(slot_);
            title_.setText(juce::String(slot_ + 1) + juce::String::fromUTF8(" \xc2\xb7 ") + (m.name.isNotEmpty() ? m.name : tr("st_empty")),
                           juce::dontSendNotification);
            info_.setText(m.loaded ? m.file.getFileName() + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(m.durationSec, 1) + " s"
                                   : tr("ed_nofile"), juce::dontSendNotification);
            mode_.setButtonText(oneShot_ ? "1-SHOT" : "LOOP");
            mode_.setToggleState(oneShot_, juce::dontSendNotification);
            startTime_.setText(tr("ed_start") + "  " + formatTime(startSec()), juce::dontSendNotification);
            endTime_.setText(tr("ed_end") + "  " + formatTime(endSec()), juce::dontSendNotification);
            lenL_.setText(tr("ed_length") + "  " + juce::String(endSec() - startSec(), 2) + " s " + tr("ed_of") + " "
                          + juce::String(duration(), 1) + " s", juce::dontSendNotification);
            if (view1_ <= view0_ || view1_ > duration() + 1e-6) { view0_ = 0.0; view1_ = duration(); }
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            g.setColour(col::band);
            g.fillRoundedRectangle(waveArea_.toFloat(), 6.f);
            g.fillRoundedRectangle(overview_.toFloat(), 4.f);
            g.setColour(col::line);
            g.drawRoundedRectangle(waveArea_.toFloat().reduced(0.5f), 6.f, 1.f);
            g.drawRoundedRectangle(overview_.toFloat().reduced(0.5f), 4.f, 1.f);

            const auto& m = proc_.stations().meta(slot_ < 0 ? 0 : slot_);
            if (slot_ < 0 || m.peaks == nullptr || m.peaks->buckets <= 0)
            {
                g.setColour(col::dim);
                g.setFont(font(13.f));
                g.drawText(tr("ed_nofile"), waveArea_, juce::Justification::centred);
                return;
            }
            const auto& pk = *m.peaks;
            auto wa = waveArea_.toFloat().reduced(2.f, 6.f);
            const auto top = wa.removeFromTop(wa.getHeight() * 0.5f);
            drawPeaks(g, pk, 0, top.reduced(0, 2), view0_, view1_, col::cyan);
            drawPeaks(g, pk, 1, wa.reduced(0, 2), view0_, view1_, col::cyan);
            g.setColour(col::line);
            g.drawHorizontalLine((int) top.getBottom(), (float) waveArea_.getX(), (float) waveArea_.getRight());

            // Срезанные части - затемнены.
            const float xs = xForTime(startSec()), xe = xForTime(endSec());
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            if (xs > waveArea_.getX()) g.fillRect(juce::Rectangle<float>((float) waveArea_.getX(), (float) waveArea_.getY(), xs - waveArea_.getX(), (float) waveArea_.getHeight()));
            if (xe < waveArea_.getRight()) g.fillRect(juce::Rectangle<float>(xe, (float) waveArea_.getY(), waveArea_.getRight() - xe, (float) waveArea_.getHeight()));

            // Fade-in / fade-out: линии усиления.
            const float yT = (float) waveArea_.getY() + 4.f, yB = (float) waveArea_.getBottom() - 4.f;
            g.setColour(col::amber.withAlpha(0.9f));
            const float xfi = xForTime(startSec() + m.fadeInMs * 0.001), xfo = xForTime(endSec() - (oneShot_ ? m.fadeOutMs * 0.001 : 0.0));
            g.drawLine(xs, yB, xfi, yT, 1.2f);
            if (oneShot_) g.drawLine(xfo, yT, xe, yB, 1.2f);

            // Маркеры.
            drawMarker(g, xs, juce::Colour(0xff6fdc8c), true);
            drawMarker(g, xe, juce::Colour(0xffff6b6b), false);

            // Позиция воспроизведения.
            const double pos = proc_.playPositionSec(slot_);
            if (pos >= view0_ && pos <= view1_)
            {
                g.setColour(col::text.withAlpha(0.85f));
                const float xp = xForTime(pos);
                g.drawVerticalLine((int) xp, (float) waveArea_.getY(), (float) waveArea_.getBottom());
            }

            // Обзор: весь файл + рамка видимой части.
            const auto ov = overview_.toFloat().reduced(2.f, 2.f);
            drawPeaks(g, pk, 0, ov, 0.0, duration(), col::cyan.withAlpha(0.6f));
            const float ox0 = ov.getX() + (float) (view0_ / duration()) * ov.getWidth();
            const float ox1 = ov.getX() + (float) (view1_ / duration()) * ov.getWidth();
            g.setColour(col::amber);
            g.drawRoundedRectangle(juce::Rectangle<float>(ox0, (float) overview_.getY(), juce::jmax(3.f, ox1 - ox0), (float) overview_.getHeight()), 3.f, 1.2f);
            const float os = ov.getX() + (float) (startSec() / duration()) * ov.getWidth();
            const float oe = ov.getX() + (float) (endSec() / duration()) * ov.getWidth();
            g.setColour(juce::Colour(0xff6fdc8c)); g.fillRect(os - 0.5f, ov.getY(), 1.5f, ov.getHeight());
            g.setColour(juce::Colour(0xffff6b6b)); g.fillRect(oe - 1.f, ov.getY(), 1.5f, ov.getHeight());
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto head = r.removeFromTop(30);
            back_.setBounds(head.removeFromLeft(110).reduced(0, 2));
            head.removeFromLeft(12);
            reset_.setBounds(head.removeFromRight(80).reduced(0, 2));
            head.removeFromRight(6);
            mode_.setBounds(head.removeFromRight(70).reduced(0, 2));
            head.removeFromRight(10);
            title_.setBounds(head.removeFromLeft(juce::jmin(220, head.getWidth() / 2)));
            info_.setBounds(head);
            r.removeFromTop(8);

            auto bottom = r.removeFromBottom(124);
            overview_ = r.removeFromBottom(24);
            r.removeFromBottom(6);
            auto times = r.removeFromTop(20);
            startTime_.setBounds(times.removeFromLeft(times.getWidth() / 2));
            endTime_.setBounds(times);
            endTime_.setJustificationType(juce::Justification::centredRight);
            r.removeFromTop(2);
            waveArea_ = r;

            bottom.removeFromTop(6);
            hintL_.setBounds(bottom.removeFromTop(16));
            bottom.removeFromTop(4);
            auto row1 = bottom.removeFromTop(30);
            autoTrim_.setBounds(row1.removeFromLeft(190).reduced(0, 2));
            row1.removeFromLeft(8);
            play_.setBounds(row1.removeFromLeft(130).reduced(0, 2));
            row1.removeFromLeft(14);
            lenL_.setBounds(row1);
            bottom.removeFromTop(6);
            auto row2 = bottom.removeFromTop(26);
            auto c1 = row2.removeFromLeft(row2.getWidth() / 2), c2 = row2;
            fadeInL_.setBounds(c1.removeFromLeft(70)); fadeIn_.setBounds(c1.reduced(4, 0).withTrimmedRight(12));
            fadeOutL_.setBounds(c2.removeFromLeft(80)); fadeOut_.setBounds(c2.reduced(4, 0));
            bottom.removeFromTop(4);
            auto row3 = bottom.removeFromTop(26);
            auto c3 = row3.removeFromLeft(row3.getWidth() / 2);
            thrL_.setBounds(c3.removeFromLeft(130)); thr_.setBounds(c3.reduced(4, 0).withTrimmedRight(12));
        }

        // --- мышь: маркеры, зум, обзор ---
        void mouseDown(const juce::MouseEvent& e) override
        {
            drag_ = None;
            if (overview_.contains(e.getPosition())) { drag_ = Overview; moveViewTo(e.position.x); return; }
            if (! waveArea_.expanded(0, 14).contains(e.getPosition())) return;
            const float xs = xForTime(startSec()), xe = xForTime(endSec());
            if (std::fabs(e.position.x - xs) <= 8.f) drag_ = Start;
            else if (std::fabs(e.position.x - xe) <= 8.f) drag_ = End;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (drag_ == Overview) { moveViewTo(e.position.x); return; }
            if (drag_ == Start) commit(juce::jmin(timeForX(e.position.x), endSec() - 0.05), endSec(), false);
            if (drag_ == End)   commit(startSec(), juce::jmax(timeForX(e.position.x), startSec() + 0.05), false);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            // Отпустили маркер - прилипаем к нулю волны, чтобы старт и стык не щёлкали.
            if (drag_ == Start) commit(proc_.stations().snapSec(slot_, startSec()), endSec(), false);
            if (drag_ == End)   commit(startSec(), proc_.stations().snapSec(slot_, endSec()), false);
            drag_ = None;
        }

        void mouseDoubleClick(const juce::MouseEvent& e) override
        {
            const float xs = xForTime(startSec()), xe = xForTime(endSec());
            if (std::fabs(e.position.x - xs) <= 8.f) commit(0.0, endSec());
            else if (std::fabs(e.position.x - xe) <= 8.f) commit(startSec(), duration());
        }

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const double dur = duration();
            if (dur <= 0.0 || ! (waveArea_.contains(e.getPosition()) || overview_.contains(e.getPosition()))) return;
            const double span = view1_ - view0_;
            if (e.mods.isShiftDown())
            {
                const double d = -(std::fabs(w.deltaY) > 1e-6f ? w.deltaY : w.deltaX) * span * 0.5;
                setView(view0_ + d, view1_ + d);
                return;
            }
            const double at = timeForX(e.position.x);
            const double k = w.deltaY > 0 ? 0.8 : 1.25;
            const double ns = juce::jlimit(juce::jmin(0.05, dur), dur, span * k);
            const double f = (at - view0_) / juce::jmax(1e-9, span);
            setView(at - f * ns, at - f * ns + ns);
        }

    private:
        enum Drag { None, Start, End, Overview };

        void timerCallback() override { repaint(waveArea_); }

        double duration() const
        {
            if (slot_ < 0) return 1.0;
            const auto& m = proc_.stations().meta(slot_);
            return m.durationSec > 0.0 ? m.durationSec : 1.0;
        }
        double startSec() const { return slot_ < 0 ? 0.0 : proc_.stations().meta(slot_).trimStart; }
        double endSec() const
        {
            if (slot_ < 0) return 1.0;
            const auto& m = proc_.stations().meta(slot_);
            return m.trimEnd > 0.0 ? m.trimEnd : duration();
        }

        void commit(double s, double e, bool snap = true)
        {
            if (slot_ < 0) return;
            if (snap) { s = proc_.stations().snapSec(slot_, s); e = proc_.stations().snapSec(slot_, e); }
            proc_.stations().setTrim(slot_, s, e);
            refreshTexts();
        }

        void syncFades()
        {
            if (slot_ < 0) return;
            const auto& m = proc_.stations().meta(slot_);
            fadeIn_.setValue(m.fadeInMs, juce::dontSendNotification);
            fadeOut_.setValue(m.fadeOutMs, juce::dontSendNotification);
        }

        float xForTime(double t) const
        {
            return (float) waveArea_.getX() + (float) ((t - view0_) / juce::jmax(1e-9, view1_ - view0_)) * (float) waveArea_.getWidth();
        }
        double timeForX(float x) const
        {
            return juce::jlimit(0.0, duration(), view0_ + (x - waveArea_.getX()) / juce::jmax(1.0, (double) waveArea_.getWidth()) * (view1_ - view0_));
        }
        void setView(double a, double b)
        {
            const double dur = duration(), span = b - a;
            if (a < 0.0) { a = 0.0; b = span; }
            if (b > dur) { b = dur; a = juce::jmax(0.0, dur - span); }
            view0_ = a; view1_ = b;
            repaint();
        }
        void moveViewTo(float x)
        {
            const double span = view1_ - view0_;
            const double c = (x - overview_.getX()) / juce::jmax(1.0, (double) overview_.getWidth()) * duration();
            setView(c - span * 0.5, c + span * 0.5);
        }

        void drawMarker(juce::Graphics& g, float x, juce::Colour c, bool start)
        {
            if (x < waveArea_.getX() - 1 || x > waveArea_.getRight() + 1) return;
            g.setColour(c);
            g.fillRect(x - 1.f, (float) waveArea_.getY(), 2.f, (float) waveArea_.getHeight());
            juce::Path flag;
            const float y = (float) waveArea_.getY();
            flag.addTriangle(x, y, x + (start ? 10.f : -10.f), y + 5.f, x, y + 10.f);
            g.fillPath(flag);
        }

        FmRxProcessor& proc_;
        int slot_ = -1;
        bool oneShot_ = false;
        double view0_ = 0.0, view1_ = 1.0;
        Drag drag_ = None;
        juce::Rectangle<int> waveArea_, overview_;
        juce::TextButton back_, reset_, autoTrim_, play_, mode_;
        juce::Label title_, info_, startTime_, endTime_, fadeInL_, fadeOutL_, thrL_, lenL_, hintL_;
        juce::Slider fadeIn_, fadeOut_, thr_;
        std::unique_ptr<juce::ParameterAttachment> modeAtt_;
    };
}
