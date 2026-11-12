#include "ui/Meter.h"

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"

#include <cmath>

namespace defeedback
{

Meter::Meter (DeFeedbackProcessor& processor)
    : proc_ (processor)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void Meter::timerCallback()
{
    const auto& snap = proc_.getUiSnapshot();

    // Instant attack, eased release, slow-falling peak hold.
    inPeak_  = juce::jmax (snap.inputLevelDb.load(), inPeak_ - 1.5f);
    outPeak_ = juce::jmax (snap.outputLevelDb.load(), outPeak_ - 1.5f);
    inRms_   = snap.inputRmsDb.load();
    outRms_  = snap.outputRmsDb.load();
    inHold_  = juce::jmax (inPeak_, inHold_ - 0.35f);
    outHold_ = juce::jmax (outPeak_, outHold_ - 0.35f);
    gr_     += (snap.reductionDb.load() - gr_) * 0.35f;

    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    // Floating panel, so the analyser can run behind the meters without clashing.
    const auto panel = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::surface.withAlpha (0.92f));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (panel, 10.0f, 1.0f);

    auto area = getLocalBounds().reduced (9, 10);
    auto footer = area.removeFromBottom (30);
    auto scale = area.removeFromLeft (22);
    footer.removeFromLeft (22);

    const int gap = 5;
    const int barW = (area.getWidth() - gap * 2) / 3;
    const auto top = (float) area.getY();
    const auto height = (float) area.getHeight();

    auto dbToY = [&] (float db)
    {
        return top + height * (1.0f - juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f));
    };

    // dB scale.
    g.setFont (DeFeedbackLookAndFeel::mono (9.0f));
    g.setColour (Palette::textMute);
    for (int db = 0; db >= -48; db -= 12)
    {
        const int y = juce::jlimit (area.getY() + 5, area.getBottom() - 5, (int) dbToY ((float) db));
        g.drawText (juce::String (-db), scale.withY (y - 5).withHeight (10).withTrimmedRight (5),
                    juce::Justification::centredRight, false);
    }

    auto drawFooter = [&] (juce::Rectangle<int> column, const juce::String& name, const juce::String& value)
    {
        column = column.expanded (gap / 2 + 2, 0);
        g.setColour (Palette::textDim);
        g.setFont (DeFeedbackLookAndFeel::caps (9.5f));
        g.drawText (name, column.removeFromTop (16).withTrimmedTop (4), juce::Justification::centred, false);
        g.setColour (Palette::text);
        g.setFont (DeFeedbackLookAndFeel::mono (9.0f));
        g.drawText (value, column, juce::Justification::centred, false);
    };

    auto nextBar = [&] { auto bar = area.removeFromLeft (barW); area.removeFromLeft (gap); return bar; };
    auto nextFooter = [&] { auto col = footer.removeFromLeft (barW); footer.removeFromLeft (gap); return col; };

    auto clipToBar = [&] (juce::Rectangle<float> bar)
    {
        juce::Path shape;
        shape.addRoundedRectangle (bar, 3.0f);
        g.setColour (Palette::surfaceHi);
        g.fillPath (shape);
        g.reduceClipRegion (shape);
    };

    // Console skin: LED ladder. `lit` says whether the segment centred on `db` is on.
    const bool ladder = Palette::skin() == Skin::console;
    auto drawLadder = [&] (juce::Rectangle<float> bar, auto&& colourFor)
    {
        const int segments = juce::jmax (8, (int) (bar.getHeight() / 6.0f));
        const float segH = bar.getHeight() / (float) segments;

        for (int i = 0; i < segments; ++i)
        {
            const float db = -60.0f * ((float) i + 0.5f) / (float) segments;   // top segment first
            const auto seg = bar.withY (bar.getY() + segH * (float) i).withHeight (segH - 1.5f);
            const juce::Colour c = colourFor (db, i, segments);
            g.setColour (c);
            g.fillRoundedRectangle (seg, 1.2f);
        }
    };

    auto drawLevel = [&] (const juce::String& name, float peakDb, float rmsDb, float holdDb)
    {
        const auto bar = nextBar().toFloat();

        if (ladder)
        {
            drawLadder (bar, [&] (float db, int, int)
            {
                const auto on = db > -3.0f ? Palette::danger : (db > -12.0f ? Palette::accent : juce::Colour (0xff59e26b));
                const bool isHold = std::abs (db - holdDb) < 1.5f && holdDb > -59.0f;
                return db <= rmsDb || isHold ? on : (db <= peakDb ? on.withAlpha (0.5f) : Palette::surfaceHi);
            });
        }
        else
        {
            juce::Graphics::ScopedSaveState state (g);
            clipToBar (bar);

            // The gradient is anchored to the whole bar, so colour tracks level.
            const juce::ColourGradient ramp (Palette::accent, bar.getX(), bar.getY(),
                                             Palette::accentDim, bar.getX(), bar.getBottom(), false);
            g.setGradientFill (ramp);
            g.setOpacity (0.45f);
            g.fillRect (bar.withTop (dbToY (peakDb)));
            g.setGradientFill (ramp);
            g.setOpacity (1.0f);
            g.fillRect (bar.withTop (dbToY (rmsDb)));

            if (peakDb > -1.0f)
            {
                g.setColour (Palette::danger);
                g.fillRect (bar.withHeight (5.0f));
            }

            if (holdDb > -59.0f)
            {
                g.setColour (Palette::text);
                g.fillRect (bar.withY (dbToY (holdDb) - 1.0f).withHeight (2.0f));
            }
        }

        drawFooter (nextFooter(), name, peakDb > -60.0f ? juce::String (juce::roundToInt (peakDb)) : juce::String ("--"));
    };

    drawLevel ("IN", inPeak_, inRms_, inHold_);
    drawLevel ("OUT", outPeak_, outRms_, outHold_);

    // Gain reduction hangs down from the top.
    {
        const auto bar = nextBar().toFloat();

        if (ladder)
        {
            drawLadder (bar, [&] (float, int i, int segments)
            {
                return (float) i + 0.5f < gr_ / 18.0f * (float) segments ? Palette::danger : Palette::surfaceHi;
            });
        }
        else
        {
            juce::Graphics::ScopedSaveState state (g);
            clipToBar (bar);
            g.setColour (Palette::danger);
            g.fillRect (bar.withHeight (height * juce::jlimit (0.0f, 1.0f, gr_ / 18.0f)));
        }

        drawFooter (nextFooter(), "GR", gr_ > 0.05f ? "-" + juce::String (gr_, 1) : juce::String ("0.0"));
    }
}

} // namespace defeedback
