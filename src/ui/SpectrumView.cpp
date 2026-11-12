#include "ui/SpectrumView.h"

#include "PluginProcessor.h"
#include "dsp/SpectrumAnalyzer.h"
#include "ui/LookAndFeel.h"

#include <cmath>

namespace defeedback
{

SpectrumView::SpectrumView (DeFeedbackProcessor& processor)
    : proc_ (processor)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void SpectrumView::timerCallback()
{
    if (proc_.getAnalyzer().computeSpectrum (mags_))
        repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::background);
    g.fillRoundedRectangle (bounds, 5.0f);

    const auto plot = bounds.reduced (6.0f, 8.0f);
    const float w = plot.getWidth();
    const float h = plot.getHeight();

    auto freqToX = [&] (float f)
    {
        const float lo = std::log10 (minFreq);
        const float hi = std::log10 (maxFreq);
        const float t = (std::log10 (juce::jlimit (minFreq, maxFreq, f)) - lo) / (hi - lo);
        return plot.getX() + t * w;
    };

    auto dbToY = [&] (float db)
    {
        const float t = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return plot.getBottom() - t * h;
    };

    // Grid.
    g.setColour (Palette::grid);
    static const float freqs[] { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    for (auto f : freqs)
        g.drawVerticalLine ((int) freqToX (f), plot.getY(), plot.getBottom());

    for (int db = -80; db <= 0; db += 20)
        g.drawHorizontalLine ((int) dbToY ((float) db), plot.getX(), plot.getRight());

    // Spectrum trace.
    const auto& analyzer = proc_.getAnalyzer();
    const int numBins = analyzer.getNumBins();
    const double sr = analyzer.getSampleRate();

    if (numBins > 1 && ! mags_.empty())
    {
        juce::Path path;
        bool started = false;

        for (int px = 0; px <= (int) w; ++px)
        {
            const float t = (float) px / juce::jmax (1.0f, w);
            const float f = std::pow (10.0f, std::log10 (minFreq) + t * (std::log10 (maxFreq) - std::log10 (minFreq)));
            const int bin = juce::jlimit (0, numBins - 1, (int) std::llround ((double) f / sr * (double) (numBins - 1) * 2.0));

            if (bin >= (int) mags_.size())
                break;

            const float y = dbToY (mags_[(size_t) bin]);
            const float x = plot.getX() + px;

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        g.setColour (Palette::accent.withAlpha (0.55f));
        g.strokePath (path, juce::PathStrokeType (1.4f));
    }

    // Notch markers.
    const auto& snap = proc_.getUiSnapshot();
    const int active = snap.activeNotches.load();
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (int i = 0; i < juce::jmin (active, UiSnapshot::kMaxNotchDisplay); ++i)
    {
        const float f = snap.notchFreq[i].load();
        const float gain = snap.notchGain[i].load();
        if (f <= 0.0f || gain >= -0.05f)
            continue;

        const float x = freqToX (f);
        const float depth = juce::jlimit (0.0f, 18.0f, -gain) / 18.0f;
        const float top = plot.getY() + depth * h * 0.0f;
        const float bottom = plot.getY() + 8.0f + depth * (h - 8.0f);

        g.setColour (Palette::warning.withAlpha (0.9f));
        g.drawVerticalLine ((int) x, top, bottom);

        g.setColour (Palette::textDim);
        g.drawText (juce::String (juce::roundToInt (f)) + " Hz",
                    juce::Rectangle<int> ((int) x - 26, (int) bottom + 1, 52, 11),
                    juce::Justification::centred, false);
    }

    // Risk read-out.
    const float risk = snap.risk.load();
    if (risk > 0.01f)
    {
        g.setColour (Palette::warning.withAlpha (0.35f));
        g.fillRect (plot.getX(), plot.getY(), w * juce::jlimit (0.0f, 1.0f, risk), 3.0f);
    }

    // Protection status pill.
    {
        const juce::String label = active > 0 ? ("PROTECTING " + juce::String (active)) : "CLEAN";
        const auto colour = active > 0 ? Palette::warning : Palette::accent;
        g.setFont (DeFeedbackLookAndFeel::typeface (10.5f, true));
        const int w = juce::jmax (66, 20 + (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label));
        juce::Rectangle<int> pill (getWidth() - w - 12, 12, w, 19);
        g.setColour (colour.withAlpha (0.15f));
        g.fillRoundedRectangle (pill.toFloat(), 4.0f);
        g.setColour (colour);
        g.drawText (label, pill, juce::Justification::centred, false);
    }
}

} // namespace defeedback
