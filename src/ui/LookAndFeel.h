#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace defeedback
{

/** MyEvent Labs design tokens: a single dark surface system with one accent. */
struct Palette
{
    static const juce::Colour background;
    static const juce::Colour surface;
    static const juce::Colour surfaceHi;
    static const juce::Colour outline;
    static const juce::Colour text;
    static const juce::Colour textDim;
    static const juce::Colour textMute;
    static const juce::Colour accent;
    static const juce::Colour accentDim;
    static const juce::Colour warning;
    static const juce::Colour danger;
    static const juce::Colour grid;

    static constexpr float radiusPanel   = 8.0f;
    static constexpr float radiusControl = 5.0f;
};

class DeFeedbackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DeFeedbackLookAndFeel();

    /** Shared type ramp. */
    static juce::Font typeface (float height, bool bold = false);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

} // namespace defeedback
