#include "ui/LookAndFeel.h"

namespace defeedback
{

const juce::Colour Palette::background  { 0xff0e1015 };
const juce::Colour Palette::panel       { 0xff191c23 };
const juce::Colour Palette::panelDark   { 0xff12141a };
const juce::Colour Palette::outline     { 0xff2a2f3a };
const juce::Colour Palette::text        { 0xffe6e8ee };
const juce::Colour Palette::textDim     { 0xff8b93a3 };
const juce::Colour Palette::accent      { 0xff2fd4b0 };
const juce::Colour Palette::accentDim   { 0xff1d7f6c };
const juce::Colour Palette::warning     { 0xffe0b341 };
const juce::Colour Palette::danger      { 0xffe0555f };
const juce::Colour Palette::grid        { 0xff232833 };

DeFeedbackLookAndFeel::DeFeedbackLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, Palette::panel);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId, Palette::background);
    setColour (juce::ComboBox::backgroundColourId, Palette::panel);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::outlineColourId, Palette::outline);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accentDim);
}

void DeFeedbackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                              juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW = juce::jmax (2.0f, radius * 0.14f);

    g.setColour (Palette::outline);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, lineW * 0.6f);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - lineW * 0.5f, radius - lineW * 0.5f,
                       0.0f, rotaryStartAngle, angle, true);
    g.setColour (slider.isEnabled() ? Palette::accent : Palette::textDim);
    g.strokePath (arc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    const auto pointerLength = radius * 0.62f;
    pointer.startNewSubPath (0.0f, -pointerLength * 0.2f);
    pointer.lineTo (0.0f, -pointerLength);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (Palette::text);
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (2.0f, lineW * 0.6f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on ? Palette::accent : Palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void DeFeedbackLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.isEnabled() ? Palette::outline : Palette::panelDark);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Path arrow;
    const auto cx = (float) width - 14.0f;
    const auto cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (Palette::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font DeFeedbackLookAndFeel::getLabelFont (juce::Label& label)
{
    return juce::Font (juce::FontOptions (label.getFont().getHeight()));
}

juce::Font DeFeedbackLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jlimit (11.0f, 15.0f, (float) buttonHeight * 0.42f)));
}

} // namespace defeedback
