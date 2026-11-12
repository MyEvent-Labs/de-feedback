/*
    Offscreen UI screenshot tool.

    Boots the real DeFeedbackProcessor + editor with no host, feeds a little
    audio so the metering/notch state is populated, pumps the message loop, and
    writes PNG snapshots.

    Usage: defeedback_screenshot [outputDir]
*/

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    // Feeds audio into the processor so the snapshot (meters, notches, level)
    // is populated. toneHz <= 0 feeds low-level silence-ish noise.
    void feed (defeedback::DeFeedbackProcessor& p, double toneHz, double seconds)
    {
        const double fs = 48000.0;
        const int block = 512;
        const int total = (int) (seconds * fs / block);
        juce::MidiBuffer midi;
        juce::Random rng (7);

        for (int b = 0; b < total; ++b)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (b * block + i) / fs;
                float v;
                if (toneHz > 0.0)
                {
                    const double ramp = juce::jlimit (0.0, 1.0, t / 0.4);
                    v = 0.35f * (float) (ramp * std::sin (2.0 * kPi * toneHz * t))
                      + 0.004f * (float) (rng.nextDouble() * 2.0 - 1.0);
                }
                else
                {
                    v = 0.004f * (float) (rng.nextDouble() * 2.0 - 1.0);
                }
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            p.processBlock (buf, midi);
        }
    }

    bool render (defeedback::DeFeedbackProcessor& p, int w, int h, const juce::File& file)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        if (editor == nullptr)
            return false;

        editor->setSize (w, h);
        editor->setVisible (true);

        // Let the child timers tick so meters/spectrum/notch list populate.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);

        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        if (! stream.openedOk())
            return false;

        juce::PNGImageFormat png;
        const bool ok = png.writeImageToStream (image, stream);
        std::printf ("%s (%dx%d @2x)\n", ok ? "wrote" : "FAILED", w, h);
        return ok;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1]))
                                       : juce::File::getCurrentWorkingDirectory().getChildFile ("docs/screenshots");

    // Protect an active, on-screen snapshot.
    {
        defeedback::DeFeedbackProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        feed (proc, 1000.0, 2.2);
        render (proc, 1080, 700, outDir.getChildFile ("de-feedback-active-1080x700.png"));
    }

    // Idle / clean snapshot at a second size.
    {
        defeedback::DeFeedbackProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        feed (proc, -1.0, 0.6);
        render (proc, 960, 640, outDir.getChildFile ("de-feedback-clean-960x640.png"));
    }

    // Compact view.
    {
        defeedback::DeFeedbackProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        feed (proc, 2000.0, 2.0);
        render (proc, 900, 600, outDir.getChildFile ("de-feedback-compact-900x600.png"));
    }

    return 0;
}
