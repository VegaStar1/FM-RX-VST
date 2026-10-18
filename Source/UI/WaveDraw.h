#pragma once
// Рисование волны станции из Peaks (мин/макс по корзинам) для окна времени [t0, t1].
#include "Stations/Waveform.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace fmrx::ui
{
    inline void drawPeaks(juce::Graphics& g, const Peaks& p, int channel, juce::Rectangle<float> r,
                          double t0, double t1, juce::Colour c)
    {
        if (p.buckets <= 0 || p.durationSec <= 0.0 || t1 <= t0 || r.getWidth() < 1.f) return;
        const auto& mn = p.mn[(size_t) juce::jlimit(0, 1, channel)];
        const auto& mx = p.mx[(size_t) juce::jlimit(0, 1, channel)];
        const float mid = r.getCentreY(), half = r.getHeight() * 0.5f;
        const int w = (int) r.getWidth();
        juce::RectangleList<float> bars;
        for (int x = 0; x < w; ++x)
        {
            const double ta = t0 + (t1 - t0) * x / w, tb = t0 + (t1 - t0) * (x + 1) / w;
            int ka = (int) (ta / p.durationSec * p.buckets), kb = (int) (tb / p.durationSec * p.buckets);
            if (kb <= 0 || ka >= p.buckets) continue;
            ka = juce::jlimit(0, p.buckets - 1, ka);
            kb = juce::jlimit(ka + 1, p.buckets, kb);
            float lo = 0.f, hi = 0.f;
            for (int k = ka; k < kb; ++k) { lo = juce::jmin(lo, mn[(size_t) k]); hi = juce::jmax(hi, mx[(size_t) k]); }
            const float y0 = mid - juce::jmin(1.f, hi) * half, y1 = mid - juce::jmax(-1.f, lo) * half;
            bars.addWithoutMerging({ r.getX() + (float) x, y0, 1.f, juce::jmax(1.f, y1 - y0) });
        }
        g.setColour(c);
        g.fillRectList(bars);
    }

    inline juce::String formatTime(double sec)
    {
        sec = juce::jmax(0.0, sec);
        const int m = (int) (sec / 60.0);
        const double s = sec - m * 60.0;
        return juce::String(m) + ":" + juce::String(s, 3).paddedLeft('0', 6);
    }

    // "1:02.5", "62.5", "0:02.560" -> секунды; < 0 - не разобрали.
    inline double parseTime(const juce::String& text)
    {
        auto t = text.trim().replaceCharacter(',', '.');
        const int firstDigit = t.indexOfAnyOf("0123456789");
        if (firstDigit < 0) return -1.0;
        t = t.substring(firstDigit);                                     // «НАЧАЛО  0:02.5» -> «0:02.5»
        if (t.containsChar(':'))
        {
            const auto m = t.upToFirstOccurrenceOf(":", false, false).getIntValue();
            const auto s = t.fromFirstOccurrenceOf(":", false, false).getDoubleValue();
            return m * 60.0 + s;
        }
        return t.containsAnyOf("0123456789") ? t.getDoubleValue() : -1.0;
    }
}
