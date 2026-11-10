#include "ui/Meter.h"

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"

#include <algorithm>

namespace defeedback
{

Meter::Meter (DeFeedbackProcessor& processor)
    : proc_ (processor)
{
    startTimerHz (24);
}

void Meter::timerCallback()
{
    const auto& snap = proc_.getUiSnapshot();

    inPeak_  = snap.inputLevelDb.load();
    inRms_   = snap.inputRmsDb.load();
    outPeak_ = snap.outputLevelDb.load();
    outRms_  = snap.outputRmsDb.load();
    gr_      = snap.reductionDb.load();

    inHold_  = juce::jmax (inPeak_, inHold_ - 0.8f);
    outHold_ = juce::jmax (outPeak_, outHold_ - 0.8f);

    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced (6, 4);

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (Palette::textDim);
    g.drawText ("METERS", bounds.removeFromTop (12), juce::Justification::centredLeft, false);

    // Level meters share a -60..0 dB scale.
    auto drawLevel = [&] (juce::Rectangle<int> row, const juce::String& name,
                          float peakDb, float rmsDb, float holdDb, juce::Colour colour)
    {
        auto labelArea = row.removeFromLeft (30);
        auto valueArea = row.removeFromRight (56);
        auto bar = row.reduced (2, 3);

        g.setColour (Palette::textDim);
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (name, labelArea, juce::Justification::centredLeft, false);

        g.setColour (Palette::panelDark);
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);

        auto dbToX = [&] (float db) -> float
        {
            const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            return bar.getX() + t * (float) bar.getWidth();
        };

        // Peak fill.
        juce::Rectangle<float> fill (bar.getX(), (float) bar.getY(), dbToX (peakDb) - (float) bar.getX(), (float) bar.getHeight());
        g.setColour (peakDb > -1.0f ? Palette::danger : colour);
        g.fillRoundedRectangle (fill, 2.0f);

        // RMS marker.
        const float rmsX = dbToX (rmsDb);
        g.setColour (Palette::text.withAlpha (0.75f));
        g.fillRect (rmsX - 1.0f, (float) bar.getY(), 1.5f, (float) bar.getHeight());

        // Peak-hold marker.
        const float holdX = dbToX (holdDb);
        g.setColour (Palette::text);
        g.fillRect (holdX - 1.0f, (float) bar.getY(), 1.5f, (float) bar.getHeight());

        g.setColour (Palette::text);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.drawText (juce::String (juce::roundToInt (peakDb)) + " dB",
                    valueArea, juce::Justification::centredRight, false);
    };

    // Gain-reduction meter (0..18 dB down the bar).
    auto drawGR = [&] (juce::Rectangle<int> row)
    {
        auto labelArea = row.removeFromLeft (30);
        auto valueArea = row.removeFromRight (56);
        auto bar = row.reduced (2, 3);

        g.setColour (Palette::textDim);
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText ("GR", labelArea, juce::Justification::centredLeft, false);

        g.setColour (Palette::panelDark);
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);

        const float t = juce::jlimit (0.0f, 1.0f, gr_ / 18.0f);
        juce::Rectangle<float> fill (bar.toFloat().withWidth ((float) bar.getWidth() * t));
        g.setColour (t > 0.66f ? Palette::danger : Palette::warning);
        g.fillRoundedRectangle (fill, 2.0f);

        g.setColour (Palette::text);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.drawText ("-" + juce::String (gr_, 1) + " dB",
                    valueArea, juce::Justification::centredRight, false);
    };

    bounds.removeFromTop (2);
    const int rowH = bounds.getHeight() / 3;
    drawLevel (bounds.removeFromTop (rowH), "IN", inPeak_, inRms_, inHold_, Palette::accentDim);
    drawLevel (bounds.removeFromTop (rowH), "OUT", outPeak_, outRms_, outHold_, Palette::accent);
    drawGR (bounds);
}

} // namespace defeedback
