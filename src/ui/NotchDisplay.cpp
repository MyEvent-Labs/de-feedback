#include "ui/NotchDisplay.h"

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"

namespace defeedback
{

NotchDisplay::NotchDisplay (DeFeedbackProcessor& processor)
    : proc_ (processor)
{
    startTimerHz (24);
}

void NotchDisplay::timerCallback()
{
    const auto& snap = proc_.getUiSnapshot();
    const int active = juce::jlimit (0, kMaxRows, snap.activeNotches.load());

    count_ = 0;
    for (int i = 0; i < active && count_ < kMaxRows; ++i)
    {
        const float f = snap.notchFreq[i].load();
        const float g = snap.notchGain[i].load();

        if (f > 0.0f && g < -0.05f)
        {
            rows_[(size_t) count_] = { f, g, snap.notchQ[i].load() };
            ++count_;
        }
    }

    repaint();
}

void NotchDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced (6, 4);

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (Palette::textDim);
    g.drawText ("ACTIVE NOTCHES  (" + juce::String (count_) + ")",
                bounds.removeFromTop (12), juce::Justification::centredLeft, false);

    bounds.removeFromTop (2);

    if (count_ == 0)
    {
        g.setColour (Palette::textDim.withAlpha (0.6f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("no notches - signal is clean",
                    bounds, juce::Justification::centredLeft, false);
        return;
    }

    const float rowH = juce::jmin (14.0f, (float) bounds.getHeight() / (float) count_);

    for (int i = 0; i < count_; ++i)
    {
        auto row = bounds.removeFromTop ((int) rowH).reduced (0, 1);
        const auto& r = rows_[(size_t) i];

        auto freqArea = row.removeFromLeft (64);
        auto dbArea = row.removeFromRight (48);
        auto bar = row.reduced (3, 3);

        g.setColour (Palette::text);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.drawText (juce::String (juce::roundToInt (r.freq)) + " Hz",
                    freqArea, juce::Justification::centredLeft, false);

        g.setColour (Palette::panelDark);
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);

        const float t = juce::jlimit (0.0f, 1.0f, std::abs (r.gain) / 18.0f);
        g.setColour (t > 0.5f ? Palette::warning : Palette::accent);
        g.fillRoundedRectangle (bar.toFloat().withWidth ((float) bar.getWidth() * t), 2.0f);

        g.setColour (Palette::textDim);
        g.drawText (juce::String (r.gain, 1) + " dB",
                    dbArea, juce::Justification::centredRight, false);
    }
}

} // namespace defeedback
