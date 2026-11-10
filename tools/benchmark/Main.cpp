/*
    Offline DSP benchmark for De-Feedback.

    Measures wall-clock cost of the detector + adaptive notch bank across
    common sample rates and block sizes, and reports the real-time factor.
    This intentionally excludes the host / audio-device layer.

    Usage: defeedback_benchmark [secondsPerCase]
*/

#include <juce_dsp/juce_dsp.h>

#include "dsp/AdaptiveNotchBank.h"
#include "dsp/FeedbackDetector.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double runCase (double fs, int block, double seconds)
    {
        defeedback::FeedbackDetector detector;
        detector.prepare (fs, 2, defeedback::DetectorConfig {});

        defeedback::AdaptiveNotchBank bank;
        bank.prepare (fs, block, 2);

        std::vector<float> left ((size_t) block), right ((size_t) block);
        juce::Random rng (5);
        const int totalBlocks = (int) (fs * seconds / block);

        const auto start = juce::Time::getHighResolutionTicks();

        std::vector<defeedback::DetectedPeak> peaks;

        for (int b = 0; b < totalBlocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (b * block + i) / fs;
                const double v = 0.2 * std::sin (2.0 * kPi * 1000.0 * t)
                               + 0.1 * std::sin (2.0 * kPi * 3000.0 * t)
                               + 0.05 * (rng.nextDouble() * 2.0 - 1.0);
                left[(size_t) i] = (float) v;
                right[(size_t) i] = (float) v;
            }

            const float* analysis[1] { left.data() };
            detector.process (analysis, 1, block, true);

            peaks.clear();
            for (int i = 0; i < detector.getNumPeaks(); ++i)
                peaks.push_back (detector.getPeak (i));

            bank.update (peaks.data(), (int) peaks.size());
            float* ptrs[2] { left.data(), right.data() };
            bank.process (ptrs, 2, block);
        }

        const auto elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        return elapsed;
    }
}

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? juce::String (argv[1]).getDoubleValue() : 5.0;

    std::printf ("De-Feedback DSP benchmark (%.1f s per case, stereo)\n", seconds);
    std::printf ("%-10s %-8s %-12s %-12s\n", "fs", "block", "wall(s)", "RT factor");

    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        for (int block : { 32, 64, 128, 256, 512, 1024 })
        {
            const double wall = runCase (fs, block, seconds);
            const double rt = seconds / juce::jmax (1.0e-9, wall);
            std::printf ("%-10.0f %-8d %-12.4f %-12.1fx\n", fs, block, wall, rt);
        }
    }

    return 0;
}
