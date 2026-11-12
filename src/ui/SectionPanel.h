#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace defeedback
{

/** A rounded surface with a small upper-case title. Add child controls and
    position them within getContentBounds(). */
class SectionPanel : public juce::Component
{
public:
    explicit SectionPanel (juce::String title) : title_ (std::move (title)) {}

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff11141c));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (juce::Colour (0xff242b3a));
        g.drawRoundedRectangle (b, 8.0f, 1.0f);

        if (title_.isNotEmpty())
        {
            g.setColour (juce::Colour (0xff8993a8));
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (title_, getLocalBounds().reduced (12, 8).removeFromTop (12),
                        juce::Justification::centredLeft, false);
        }
    }

    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().withTrimmedTop (title_.isEmpty() ? 8 : 26).reduced (12, 8);
    }

private:
    juce::String title_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionPanel)
};

} // namespace defeedback
