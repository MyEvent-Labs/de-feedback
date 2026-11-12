#include "ui/LookAndFeel.h"

#include "BinaryData.h"

namespace defeedback
{

// Defaults are the Scope skin; setSkin() overwrites all of them.
juce::Colour Palette::background { 0xff06070a };
juce::Colour Palette::screen     { 0xff06070a };
juce::Colour Palette::surface    { 0xff0e1014 };
juce::Colour Palette::surfaceHi  { 0xff181b22 };
juce::Colour Palette::outline    { 0xff262b35 };
juce::Colour Palette::text       { 0xfff2f4ee };
juce::Colour Palette::textDim    { 0xff8b9286 };
juce::Colour Palette::textMute   { 0xff555b54 };
juce::Colour Palette::accent     { 0xffc8ff3d };
juce::Colour Palette::accentDim  { 0xff5d7a1a };
juce::Colour Palette::warning    { 0xffffb23e };
juce::Colour Palette::danger     { 0xffff5a4d };
juce::Colour Palette::grid       { 0x12ffffff };
bool Palette::glow = true;

namespace
{
    Skin currentSkin = Skin::scope;
    bool skinChosen = false;

    juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "De-Feedback";
        o.folderName = "MyEvent Labs";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
}

Skin Palette::skin() { return currentSkin; }

void Palette::setSkin (Skin s, bool remember)
{
    // ponytail: process-wide state, so every open instance shares one skin; editors that are
    // already open pick up the colours but only fully restyle when reopened. Broadcast a
    // change message if live multi-window switching ever matters.
    currentSkin = s;
    skinChosen = true;

    struct Tokens { juce::uint32 background, screen, surface, surfaceHi, outline, text, textDim, textMute,
                                 accent, accentDim, warning, danger, grid; bool glow; };

    static const Tokens table[] = {
        // scope
        { 0xff06070a, 0xff06070a, 0xff0e1014, 0xff181b22, 0xff262b35, 0xfff2f4ee, 0xff8b9286, 0xff555b54,
          0xffc8ff3d, 0xff5d7a1a, 0xffffb23e, 0xffff5a4d, 0x12ffffff, true },
        // console
        { 0xff211e1b, 0xff0b0a07, 0xff2b2723, 0xff39342e, 0xff4d463e, 0xffe8decd, 0xffa0968a, 0xff6b6357,
          0xffffb000, 0xff8a5f00, 0xffffd27a, 0xffff6142, 0x16ffb000, true },
        // instrument
        { 0xffe9e7e0, 0xfff6f5f0, 0xffe1ded6, 0xffd3d0c6, 0xff17171a, 0xff17171a, 0xff66635b, 0xff9a978e,
          0xff17171a, 0xff17171a, 0xffb86a00, 0xffff4f1a, 0x1617171a, false },
    };

    const auto& t = table[(int) s];
    background = juce::Colour (t.background);  screen    = juce::Colour (t.screen);
    surface    = juce::Colour (t.surface);     surfaceHi = juce::Colour (t.surfaceHi);
    outline    = juce::Colour (t.outline);     text      = juce::Colour (t.text);
    textDim    = juce::Colour (t.textDim);     textMute  = juce::Colour (t.textMute);
    accent     = juce::Colour (t.accent);      accentDim = juce::Colour (t.accentDim);
    warning    = juce::Colour (t.warning);     danger    = juce::Colour (t.danger);
    grid       = juce::Colour (t.grid);        glow      = t.glow;

    if (remember)
    {
        juce::PropertiesFile settings (settingsOptions());
        settings.setValue ("skin", (int) s);
        settings.saveIfNeeded();
    }
}

void Palette::loadSavedSkin()
{
    if (skinChosen)
        return;

    juce::PropertiesFile settings (settingsOptions());
    setSkin ((Skin) juce::jlimit (0, 2, settings.getIntValue ("skin", 0)), false);
}

namespace
{
    juce::Font fontFrom (const juce::Typeface::Ptr& face, float height)
    {
        return juce::Font (juce::FontOptions (face).withHeight (height));
    }
}

juce::Font DeFeedbackLookAndFeel::typeface (float height, bool bold)
{
    static const auto medium = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf,
                                                                        (size_t) BinaryData::BarlowSemiCondensedMedium_ttfSize);
    static const auto semiBold = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedSemiBold_ttf,
                                                                          (size_t) BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    return fontFrom (bold ? semiBold : medium, height);
}

juce::Font DeFeedbackLookAndFeel::caps (float height)
{
    return typeface (height, true).withExtraKerningFactor (0.14f);
}

juce::Font DeFeedbackLookAndFeel::mono (float height)
{
    static const auto face = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoMedium_ttf,
                                                                      (size_t) BinaryData::JetBrainsMonoMedium_ttfSize);
    return fontFrom (face, height);
}

DeFeedbackLookAndFeel::DeFeedbackLookAndFeel() { refreshColours(); }

void DeFeedbackLookAndFeel::refreshColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::TextButton::buttonColourId, Palette::surfaceHi);
    setColour (juce::TextButton::buttonOnColourId, Palette::accent);
    setColour (juce::TextButton::textColourOffId, Palette::text.withAlpha (0.82f));
    setColour (juce::TextButton::textColourOnId, Palette::background);
    setColour (juce::PopupMenu::backgroundColourId, Palette::surface);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::background);
    setColour (juce::ToggleButton::textColourId, Palette::text);
    setColour (juce::ToggleButton::tickColourId, Palette::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Palette::outline);
    setColour (juce::AlertWindow::backgroundColourId, Palette::surface);
    setColour (juce::AlertWindow::textColourId, Palette::text);
    setColour (juce::AlertWindow::outlineColourId, Palette::outline);
    setColour (juce::HyperlinkButton::textColourId, Palette::accent);
}

void DeFeedbackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                              juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto trackW = juce::jmax (3.0f, radius * 0.12f);
    const auto arcR = radius - trackW;
    const bool enabled = slider.isEnabled();
    const bool hot = enabled && slider.isMouseOverOrDragging();
    const auto rounded = juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // Value read-out in the middle of the knob: number on top, unit beneath.
    auto drawValue = [&] (float innerRadius, juce::Colour numberColour, juce::Colour unitColour)
    {
        const auto valueText = slider.getTextFromValue (slider.getValue());
        const auto number = valueText.contains (" ") ? valueText.upToLastOccurrenceOf (" ", false, false) : valueText;
        const auto unit = valueText.contains (" ") ? valueText.fromLastOccurrenceOf (" ", false, false) : juce::String();

        const auto inner = juce::Rectangle<float> (innerRadius * 1.5f, innerRadius * 1.5f).withCentre (centre);
        const auto numberH = juce::jlimit (10.0f, 19.0f, innerRadius * 0.52f);
        auto numberArea = inner.withSizeKeepingCentre (inner.getWidth(), numberH + 2.0f);
        if (unit.isNotEmpty())
            numberArea.translate (0.0f, -numberH * 0.28f);

        g.setColour (enabled ? numberColour : Palette::textMute);
        g.setFont (mono (numberH));
        g.drawFittedText (number, numberArea.toNearestInt(), juce::Justification::centred, 1, 0.7f);

        if (unit.isNotEmpty())
        {
            g.setColour (unitColour);
            g.setFont (caps (juce::jlimit (8.0f, 11.0f, innerRadius * 0.30f)));
            g.drawText (unit.toUpperCase(), numberArea.translated (0.0f, numberH * 0.95f).toNearestInt(),
                        juce::Justification::centred, false);
        }
    };

    auto drawTicks = [&] (int count, float inner, float outer, float thickness, bool lightUp)
    {
        for (int i = 0; i <= count; ++i)
        {
            const auto a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * (float) i / (float) count;
            const auto major = i % 5 == 0;
            const auto p0 = centre.getPointOnCircumference (radius * inner, a);
            const auto p1 = centre.getPointOnCircumference (radius * (major ? outer : outer - 0.05f), a);
            g.setColour (lightUp && enabled && a <= angle + 0.001f ? Palette::accent : Palette::textMute);
            g.drawLine (p0.x, p0.y, p1.x, p1.y, thickness);
        }
    };

    if (Palette::skin() == Skin::console)
    {
        // Machined metal cap inside a ring of tick marks that light up to the value.
        drawTicks (20, 0.86f, 1.0f, 1.3f, true);

        const auto capR = radius * 0.74f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre.translated (0.0f, 2.5f)));

        juce::ColourGradient metal (juce::Colour (0xff958d80), centre.x - capR * 0.6f, centre.y - capR * 0.8f,
                                    juce::Colour (0xff14110e), centre.x + capR * 0.7f, centre.y + capR, false);
        metal.addColour (0.45, juce::Colour (hot ? 0xff4a443c : 0xff3d3832));
        g.setGradientFill (metal);
        g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre));

        const auto faceR = capR * 0.78f;
        g.setColour (juce::Colour (0xff26221e));
        g.fillEllipse (juce::Rectangle<float> (faceR * 2.0f, faceR * 2.0f).withCentre (centre));
        g.setColour (juce::Colour (0xff5a534a));
        g.drawEllipse (juce::Rectangle<float> (faceR * 2.0f, faceR * 2.0f).withCentre (centre), 0.8f);

        const auto p0 = centre.getPointOnCircumference (capR * 0.80f, angle);
        const auto p1 = centre.getPointOnCircumference (capR * 0.99f, angle);
        g.setColour (hot ? Palette::accent : Palette::text);
        g.drawLine (juce::Line<float> (p0, p1), 2.6f);

        drawValue (faceR * 0.92f, Palette::accent, Palette::textDim);
        return;
    }

    if (Palette::skin() == Skin::instrument)
    {
        // Solid ink disc with a single orange marker.
        drawTicks (10, 0.88f, 1.0f, 1.1f, false);

        const auto discR = radius * 0.76f;
        g.setColour (hot ? Palette::text.brighter (0.25f) : Palette::text);
        g.fillEllipse (juce::Rectangle<float> (discR * 2.0f, discR * 2.0f).withCentre (centre));

        const auto dot = centre.getPointOnCircumference (discR * 0.84f, angle);
        const auto dotR = discR * (hot ? 0.10f : 0.085f);
        g.setColour (enabled ? Palette::danger : Palette::textMute);
        g.fillEllipse (dot.x - dotR, dot.y - dotR, dotR * 2.0f, dotR * 2.0f);

        drawValue (discR * 0.80f, Palette::background, Palette::background.withAlpha (0.6f));
        return;
    }

    // Scope: thin glowing ring.
    // Track.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (hot ? Palette::outline.brighter (0.15f) : Palette::outline);
    g.strokePath (track, rounded);

    if (enabled)
    {
        // Value arc with a soft glow that swells when the knob is touched.
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (Palette::accent.withAlpha (hot ? 0.22f : 0.10f));
        g.strokePath (value, juce::PathStrokeType (trackW * 2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Palette::accent);
        g.strokePath (value, rounded);

        // Handle.
        const auto handle = centre.getPointOnCircumference (arcR, angle);
        const auto handleR = trackW * (hot ? 1.15f : 0.9f);
        g.setColour (Palette::text);
        g.fillEllipse (handle.x - handleR, handle.y - handleR, handleR * 2.0f, handleR * 2.0f);
    }

    drawValue (arcR, Palette::text, Palette::textDim);
}

void DeFeedbackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& backgroundColour,
                                                  bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto on = button.getToggleState();
    const auto onColour = button.findColour (juce::TextButton::buttonOnColourId);

    if (on)
    {
        g.setColour (shouldDrawButtonAsHighlighted ? onColour.brighter (0.15f) : onColour);
        g.fillRoundedRectangle (bounds, Palette::radiusControl);
        return;
    }

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour (shouldDrawButtonAsDown ? backgroundColour.brighter (0.12f) : backgroundColour);
        g.fillRoundedRectangle (bounds, Palette::radiusControl);
    }

    g.setColour (shouldDrawButtonAsHighlighted ? Palette::textMute : Palette::outline);
    g.drawRoundedRectangle (bounds, Palette::radiusControl, 1.0f);
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
    g.setFont (caps (11.0f));
    g.drawText (button.getButtonText().toUpperCase(),
                button.getLocalBounds().withTrimmedLeft ((int) boxSize + 9),
                juce::Justification::centredLeft, false);
}

juce::Font DeFeedbackLookAndFeel::getLabelFont (juce::Label& label)         { return label.getFont(); }
juce::Font DeFeedbackLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return caps (11.5f); }
juce::Font DeFeedbackLookAndFeel::getPopupMenuFont()                         { return typeface (15.0f); }
juce::Font DeFeedbackLookAndFeel::getAlertWindowTitleFont()                  { return typeface (18.0f, true); }
juce::Font DeFeedbackLookAndFeel::getAlertWindowMessageFont()                { return typeface (15.0f); }

} // namespace defeedback
