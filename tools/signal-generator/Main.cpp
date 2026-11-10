/*
    Offline, reproducible test-signal generator for De-Feedback.

    Produces a directory of WAV fixtures used by manual review and by future
    automated analysis. Everything is synthesised; no third-party audio is
    bundled.

    Usage: defeedback_signal_generator [outputDir] [sampleRate]
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Biquad.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    using Signal = std::vector<float>;

    void writeWav (const juce::File& file, const Signal& left, const Signal& right, double sampleRate)
    {
        file.deleteFile();
        auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
        if (stream == nullptr)
            return;

        juce::WavAudioFormat format;
        const bool stereo = ! right.empty();
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.release(), sampleRate, stereo ? 2u : 1u, 24, {}, 0));

        if (writer == nullptr)
            return;

        juce::AudioBuffer<float> buffer (stereo ? 2 : 1, (int) left.size());
        buffer.copyFrom (0, 0, left.data(), (int) left.size());
        if (stereo)
            buffer.copyFrom (1, 0, right.data(), (int) right.size());

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    Signal makeSweep (double fs, double seconds, bool logarithmic)
    {
        const int n = (int) (fs * seconds);
        Signal s ((size_t) n);
        const double f0 = logarithmic ? 20.0 : 50.0;
        const double f1 = 20000.0;

        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            const double u = t / seconds;
            const double phase = logarithmic
                ? 2.0 * kPi * f0 * seconds * (std::pow (f1 / f0, u) - 1.0) / std::log (f1 / f0)
                : 2.0 * kPi * (f0 * t + 0.5 * (f1 - f0) * t * t / seconds);
            s[(size_t) i] = 0.5f * (float) std::sin (phase);
        }

        return s;
    }

    Signal makeNoise (double fs, double seconds, bool pink, int seed)
    {
        const int n = (int) (fs * seconds);
        Signal s ((size_t) n);
        juce::Random rng (seed);

        // Simple pink filter via one-pole cascade (Voss-ish); adequate for fixtures.
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

        for (int i = 0; i < n; ++i)
        {
            const double white = rng.nextDouble() * 2.0 - 1.0;

            if (pink)
            {
                b0 = 0.99886 * b0 + white * 0.0555179;
                b1 = 0.99332 * b1 + white * 0.0750759;
                b2 = 0.96900 * b2 + white * 0.1538520;
                b3 = 0.86650 * b3 + white * 0.3104856;
                b4 = 0.55000 * b4 + white * 0.5329522;
                b5 = -0.7616 * b5 - white * 0.0168980;
                const double pinkv = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
                b6 = white * 0.115926;
                s[(size_t) i] = (float) (pinkv * 0.11);
            }
            else
            {
                s[(size_t) i] = (float) (white * 0.3);
            }
        }

        return s;
    }

    // A resonant feedback loop: broadband excitation circulating through a
    // narrow resonator and a short delay with near-unity gain, producing a
    // realistic narrow-band buildup.
    Signal makeFeedbackLoop (double fs, double seconds, double resonanceHz, double loopGain, bool slowBuildup)
    {
        const int n = (int) (fs * seconds);
        Signal s ((size_t) n, 0.0f);

        const int delay = juce::jmax (1, (int) (fs * 0.002)); // 2 ms loop
        Signal line ((size_t) delay, 0.0f);
        int idx = 0;

        defeedback::Biquad resonator;
        resonator.setCoeffs (defeedback::Biquad::makePeak (fs, resonanceHz, 30.0, 12.0));
        resonator.reset();

        juce::Random rng (99);

        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            const double gain = slowBuildup ? (0.9 + 0.1 * (t / seconds)) : juce::jmax (0.0, loopGain);
            const double excitation = 0.02 * (rng.nextDouble() * 2.0 - 1.0);

            const double delayed = line[(size_t) idx];
            double v = excitation + delayed * gain;
            double shaped = resonator.processSample (v);
            shaped = juce::jlimit (-1.0, 1.0, shaped);

            line[(size_t) idx] = (float) shaped;
            idx = (idx + 1) % delay;

            s[(size_t) i] = (float) shaped;
        }

        return s;
    }

    Signal makeSustainedTone (double fs, double seconds, double freq)
    {
        const int n = (int) (fs * seconds);
        Signal s ((size_t) n);
        for (int i = 0; i < n; ++i)
            s[(size_t) i] = 0.4f * (float) std::sin (2.0 * kPi * freq * (double) i / fs);
        return s;
    }

    Signal makeMultiTone (double fs, double seconds)
    {
        const int n = (int) (fs * seconds);
        Signal s ((size_t) n, 0.0f);
        const double tones[] { 196.0, 246.94, 293.66, 392.0, 523.25 };
        for (int i = 0; i < n; ++i)
        {
            double v = 0.0;
            for (auto f : tones)
                v += 0.12 * std::sin (2.0 * kPi * f * (double) i / fs);
            s[(size_t) i] = (float) v;
        }
        return s;
    }

    Signal makeTransients (double fs, double seconds)
    {
        const int n = (int) (fs * seconds);
        Signal s ((size_t) n, 0.0f);
        juce::Random rng (7);
        for (int i = 0; i < n; ++i)
        {
            if (i % (int) (fs * 0.35) == 0)
                for (int k = 0; k < 64 && i + k < n; ++k)
                    s[(size_t) (i + k)] = (float) ((rng.nextDouble() * 2.0 - 1.0) * std::exp (-k / 8.0));
        }
        return s;
    }
}

int main (int argc, char** argv)
{
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1]))
                                       : juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures");
    outDir.createDirectory();

    const double fs = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 48000.0;

    std::printf ("Generating De-Feedback fixtures at %.0f Hz into %s\n", fs, outDir.getFullPathName().toRawUTF8());

    writeWav (outDir.getChildFile ("sine_sweep.wav"), makeSweep (fs, 5.0, false), {}, fs);
    writeWav (outDir.getChildFile ("log_sweep.wav"), makeSweep (fs, 5.0, true), {}, fs);
    writeWav (outDir.getChildFile ("white_noise.wav"), makeNoise (fs, 5.0, false, 1), {}, fs);
    writeWav (outDir.getChildFile ("pink_noise.wav"), makeNoise (fs, 5.0, true, 2), {}, fs);
    writeWav (outDir.getChildFile ("sustained_tone.wav"), makeSustainedTone (fs, 5.0, 1000.0), {}, fs);
    writeWav (outDir.getChildFile ("multi_tone.wav"), makeMultiTone (fs, 5.0), {}, fs);
    writeWav (outDir.getChildFile ("transients.wav"), makeTransients (fs, 5.0), {}, fs);
    writeWav (outDir.getChildFile ("silence.wav"), Signal ((size_t) (fs * 2.0), 0.0f), {}, fs);

    // Stereo feedback fixtures.
    auto fb = makeFeedbackLoop (fs, 6.0, 3200.0, 1.25, false);
    writeWav (outDir.getChildFile ("feedback_sudden.wav"), fb, fb, fs);

    auto slow = makeFeedbackLoop (fs, 8.0, 1500.0, 1.02, true);
    writeWav (outDir.getChildFile ("feedback_slow.wav"), slow, slow, fs);

    std::printf ("Done.\n");
    return 0;
}
