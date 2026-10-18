#pragma once
#include "PluginProcessor.h"
#include "Strings.h"
#include "Theme.h"

namespace fmrx::ui
{
    // Окно тюнера: слева табло (частота, RDS, индикаторы, S-метр), справа диапазон: зоны приёма станций
    // (чистое ядро + шумовые «юбки», ширина - от ручек CLEAN/NOISE ZONE), пики станций и стрелка.
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
            dragStartTune_ = proc_.displayFreq();
            dragStartX_ = e.position.x;
            for (int i = 0; i < kNumStations; ++i)                  // клик по пику - настройка на станцию
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
            const float perPx = e.mods.isShiftDown() ? 0.002f : 0.01f;    // «верньер»: 1 px ~ 10 кГц
            proc_.tuneTo(juce::jlimit(87.5f, 108.f, dragStartTune_ + (e.position.x - dragStartX_) * perPx));
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (! dragged_ && bandArea_.contains(e.position))
                proc_.tuneTo(juce::jlimit(87.5f, 108.f, (float) mhzFor(e.position.x)));
        }

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (! bandArea_.contains(e.position)) return;
            const float step = e.mods.isShiftDown() ? 0.01f : 0.05f;
            proc_.tuneTo(juce::jlimit(87.5f, 108.f, proc_.displayFreq() + (w.deltaY > 0 ? step : -step)));
        }

    private:
        void timerCallback() override { ++frame_; repaint(); }

        float xFor(double mhz) const
        {
            return bandArea_.getX() + (float) ((mhz - dsp::kBandLowMHz) / (dsp::kBandHighMHz - dsp::kBandLowMHz)) * bandArea_.getWidth();
        }
        double mhzFor(float x) const
        {
            return dsp::kBandLowMHz + (x - bandArea_.getX()) / bandArea_.getWidth() * (dsp::kBandHighMHz - dsp::kBandLowMHz);
        }
        float pxPerHz() const { return bandArea_.getWidth() / (float) ((dsp::kBandHighMHz - dsp::kBandLowMHz) * 1e6); }

        void paintReadout(juce::Graphics& g, juce::Rectangle<float> r, const BandMeters& m)
        {
            using namespace col;
            g.setColour(lcd);
            g.fillRoundedRectangle(r, 10.f);
            g.setColour(lcdLine);
            g.drawRoundedRectangle(r.reduced(0.5f), 10.f, 1.f);
            r = r.reduced(16.f, 12.f);

            const auto& c = proc_.control();
            const int st = c.state();
            using S = RadioControl::State;
            const bool muted = proc_.param(params::mute) > 0.5f;
            const bool moving = st == (int) S::Searching || st == (int) S::Settling || st == (int) S::Scanning;
            const int tuned = moving ? -1 : proc_.tunedStation();
            const bool isTuned = tuned >= 0 && m.snrDb > 10.f;
            const bool stereo = isTuned && m.blend > 0.5f;

            // Строка индикаторов: STEREO, состояние (TUNED / SEARCH / SCAN / LOCK), MUTE.
            auto top = r.removeFromTop(16.f);
            g.setFont(mono(11.f));
            const float w3 = top.getWidth() / 3.f;
            g.setColour(stereo ? amber : amberDk);
            g.drawText(juce::String::fromUTF8("\xe2\x97\x8f STEREO"), top.removeFromLeft(w3), juce::Justification::left);
            juce::String mid = tr("tuned");
            bool midOn = isTuned;
            if (st == (int) S::Locked) { mid = tr("locked"); midOn = true; }
            else if (st == (int) S::Scanning || st == (int) S::Dwell) { mid = tr("scanning"); midOn = (frame_ / 8) % 2 == 0 || st == (int) S::Dwell; }
            else if (moving || c.endSearchActive()) { mid = tr("searching"); midOn = (frame_ / 8) % 2 == 0; }
            g.setColour(midOn ? amber : amberDk);
            g.drawText(mid, top.removeFromLeft(w3), juce::Justification::centred);
            g.setColour(muted ? warn : amberDk);
            g.drawText(tr("muted"), top, juce::Justification::right);

            r.removeFromTop(6.f);
            auto freqRow = r.removeFromTop(r.getHeight() * 0.42f);
            g.setColour(muted ? amber.withAlpha(0.35f) : amber);
            g.setFont(mono(juce::jmin(60.f, freqRow.getHeight()), true));
            auto unit = freqRow.removeFromRight(44.f);
            g.drawFittedText(juce::String(proc_.displayFreq(), 1), freqRow.toNearestInt(), juce::Justification::centredRight, 1);
            g.setColour(lcdDim);
            g.setFont(mono(15.f));
            g.drawText("MHz", unit.withTrimmedTop(unit.getHeight() * 0.45f), juce::Justification::centred);

            r.removeFromTop(6.f);
            auto rds = r.removeFromTop(34.f);
            g.setColour(lcdDeep);
            g.fillRoundedRectangle(rds, 5.f);
            g.setColour(lcdEdge);
            g.drawRoundedRectangle(rds, 5.f, 1.f);
            juce::String name = juce::String::fromUTF8("\xc2\xb7 \xc2\xb7 \xc2\xb7 \xc2\xb7 \xc2\xb7 \xc2\xb7");
            if (isTuned)
            {
                name = proc_.stations().meta(tuned).name;
                if (name.isEmpty()) name = "STATION " + juce::String(tuned + 1);
            }
            else if (moving) name = juce::String::fromUTF8("\xe2\x80\xa6 ") + tr("searching") + juce::String::fromUTF8(" \xe2\x80\xa6");
            g.setColour(lcdText.withAlpha(muted ? 0.35f : 1.f));
            g.setFont(mono(17.f));
            g.drawFittedText(name, rds.toNearestInt().reduced(6, 0), juce::Justification::centred, 1);

            r.removeFromTop(8.f);
            auto bottom = r;
            const int lit = muted ? 0 : juce::jlimit(0, 6, (int) std::lround((m.snrDb - 4.f) / 7.f));
            for (int b = 0; b < 6; ++b)
            {
                const float h = 8.f + (float) b * 5.f;
                const juce::Rectangle<float> bar(bottom.getX() + (float) b * 14.f, bottom.getBottom() - h, 10.f, h);
                g.setColour(b < lit ? amber : lcdLine);
                g.fillRoundedRectangle(bar, 2.f);
            }
            g.setColour(lcdDim);
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

            const float cleanHz = proc_.param(params::cleanZone) * 1000.f;
            const float noiseHz = proc_.param(params::noiseZone) * 1000.f;

            auto head = r.removeFromTop(16.f);
            g.setFont(font(11.f));
            g.setColour(dim);
            g.drawText(tr("band"), head, juce::Justification::left);
            auto fmtZone = [](float hz) { return hz >= 1e6f ? juce::String(hz / 1e6f, 2) + " MHz" : juce::String(juce::roundToInt(hz / 1000.f)) + " kHz"; };
            g.drawText("ZONE " + fmtZone(cleanHz) + " + " + fmtZone(noiseHz) + "   IF " + juce::String(juce::roundToInt(proc_.param(params::ifBw))) + " kHz",
                       head, juce::Justification::right);

            auto scale = r.removeFromBottom(16.f);
            bandArea_ = r.withTrimmedTop(6.f);
            const auto a = bandArea_;

            const int tuned = proc_.tunedStation();
            const auto& ctl = proc_.control();

            // Зоны приёма станций (на всю высоту): чистое ядро и шумовые «юбки» с градиентом.
            auto drawZones = [&](float y0, float h, float coreA, float skirtA)
            {
                for (int i = 0; i < kNumStations; ++i)
                {
                    if (! proc_.stationAudible(i) || (ctl.doneMask() & (1u << i))) continue;
                    const float x = xFor(proc_.stationFreq(i));
                    const auto c = i == tuned ? amber : cyan;
                    const float cw = juce::jmax(1.5f, cleanHz * pxPerHz());
                    const float nw = juce::jmax(1.f, noiseHz * pxPerHz());
                    for (int side = -1; side <= 1; side += 2)
                    {
                        const float x0 = x + (float) side * cw, x1 = x + (float) side * (cw + nw);
                        juce::ColourGradient grad(c.withAlpha(skirtA), x0, 0.f, c.withAlpha(0.f), x1, 0.f, false);
                        g.setGradientFill(grad);
                        g.fillRect(juce::Rectangle<float>(juce::jmin(x0, x1), y0, std::fabs(x1 - x0), h));
                    }
                    g.setColour(c.withAlpha(coreA));
                    g.fillRect(x - cw, y0, 2.f * cw, h);
                }
            };
            drawZones(a.getY(), a.getHeight(), 0.16f, 0.13f);

            // Шумовая полка: зависит от уровня шума эфира.
            const float noiseNorm = juce::jlimit(0.f, 1.f, (proc_.param(params::noise) + 100.f) / 80.f);
            juce::Random rnd((juce::int64) frame_ * 7919);
            const int cols = juce::jmax(40, (int) (a.getWidth() / 4.f));
            g.setColour(noiseFloor.withAlpha(0.6f));
            for (int i = 0; i < cols; ++i)
            {
                const float h = (4.f + 30.f * noiseNorm) * (0.4f + 0.6f * rnd.nextFloat());
                const float x = a.getX() + a.getWidth() * (float) i / (float) cols;
                g.fillRect(x, a.getBottom() - h, a.getWidth() / (float) cols - 1.f, h);
            }

            // Полоса зон у шкалы - поверх шумовой полки, чтобы ширину было видно всегда.
            drawZones(a.getBottom() - 6.f, 6.f, 0.95f, 0.75f);

            // IF-полоса вокруг стрелки.
            const float tunedF = proc_.displayFreq();
            const float nx = xFor(tunedF);
            const float ifW = proc_.param(params::ifBw) * 1000.f * pxPerHz();
            g.setColour(amber.withAlpha(0.10f));
            g.fillRect(nx - ifW * 0.5f, a.getY(), ifW, a.getHeight());

            // Пики станций: высота = мощность передатчика с замираниями, «дышит» со звуком.
            struct Lbl { float x = -1.f, y = 0.f; bool hot = false; int state = 0; };
            std::array<Lbl, kNumStations> labels {};
            for (int i = 0; i < kNumStations; ++i)
            {
                if (! proc_.stationAudible(i)) continue;
                const bool done = (ctl.doneMask() & (1u << i)) != 0;
                const bool parked = (ctl.parkedMask() & (1u << i)) != 0;
                const float level = proc_.state().getRawParameterValue(params::st(i, "level"))->load();
                const float lv = juce::jlimit(0.f, 1.f, (level + 60.f) / 60.f);
                const float fadeV = juce::jlimit(0.f, 1.2f, m.strength[(size_t) i] / juce::Decibels::decibelsToGain(level));
                const float act = parked ? 0.f : juce::jlimit(0.f, 1.f, m.activity[(size_t) i] * 4.f);
                float h = a.getHeight() * 0.85f * (0.15f + 0.85f * lv) * (0.6f + 0.25f * fadeV + 0.15f * act);
                if (done) h = a.getHeight() * 0.12f;
                const float x = xFor(proc_.stationFreq(i));
                const bool hot = i == tuned;
                juce::Path peak;
                peak.startNewSubPath(x, a.getBottom() - h);
                peak.lineTo(x + 3.f, a.getBottom() - h * 0.45f);
                peak.lineTo(x + 16.f, a.getBottom());
                peak.lineTo(x - 16.f, a.getBottom());
                peak.lineTo(x - 3.f, a.getBottom() - h * 0.45f);
                peak.closeSubPath();
                g.setColour((hot ? amber : cyan).withAlpha(done ? 0.25f : (parked ? 0.55f : 1.f)));
                g.fillPath(peak);
                labels[(size_t) i] = { x, a.getBottom() - h - 15.f, hot, done ? 2 : (parked ? 1 : 0) };
            }

            // Стрелка.
            g.setColour(amber.withAlpha(0.25f));
            g.fillRect(nx - 3.f, a.getY() - 4.f, 6.f, a.getHeight() + 8.f);
            g.setColour(amber);
            g.fillRect(nx - 1.f, a.getY() - 4.f, 2.f, a.getHeight() + 8.f);

            // Номера станций - поверх стрелки; у остановленных и доигравших - значок.
            g.setFont(mono(10.f));
            for (int i = 0; i < kNumStations; ++i)
            {
                const auto& l = labels[(size_t) i];
                if (l.x < 0.f) continue;
                juce::String t(i + 1);
                if (l.state == 1) t += juce::String::fromUTF8("\xe2\x96\xa0");          // ■ стоп
                if (l.state == 2) t += juce::String::fromUTF8("\xe2\x9c\x93");          // ✓ доиграла
                const auto box = juce::Rectangle<float>(l.x - 12.f, l.y, 24.f, 12.f);
                if (l.hot) { g.setColour(band); g.fillRoundedRectangle(box, 3.f); }
                g.setColour(l.hot ? amber : dim);
                g.drawText(t, box, juce::Justification::centred);
            }

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
