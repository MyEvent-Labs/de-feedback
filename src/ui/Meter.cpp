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
    auto bounds = getLocalBounds();

    auto drawLevel = [&] (juce::Rectangle<int> row, const juce::String& name,
                          float peakDb, float rmsDb, float holdDb, juce::Colour colour)
    {
        auto labelArea = row.removeFromLeft (32);
        auto valueArea = row.removeFromRight (58);
        auto bar = row.reduced (2, 5);

        g.setColour (Palette::textDim);
        g.setFont (DeFeedbackLookAndFeel::typeface (10.0f, true));
        g.drawText (name, labelArea, juce::Justification::centredLeft, false);

        g.setColour (Palette::background);
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);

        auto dbToX = [&] (float db) -> float
        {
            const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            return bar.getX() + t * (float) bar.getWidth();
        };

        juce::Rectangle<float> fill (bar.getX(), (float) bar.getY(), dbToX (peakDb) - (float) bar.getX(), (float) bar.getHeight());
        g.setColour (peakDb > -1.0f ? Palette::danger : colour);
        g.fillRoundedRectangle (fill, 3.0f);

        const float rmsX = dbToX (rmsDb);
        g.setColour (Palette::text.withAlpha (0.7f));
        g.fillRect (rmsX - 1.0f, (float) bar.getY(), 1.5f, (float) bar.getHeight());

        const float holdX = dbToX (holdDb);
        g.setColour (Palette::text);
        g.fillRect (holdX - 1.0f, (float) bar.getY(), 1.5f, (float) bar.getHeight());

        g.setColour (Palette::text);
        g.setFont (DeFeedbackLookAndFeel::typeface (10.0f));
        g.drawText (juce::String (juce::roundToInt (peakDb)) + " dB", valueArea, juce::Justification::centredRight, false);
    };

    auto drawGR = [&] (juce::Rectangle<int> row)
    {
        auto labelArea = row.removeFromLeft (32);
        auto valueArea = row.removeFromRight (58);
        auto bar = row.reduced (2, 5);

        g.setColour (Palette::textDim);
        g.setFont (DeFeedbackLookAndFeel::typeface (10.0f, true));
        g.drawText ("GR", labelArea, juce::Justification::centredLeft, false);

        g.setColour (Palette::background);
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);

        const float t = juce::jlimit (0.0f, 1.0f, gr_ / 18.0f);
        g.setColour (t > 0.66f ? Palette::danger : Palette::warning);
        g.fillRoundedRectangle (bar.toFloat().withWidth ((float) bar.getWidth() * t), 3.0f);

        g.setColour (Palette::text);
        g.setFont (DeFeedbackLookAndFeel::typeface (10.0f));
        g.drawText ("-" + juce::String (gr_, 1) + " dB", valueArea, juce::Justification::centredRight, false);
    };

    const int rowH = bounds.getHeight() / 3;
    drawLevel (bounds.removeFromTop (rowH), "IN", inPeak_, inRms_, inHold_, Palette::accentDim);
    drawLevel (bounds.removeFromTop (rowH), "OUT", outPeak_, outRms_, outHold_, Palette::accent);
    drawGR (bounds);
}

} // namespace defeedback
