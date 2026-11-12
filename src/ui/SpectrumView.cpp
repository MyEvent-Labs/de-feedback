#include "ui/SpectrumView.h"

#include "PluginProcessor.h"
#include "dsp/Parameters.h"
#include "dsp/SpectrumAnalyzer.h"
#include "ui/LookAndFeel.h"

#include <cmath>

namespace defeedback
{

namespace
{
    juce::String formatFrequency (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

SpectrumView::SpectrumView (DeFeedbackProcessor& processor)
    : proc_ (processor),
      notches_ ((size_t) UiSnapshot::kMaxNotchDisplay)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void SpectrumView::resized()
{
    display_.assign ((size_t) juce::jmax (1, getWidth()), minDb);
}

void SpectrumView::timerCallback()
{
    proc_.getAnalyzer().computeSpectrum (mags_);

    // Ease the notch bank so cuts visibly drop in and recover.
    const auto& snap = proc_.getUiSnapshot();
    const int active = juce::jmin (snap.activeNotches.load(), UiSnapshot::kMaxNotchDisplay);

    for (int i = 0; i < UiSnapshot::kMaxNotchDisplay; ++i)
    {
        auto& n = notches_[(size_t) i];
        const float f = i < active ? snap.notchFreq[i].load() : 0.0f;
        float target = 0.0f;

        if (f > 0.0f)
        {
            n.freq = f;
            n.q = juce::jmax (0.5f, snap.notchQ[i].load());
            target = juce::jmin (0.0f, snap.notchGain[i].load());
        }

        n.gain += (target - n.gain) * 0.28f;
    }

    heroDb_ += (snap.reductionDb.load() - heroDb_) * 0.3f;
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const auto plot = full.withTrimmedBottom (18.0f);
    const float w = plot.getWidth();
    const float h = plot.getHeight();
    const int numPixels = juce::jmax (1, (int) w);
    const float contentRight = contentRight_ > 0 ? (float) contentRight_ : plot.getRight() - 16.0f;
    const float logLo = std::log10 (minFreq), logHi = std::log10 (maxFreq);

    auto freqToX = [&] (float f)
    {
        return plot.getX() + (std::log10 (juce::jlimit (minFreq, maxFreq, f)) - logLo) / (logHi - logLo) * w;
    };
    auto xToFreq = [&] (float x) { return std::pow (10.0f, logLo + (x - plot.getX()) / w * (logHi - logLo)); };
    auto dbToY = [&] (float db)
    {
        return plot.getBottom() - (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb) * h;
    };

    // Grid.
    g.setFont (DeFeedbackLookAndFeel::mono (9.5f));
    for (auto p : { std::pair<float, const char*> { 50.0f, "" }, { 100.0f, "100" }, { 200.0f, "" }, { 500.0f, "" },
                    { 1000.0f, "1k" }, { 2000.0f, "" }, { 5000.0f, "" }, { 10000.0f, "10k" } })
    {
        const bool major = p.second[0] != 0;
        const int x = (int) freqToX (p.first);
        g.setColour (Palette::grid.withMultipliedAlpha (major ? 1.0f : 0.5f));
        g.drawVerticalLine (x, plot.getY(), plot.getBottom());

        if (major)
        {
            g.setColour (Palette::textMute);
            g.drawText (p.second, juce::Rectangle<int> (x - 20, (int) plot.getBottom() + 3, 40, 12),
                        juce::Justification::centred, false);
        }
    }

    for (int db = -72; db <= -12; db += 12)
    {
        const int y = (int) dbToY ((float) db);
        g.setColour (Palette::grid);
        g.drawHorizontalLine (y, plot.getX(), plot.getRight());
        g.setColour (Palette::textMute);
        g.drawText (juce::String (db), juce::Rectangle<int> (8, y - 13, 30, 12), juce::Justification::centredLeft, false);
    }

    // Spectrum trace, smoothed and mapped logarithmically.
    const auto& analyzer = proc_.getAnalyzer();
    const int numBins = analyzer.getNumBins();
    const double sr = analyzer.getSampleRate();

    if (numBins > 1 && ! mags_.empty())
    {
        if ((int) display_.size() != numPixels)
            display_.assign ((size_t) numPixels, minDb);

        juce::Path line;

        for (int px = 0; px < numPixels; ++px)
        {
            const double binsPerHz = (double) (numBins - 1) * 2.0 / sr;
            const double p0 = (double) xToFreq (plot.getX() + (float) px) * binsPerHz;
            const double p1 = (double) xToFreq (plot.getX() + (float) (px + 1)) * binsPerHz;

            const int b0 = juce::jlimit (0, numBins - 1, (int) std::floor (p0));
            const int b1 = juce::jlimit (b0, numBins - 1, (int) std::ceil (p1));

            float target = minDb;
            if (b1 - b0 <= 1)
            {
                // Fewer than one bin per pixel (low end): interpolate so the trace stays smooth.
                const int next = juce::jmin (b0 + 1, numBins - 1);
                const float frac = juce::jlimit (0.0f, 1.0f, (float) (p0 - (double) b0));
                target = mags_[(size_t) b0] + (mags_[(size_t) next] - mags_[(size_t) b0]) * frac;
            }
            else
            {
                for (int b = b0; b <= b1; ++b)
                    target = juce::jmax (target, mags_[(size_t) b]);
            }

            float& d = display_[(size_t) px];
            d += (target - d) * (target > d ? 0.55f : 0.12f);

            const float x = plot.getX() + (float) px;
            if (px == 0)
                line.startNewSubPath (x, dbToY (d));
            else
                line.lineTo (x, dbToY (d));
        }

        juce::Path fill (line);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();

        const auto joint = juce::PathStrokeType::curved;

        if (Palette::glow)
        {
            g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.42f), plot.getX(), plot.getY() + h * 0.25f,
                                                     Palette::accent.withAlpha (0.0f), plot.getX(), plot.getBottom(), false));
            g.fillPath (fill);
            g.setColour (Palette::accent.withAlpha (0.14f));
            g.strokePath (line, juce::PathStrokeType (7.0f, joint, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour (Palette::accent.withAlpha (0.07f));
            g.fillPath (fill);
        }

        g.setColour (Palette::accent);
        g.strokePath (line, juce::PathStrokeType (1.7f, joint, juce::PathStrokeType::rounded));
    }

    // Cut curve: what the notch bank is doing to the signal right now.
    const float zeroY = plot.getY() + h * 0.30f;
    const float pxPerDb = h * 0.30f / 18.0f;
    bool cutting = false;
    for (const auto& n : notches_)
        cutting = cutting || n.gain < -0.05f;

    {
        juce::Path curve;
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
        {
            float db = 0.0f;
            if (cutting)
            {
                const float f = xToFreq (x);
                // ponytail: bell approximated as gain / (1 + Q^2 (f/f0 - f0/f)^2); swap in the
                // exact biquad magnitude if the drawn width ever needs to be measurement-grade.
                for (const auto& n : notches_)
                    if (n.gain < -0.05f)
                    {
                        const float d = n.q * (f / n.freq - n.freq / f);
                        db += n.gain / (1.0f + d * d);
                    }
            }

            const float y = zeroY - db * pxPerDb;
            if (x <= plot.getX())
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }

        if (cutting)
        {
            juce::Path area (curve);
            area.lineTo (plot.getRight(), zeroY);
            area.lineTo (plot.getX(), zeroY);
            area.closeSubPath();
            g.setColour (Palette::danger.withAlpha (0.12f));
            g.fillPath (area);
        }

        g.setColour (cutting ? Palette::danger : Palette::textMute.withAlpha (0.45f));
        g.strokePath (curve, juce::PathStrokeType (cutting ? 2.0f : 1.0f, juce::PathStrokeType::curved));

        g.setColour (Palette::textMute);
        g.setFont (DeFeedbackLookAndFeel::caps (9.0f));
        g.drawText ("CUT", juce::Rectangle<int> (46, (int) zeroY - 15, 40, 12), juce::Justification::centredLeft, false);
    }

    // One tag per notch.
    {
        const auto tagFont = DeFeedbackLookAndFeel::mono (10.5f);
        g.setFont (tagFont);
        int row = 0;

        for (const auto& n : notches_)
        {
            if (n.gain > -0.5f || n.freq <= 0.0f)
                continue;

            const float x = freqToX (n.freq);
            const float y = zeroY - n.gain * pxPerDb;

            g.setColour (Palette::danger.withAlpha (0.25f));
            g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
            g.setColour (Palette::danger);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);

            const auto label = formatFrequency (n.freq) + "  " + minusSign() + juce::String (-n.gain, 1) + " dB";
            const float tw = juce::GlyphArrangement::getStringWidth (tagFont, label) + 16.0f;
            juce::Rectangle<float> tag (x - tw * 0.5f, y + 13.0f + (float) (row % 3) * 22.0f, tw, 19.0f);
            tag.setX (juce::jlimit (plot.getX() + 6.0f, juce::jmax (plot.getX() + 6.0f, contentRight - tw), tag.getX()));
            ++row;

            g.setColour (Palette::background.withAlpha (0.88f));
            g.fillRoundedRectangle (tag, 5.0f);
            g.setColour (Palette::danger.withAlpha (0.55f));
            g.drawRoundedRectangle (tag, 5.0f, 1.0f);
            g.setColour (Palette::text);
            g.drawText (label, tag.toNearestInt(), juce::Justification::centred, false);
        }
    }

    // Hero read-out.
    {
        const auto& snap = proc_.getUiSnapshot();
        const int active = snap.activeNotches.load();
        const bool bypassed = proc_.apvts.getRawParameterValue (parameters::bypass)->load() > 0.5f;
        const bool frozen = proc_.apvts.getRawParameterValue (parameters::freeze)->load() > 0.5f;
        const bool protecting = ! bypassed && active > 0 && heroDb_ > 0.05f;

        const juce::Rectangle<int> big ((int) contentRight - 360, 14, 360, 52);
        juce::String status;

        if (protecting)
        {
            auto number = big.withTrimmedRight (38);
            g.setColour (Palette::danger);
            g.setFont (DeFeedbackLookAndFeel::mono (52.0f).withExtraKerningFactor (-0.07f));
            g.drawText (minusSign() + juce::String (heroDb_, 1), number, juce::Justification::centredRight, false);
            g.setColour (Palette::textDim);
            g.setFont (DeFeedbackLookAndFeel::caps (15.0f));
            g.drawText ("dB", big.withTrimmedLeft (big.getWidth() - 32).withTrimmedTop (22), juce::Justification::centredLeft, false);

            status << "PROTECTING  " << middleDot() << "  " << active << (active == 1 ? " NOTCH" : " NOTCHES");
        }
        else
        {
            g.setColour (bypassed ? Palette::textDim : Palette::text.withAlpha (0.9f));
            g.setFont (DeFeedbackLookAndFeel::caps (26.0f));
            g.drawText (bypassed ? "BYPASSED" : "LISTENING", big.withTrimmedTop (10), juce::Justification::centredRight, false);
            status = bypassed ? "PROTECTION OFF" : "NO FEEDBACK DETECTED";
        }

        if (frozen && ! bypassed)
            status << "  " << middleDot() << "  FROZEN";

        const auto statusFont = DeFeedbackLookAndFeel::caps (11.0f);
        const juce::Rectangle<int> line (big.getX(), big.getBottom() + 4, big.getWidth(), 14);
        g.setFont (statusFont);
        g.setColour (Palette::textDim);
        g.drawText (status, line, juce::Justification::centredRight, false);

        const float dotX = (float) line.getRight() - juce::GlyphArrangement::getStringWidth (statusFont, status) - 14.0f;
        g.setColour (bypassed ? Palette::textMute : (protecting ? Palette::danger : Palette::accent));
        g.fillEllipse (dotX, (float) line.getCentreY() - 3.5f, 7.0f, 7.0f);
    }
}

} // namespace defeedback
