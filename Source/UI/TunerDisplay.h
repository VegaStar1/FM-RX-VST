#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

namespace fmrx::ui
{
    // Окно тюнера: слева табло (частота, RDS, индикаторы, S-метр), справа диапазон со станциями и стрелкой.
    class TunerDisplay : public juce::Component, private juce::Timer
    {
    public:
        explicit TunerDisplay(FmRxProcessor& p) : proc_(p)
        {
            startTimerHz(30);
            setRepaintsOnMouseActivity(false);
        }

        void paint(juce::Graphics& g) override
        {
            const auto m = proc_.meters();
            auto r = getLocalBounds().toFloat();
            const auto readout = r.removeFromLeft(juce::jmin(280.f, r.getWidth() * 0.28f));
            r.removeFromLeft(12.f);
            paintReadout(g, readout, m);
            paintBand(g, r, m);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (! bandArea_.contains(e.position)) return;
            dragStartTune_ = tune();
            dragStartX_ = e.position.x;
            // Клик рядом с пиком станции - настройка на неё.
            for (int i = 0; i < kNumStations; ++i)
                if (proc_.stationAudible(i) && std::fabs(xFor(proc_.stationFreq(i)) - e.position.x) < 10.f
                    && e.position.y < bandArea_.getBottom() - 4.f)
                {
                    proc_.tuneTo(proc_.stationFreq(i));
                    dragged_ = true;
                    return;
                }
            dragged_ = false;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (! bandArea_.contains(e.mouseDownPosition)) return;
            dragged_ = true;
            // Перетаскивание = «верньер»: 1 пиксель ~ 10 кГц, с Shift - точнее.
            const float perPx = e.mods.isShiftDown() ? 0.002f : 0.01f;
            proc_.tuneTo(juce::jlimit(87.5f, 108.f, dragStartTune_ + (e.position.x - dragStartX_) * perPx));
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (! dragged_ && bandArea_.contains(e.position) && e.getNumberOfClicks() >= 1)
                proc_.tuneTo(juce::jlimit(87.5f, 108.f, (float) mhzFor(e.position.x)));
        }

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (! bandArea_.contains(e.position)) return;
            const float step = e.mods.isShiftDown() ? 0.01f : 0.05f;
            proc_.tuneTo(juce::jlimit(87.5f, 108.f, tune() + (w.deltaY > 0 ? step : -step)));
        }

    private:
        void timerCallback() override { ++frame_; repaint(); }

        float tune() const { return proc_.state().getRawParameterValue(params::tune)->load(); }
        float param(const char* id) const { return proc_.state().getRawParameterValue(id)->load(); }

        float xFor(double mhz) const
        {
            return bandArea_.getX() + (float) ((mhz - dsp::kBandLowMHz) / (dsp::kBandHighMHz - dsp::kBandLowMHz)) * bandArea_.getWidth();
        }
        double mhzFor(float x) const
        {
            return dsp::kBandLowMHz + (x - bandArea_.getX()) / bandArea_.getWidth() * (dsp::kBandHighMHz - dsp::kBandLowMHz);
        }

        void paintReadout(juce::Graphics& g, juce::Rectangle<float> r, const BandMeters& m)
        {
            using namespace col;
            g.setColour(lcd);
            g.fillRoundedRectangle(r, 10.f);
            g.setColour(lcdLine);
            g.drawRoundedRectangle(r.reduced(0.5f), 10.f, 1.f);
            r = r.reduced(16.f, 12.f);

            const int tuned = proc_.tunedStation();
            const bool isTuned = tuned >= 0 && m.snrDb > 10.f;
            const bool stereo = isTuned && m.blend > 0.5f;
            const bool afcOn = param(params::afc) > 0.5f;

            auto top = r.removeFromTop(16.f);
            g.setFont(mono(11.f));
            const float w3 = top.getWidth() / 3.f;
            g.setColour(stereo ? amber : amberDk); g.drawText(juce::String::fromUTF8("\xe2\x97\x8f STEREO"), top.removeFromLeft(w3), juce::Justification::left);
            g.setColour(isTuned ? amber : amberDk); g.drawText("TUNED", top.removeFromLeft(w3), juce::Justification::centred);
            g.setColour(afcOn ? amber : amberDk);   g.drawText("AFC", top, juce::Justification::right);

            r.removeFromTop(6.f);
            auto freqRow = r.removeFromTop(r.getHeight() * 0.42f);
            g.setColour(amber);
            g.setFont(mono(juce::jmin(60.f, freqRow.getHeight()), true));
            auto unit = freqRow.removeFromRight(44.f);
            g.drawFittedText(juce::String(tune(), 1), freqRow.toNearestInt(), juce::Justification::centredRight, 1);
            g.setColour(juce::Colour(0xffb8893a));
            g.setFont(mono(15.f));
            g.drawText("MHz", unit.withTrimmedTop(unit.getHeight() * 0.45f), juce::Justification::centred);

            r.removeFromTop(6.f);
            auto rds = r.removeFromTop(34.f);
            g.setColour(juce::Colour(0xff0f0d09));
            g.fillRoundedRectangle(rds, 5.f);
            g.setColour(juce::Colour(0xff2e281b));
            g.drawRoundedRectangle(rds, 5.f, 1.f);
            juce::String name = juce::String::fromUTF8("\xc2\xb7 \xc2\xb7 \xc2\xb7 \xc2\xb7 \xc2\xb7 \xc2\xb7");
            if (isTuned)
            {
                name = proc_.stations().meta(tuned).name;
                if (name.isEmpty()) name = "STATION " + juce::String(tuned + 1);
            }
            g.setColour(lcdText);
            g.setFont(mono(17.f));
            g.drawFittedText(name, rds.toNearestInt().reduced(6, 0), juce::Justification::centred, 1);

            r.removeFromTop(8.f);
            auto bottom = r;
            const int lit = juce::jlimit(0, 6, (int) std::lround((m.snrDb - 4.f) / 7.f));
            for (int b = 0; b < 6; ++b)
            {
                const float h = 8.f + (float) b * 5.f;
                const juce::Rectangle<float> bar(bottom.getX() + (float) b * 14.f, bottom.getBottom() - h, 10.f, h);
                g.setColour(b < lit ? amber : lcdLine);
                g.fillRoundedRectangle(bar, 2.f);
            }
            g.setColour(juce::Colour(0xffb8893a));
            g.setFont(mono(11.f));
            g.drawText("SNR " + juce::String(juce::roundToInt(m.snrDb)) + " dB", bottom.removeFromBottom(15.f), juce::Justification::right);
            if (std::fabs(m.afcHz) > 500.f)
                g.drawText("AFC " + juce::String(m.afcHz / 1000.f, 1) + " kHz", bottom.removeFromBottom(15.f), juce::Justification::right);
        }

        void paintBand(juce::Graphics& g, juce::Rectangle<float> r, const BandMeters& m)
        {
            using namespace col;
            g.setColour(band);
            g.fillRoundedRectangle(r, 10.f);
            g.setColour(line);
            g.drawRoundedRectangle(r.reduced(0.5f), 10.f, 1.f);
            r = r.reduced(16.f, 10.f);

            auto head = r.removeFromTop(16.f);
            g.setFont(font(11.f));
            g.setColour(dim);
            g.drawText(juce::String::fromUTF8("BAND 87.5 \xe2\x80\x93 108.0 MHz"), head, juce::Justification::left);
            g.drawText("IF " + juce::String(juce::roundToInt(param(params::ifBw))) + " kHz", head, juce::Justification::right);

            auto scale = r.removeFromBottom(16.f);
            bandArea_ = r.withTrimmedTop(6.f);
            const auto a = bandArea_;

            // Шумовая полка: зависит от уровня шума эфира.
            const float noiseNorm = juce::jlimit(0.f, 1.f, (param(params::noise) + 100.f) / 80.f);
            juce::Random rnd((juce::int64) frame_ * 7919);
            const int cols = juce::jmax(40, (int) (a.getWidth() / 4.f));
            g.setColour(juce::Colour(0xff3c5f66).withAlpha(0.6f));
            for (int i = 0; i < cols; ++i)
            {
                const float h = (4.f + 30.f * noiseNorm) * (0.4f + 0.6f * rnd.nextFloat());
                const float x = a.getX() + a.getWidth() * (float) i / (float) cols;
                g.fillRect(x, a.getBottom() - h, a.getWidth() / (float) cols - 1.f, h);
            }

            // Полоса ПЧ вокруг стрелки.
            const float nx = xFor(tune());
            const float ifW = (float) (param(params::ifBw) * 1000.0 / ((dsp::kBandHighMHz - dsp::kBandLowMHz) * 1e6)) * a.getWidth();
            g.setColour(amber.withAlpha(0.10f));
            g.fillRect(nx - ifW * 0.5f, a.getY(), ifW, a.getHeight());

            // Станции: высота пика = мощность передатчика с учётом замираний, «дышит» со звуком.
            const int tuned = proc_.tunedStation();
            struct Lbl { float x = -1.f, y = 0.f, hot = 0.f; };
            std::array<Lbl, kNumStations> labels {};
            for (int i = 0; i < kNumStations; ++i)
            {
                if (! proc_.stationAudible(i)) continue;
                const float level = proc_.state().getRawParameterValue(params::st(i, "level"))->load();
                const float lv = juce::jlimit(0.f, 1.f, (level + 60.f) / 60.f);
                const float fadeV = juce::jlimit(0.f, 1.2f, m.strength[(size_t) i] / juce::Decibels::decibelsToGain(level));
                const float act = juce::jlimit(0.f, 1.f, m.activity[(size_t) i] * 4.f);
                const float h = a.getHeight() * 0.85f * (0.15f + 0.85f * lv) * (0.6f + 0.25f * fadeV + 0.15f * act);
                const float x = xFor(proc_.stationFreq(i));
                const bool hot = i == tuned;
                juce::Path peak;
                peak.startNewSubPath(x, a.getBottom() - h);
                peak.lineTo(x + 3.f, a.getBottom() - h * 0.45f);
                peak.lineTo(x + 16.f, a.getBottom());
                peak.lineTo(x - 16.f, a.getBottom());
                peak.lineTo(x - 3.f, a.getBottom() - h * 0.45f);
                peak.closeSubPath();
                g.setColour(hot ? amber : cyan);
                g.fillPath(peak);
                labels[(size_t) i] = { x, a.getBottom() - h - 15.f, hot ? 1.f : 0.f };
            }

            // Стрелка.
            g.setColour(amber.withAlpha(0.25f));
            g.fillRect(nx - 3.f, a.getY() - 4.f, 6.f, a.getHeight() + 8.f);
            g.setColour(amber);
            g.fillRect(nx - 1.f, a.getY() - 4.f, 2.f, a.getHeight() + 8.f);

            // Номера станций - поверх стрелки, чтобы не прятались.
            g.setFont(mono(10.f));
            for (int i = 0; i < kNumStations; ++i)
            {
                const auto& l = labels[(size_t) i];
                if (l.x < 0.f) continue;
                const auto box = juce::Rectangle<float>(l.x - 8.f, l.y, 16.f, 12.f);
                if (l.hot > 0.f) { g.setColour(band); g.fillRoundedRectangle(box, 3.f); }
                g.setColour(l.hot > 0.f ? amber : dim);
                g.drawText(juce::String(i + 1), box, juce::Justification::centred);
            }

            // Шкала.
            g.setFont(mono(10.f));
            g.setColour(dim);
            for (int f = 88; f <= 108; f += 2)
            {
                const float x = xFor(f);
                g.drawText(juce::String(f), juce::Rectangle<float>(x - 14.f, scale.getY() + 2.f, 28.f, 12.f), juce::Justification::centred);
            }
            g.setColour(line);
            g.drawHorizontalLine((int) a.getBottom(), a.getX(), a.getRight());
        }

        FmRxProcessor& proc_;
        juce::Rectangle<float> bandArea_;
        float dragStartTune_ = 98.f, dragStartX_ = 0.f;
        bool dragged_ = false;
        int frame_ = 0;
    };
}
