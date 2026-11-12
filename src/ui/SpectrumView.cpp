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

void SpectrumView::resized()
{
    display_.assign ((size_t) juce::jmax (1, getWidth()), minDb);
}

void SpectrumView::timerCallback()
{
    if (proc_.getAnalyzer().computeSpectrum (mags_))
        repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const auto plot = full.reduced (10.0f, 12.0f);
    const float w = plot.getWidth();
    const float h = plot.getHeight();
    const int numPixels = juce::jmax (1, (int) w);

    // Column background.
    g.setColour (Palette::background);
    g.fillRoundedRectangle (full, 5.0f);

    auto freqToX = [&] (float f)
    {
        const float lo = std::log10 (minFreq), hi = std::log10 (maxFreq);
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
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) freqToX (f), plot.getY(), plot.getBottom());
    for (int db = -72; db <= 0; db += 12)
        g.drawHorizontalLine ((int) dbToY ((float) db), plot.getX(), plot.getRight());

    // Grid labels.
    g.setColour (Palette::textMute);
    g.setFont (DeFeedbackLookAndFeel::typeface (9.0f));
    for (auto p : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
        g.drawText (p.second, juce::Rectangle<int> ((int) freqToX (p.first) - 20, (int) plot.getBottom() + 1, 40, 11),
                    juce::Justification::centred, false);

    // Spectrum trace, smoothed and mapped logarithmically.
    const auto& analyzer = proc_.getAnalyzer();
    const int numBins = analyzer.getNumBins();
    const double sr = analyzer.getSampleRate();

    if (numBins > 1 && ! mags_.empty())
    {
        if ((int) display_.size() != numPixels)
            display_.assign ((size_t) numPixels, minDb);

        juce::Path fill, line;
        line.startNewSubPath (plot.getX(), plot.getBottom());
        fill.startNewSubPath (plot.getX(), plot.getBottom());

        for (int px = 0; px < numPixels; ++px)
        {
            const float t0 = (float) px / (float) numPixels;
            const float t1 = (float) (px + 1) / (float) numPixels;
            const float f0 = std::pow (10.0f, std::log10 (minFreq) + t0 * (std::log10 (maxFreq) - std::log10 (minFreq)));
            const float f1 = std::pow (10.0f, std::log10 (minFreq) + t1 * (std::log10 (maxFreq) - std::log10 (minFreq)));

            const int b0 = juce::jlimit (0, numBins - 1, (int) std::floor ((double) f0 / sr * (double) (numBins - 1) * 2.0));
            const int b1 = juce::jlimit (b0, numBins - 1, (int) std::ceil ((double) f1 / sr * (double) (numBins - 1) * 2.0));

            float target = minDb;
            for (int b = b0; b <= b1; ++b)
                target = juce::jmax (target, mags_[(size_t) b]);

            float& d = display_[(size_t) px];
            d += (target - d) * (target > d ? 0.55f : 0.12f);

            const float x = plot.getX() + (float) px;
            const float y = dbToY (d);
            line.lineTo (x, y);
            fill.lineTo (x, y);
        }

        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.28f), plot.getX(), plot.getY(),
                                                 Palette::accent.withAlpha (0.02f), plot.getX(), plot.getBottom(), false));
        g.fillPath (fill);

        g.setColour (Palette::accent);
        g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Notch markers.
    const auto& snap = proc_.getUiSnapshot();
    const int active = snap.activeNotches.load();
    g.setFont (DeFeedbackLookAndFeel::typeface (9.5f, true));

    for (int i = 0; i < juce::jmin (active, UiSnapshot::kMaxNotchDisplay); ++i)
    {
        const float f = snap.notchFreq[i].load();
        const float gain = snap.notchGain[i].load();
        if (f <= 0.0f || gain >= -0.05f)
            continue;

        const float x = freqToX (f);
        const float depth = juce::jlimit (0.0f, 18.0f, -gain) / 18.0f;
        const float bottom = plot.getY() + 6.0f + depth * (h - 6.0f);

        g.setColour (Palette::warning.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, plot.getY(), bottom);

        g.setColour (Palette::warning);
        g.fillEllipse (x - 2.5f, plot.getY() - 1.0f, 5.0f, 5.0f);

        g.setColour (Palette::textDim);
        g.drawText (juce::String (juce::roundToInt (f)) + " Hz",
                    juce::Rectangle<int> ((int) x - 28, (int) bottom + 2, 56, 11),
                    juce::Justification::centred, false);
    }

    // Protection status pill.
    {
        const juce::String label = active > 0 ? ("PROTECTING " + juce::String (active)) : "CLEAN";
        const auto colour = active > 0 ? Palette::warning : Palette::accent;
        g.setFont (DeFeedbackLookAndFeel::typeface (10.5f, true));
        const int pw = juce::jmax (66, 20 + (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label));
        juce::Rectangle<int> pill ((int) plot.getRight() - pw - 8, (int) plot.getY() + 8, pw, 19);
        g.setColour (colour.withAlpha (0.16f));
        g.fillRoundedRectangle (pill.toFloat(), 4.0f);
        g.setColour (colour);
        g.drawText (label, pill, juce::Justification::centred, false);
    }
}

} // namespace defeedback
