#include "ui/LookAndFeel.h"

namespace defeedback
{

const juce::Colour Palette::background { 0xff0a0c11 };
const juce::Colour Palette::surface    { 0xff11141c };
const juce::Colour Palette::surfaceHi  { 0xff1a1f2b };
const juce::Colour Palette::outline    { 0xff242b3a };
const juce::Colour Palette::text       { 0xffedeff5 };
const juce::Colour Palette::textDim    { 0xff8993a8 };
const juce::Colour Palette::textMute   { 0xff555f75 };
const juce::Colour Palette::accent     { 0xff35e0c3 };
const juce::Colour Palette::accentDim  { 0xff1b8e7b };
const juce::Colour Palette::warning    { 0xfff2b441 };
const juce::Colour Palette::danger     { 0xfff2545b };
const juce::Colour Palette::grid       { 0xff1a2130 };

juce::Font DeFeedbackLookAndFeel::typeface (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

DeFeedbackLookAndFeel::DeFeedbackLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, Palette::surfaceHi);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId, Palette::background);
    setColour (juce::ComboBox::backgroundColourId, Palette::surfaceHi);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::outlineColourId, Palette::outline);
    setColour (juce::PopupMenu::backgroundColourId, Palette::surface);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accentDim);
    setColour (juce::ToggleButton::textColourId, Palette::text);
    setColour (juce::ToggleButton::tickColourId, Palette::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Palette::outline);
}

void DeFeedbackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                              juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (7.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto trackW = juce::jmax (3.0f, radius * 0.16f);
    const auto arcR = radius - trackW * 0.5f;

    // Inner disc.
    g.setColour (Palette::surfaceHi);
    g.fillEllipse (centre.x - radius * 0.72f, centre.y - radius * 0.72f, radius * 1.44f, radius * 1.44f);

    // Track.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Palette::outline);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc.
    if (slider.isEnabled())
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (Palette::accent);
        g.strokePath (value, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer.
    const auto pointerLength = radius * 0.52f;
    juce::Path pointer;
    pointer.startNewSubPath (0.0f, -radius * 0.30f);
    pointer.lineTo (0.0f, -radius * 0.30f - pointerLength);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (slider.isEnabled() ? Palette::text : Palette::textMute);
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DeFeedbackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& backgroundColour,
                                                  bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto on = button.getToggleState();

    auto fill = on ? Palette::accent : backgroundColour;
    if (shouldDrawButtonAsDown)
        fill = fill.brighter (0.12f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.06f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, Palette::radiusControl);

    g.setColour (on ? Palette::accent : Palette::outline);
    g.drawRoundedRectangle (bounds, Palette::radiusControl, 1.0f);
}

void DeFeedbackLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (Palette::surfaceHi);
    g.fillRoundedRectangle (bounds, Palette::radiusControl);
    g.setColour (box.isEnabled() ? Palette::outline : Palette::surface);
    g.drawRoundedRectangle (bounds, Palette::radiusControl, 1.0f);

    juce::Path arrow;
    const auto cx = (float) width - 14.0f;
    const auto cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (Palette::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DeFeedbackLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                              bool shouldDrawButtonAsHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto boxSize = juce::jmin (16.0f, bounds.getHeight());
    const juce::Rectangle<float> box (bounds.getX(), bounds.getCentreY() - boxSize * 0.5f, boxSize, boxSize);

    g.setColour (button.getToggleState() ? Palette::accent : Palette::surfaceHi);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (shouldDrawButtonAsHighlighted ? Palette::textDim : Palette::outline);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);

    if (button.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + boxSize * 0.28f, box.getCentreY());
        tick.lineTo (box.getCentreX() - boxSize * 0.02f, box.getBottom() - boxSize * 0.30f);
        tick.lineTo (box.getRight() - boxSize * 0.26f, box.getY() + boxSize * 0.30f);
        g.setColour (Palette::background);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (Palette::textDim);
    g.setFont (typeface (12.0f));
    g.drawText (button.getButtonText(),
                button.getLocalBounds().withTrimmedLeft ((int) boxSize + 8),
                juce::Justification::centredLeft, false);
}

juce::Font DeFeedbackLookAndFeel::getLabelFont (juce::Label& label)          { return typeface (label.getFont().getHeight()); }
juce::Font DeFeedbackLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return typeface (juce::jlimit (11.0f, 14.0f, (float) h * 0.42f), true); }
juce::Font DeFeedbackLookAndFeel::getComboBoxFont (juce::ComboBox&)          { return typeface (12.0f); }
juce::Font DeFeedbackLookAndFeel::getPopupMenuFont()                          { return typeface (12.5f); }

} // namespace defeedback
