#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace defeedback
{

/** MyEvent Labs colour system. One controlled accent, dark neutral base. */
struct Palette
{
    static const juce::Colour background;
    static const juce::Colour panel;
    static const juce::Colour panelDark;
    static const juce::Colour outline;
    static const juce::Colour text;
    static const juce::Colour textDim;
    static const juce::Colour accent;
    static const juce::Colour accentDim;
    static const juce::Colour warning;
    static const juce::Colour danger;
    static const juce::Colour grid;
};

class DeFeedbackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DeFeedbackLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

} // namespace defeedback
