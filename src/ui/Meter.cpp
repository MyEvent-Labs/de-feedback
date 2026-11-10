#include "ui/Meter.h"

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"

namespace defeedback
{

Meter::Meter (DeFeedbackProcessor& processor)
    : proc_ (processor)
{
    startTimerHz (20);
}

void Meter::timerCallback()
{
    const auto& snap = proc_.getUiSnapshot();
    inputDb_  = snap.inputLevelDb.load();
    outputDb_ = snap.outputLevelDb.load();
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    auto drawBar = [&] (juce::Rectangle<int> area, float db, const juce::String& name, juce::Colour colour)
    {
        g.setColour (Palette::textDim);
        g.drawText (name, area.removeFromLeft (34), juce::Justification::centredLeft, false);

        auto bar = area.reduced (0, 3);
        g.setColour (Palette::panelDark);
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto filled = bar.toFloat().withWidth (bar.getWidth() * norm);

        g.setColour (db > -1.0f ? Palette::danger : colour);
        g.fillRoundedRectangle (filled, 3.0f);

        g.setColour (Palette::textDim);
        g.drawText (juce::String (juce::roundToInt (db)) + " dB",
                    area.removeFromRight (44), juce::Justification::centredRight, false);
    };

    auto top = bounds.removeFromTop (bounds.getHeight() / 2);
    drawBar (top, inputDb_, "IN", Palette::accentDim);
    drawBar (bounds, outputDb_, "OUT", Palette::accent);
}

} // namespace defeedback
