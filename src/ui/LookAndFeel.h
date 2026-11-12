#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace defeedback
{

/** One layout, three skins. A skin swaps the colour tokens below plus the knob,
    meter and trace drawing style.
      scope      - near-black, lime signal, coral cuts (default)
      console    - warm rack gear: metal knobs, LED ladders, amber trace
      instrument - ink on paper with a single orange accent */
enum class Skin { scope, console, instrument };

/** MyEvent Labs design tokens for the active skin. `accent` is the signal
    colour, `danger` is feedback and every cut the plugin makes. */
struct Palette
{
    static juce::Colour background;
    static juce::Colour screen;      // behind the analyser
    static juce::Colour surface;
    static juce::Colour surfaceHi;
    static juce::Colour outline;
    static juce::Colour text;
    static juce::Colour textDim;
    static juce::Colour textMute;
    static juce::Colour accent;
    static juce::Colour accentDim;
    static juce::Colour warning;
    static juce::Colour danger;
    static juce::Colour grid;
    static bool glow;                // soft bloom on traces and knobs

    static Skin skin();

    /** Switches every token. `remember` stores the choice per user on this computer. */
    static void setSkin (Skin, bool remember);

    /** Restores the remembered skin, once per process (a no-op after any setSkin). */
    static void loadSavedSkin();

    static constexpr float radiusPanel   = 14.0f;
    static constexpr float radiusControl = 8.0f;
};

/** UTF-8 glyphs used in read-outs (kept as byte escapes so the source stays ASCII). */
inline juce::String minusSign()  { return juce::String::fromUTF8 ("\xe2\x88\x92"); }
inline juce::String middleDot()  { return juce::String::fromUTF8 ("\xc2\xb7"); }

class DeFeedbackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DeFeedbackLookAndFeel();

    /** Re-reads the palette into JUCE's colour IDs; call after a skin change. */
    void refreshColours();

    /** Shared type ramp, all from fonts embedded in the binary. */
    static juce::Font typeface (float height, bool bold = false);  // Barlow Semi Condensed
    static juce::Font caps (float height);                         // bold + tracked, for UPPERCASE labels
    static juce::Font mono (float height);                         // JetBrains Mono, for numbers

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
};

} // namespace defeedback
