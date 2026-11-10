#include "TestRegistry.h"

#include "dsp/AdaptiveNotchBank.h"
#include "dsp/FeedbackDetector.h"

#include <cmath>
#include <vector>

namespace defeedback::tests
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct LoopResult
    {
        int    detectedAtBlock = -1;
        int    maxNotches = 0;
        float  maxAbsOutput = 0.0f;
        bool   allFinite = true;
        int    blocks = 0;
    };

    // Simulated feedback loop: broadband noise plus a narrow tone whose gain
    // ramps up over time (feedback onset). The detector + bank run in the
    // loop; we measure when the first notch appears and whether output stays
    // bounded.
    LoopResult runFeedbackLoop (double fs, int block, bool stereo, double toneHz, double onsetSeconds)
    {
        DetectorConfig detCfg;
        detCfg.minProminenceDb = 8.0f;
        detCfg.onFrames = 4;
        detCfg.onConfidence = 0.5f;

        FeedbackDetector det;
        det.prepare (fs, stereo ? 2 : 1, detCfg);

        AdaptiveNotchBank::Config bankCfg;
        bankCfg.linked = stereo;
        bankCfg.maxCutDb = 9.0f;
        AdaptiveNotchBank bank;
        bank.prepare (fs, block, stereo ? 2 : 1);
        bank.setConfig (bankCfg);

        LoopResult result;
        const int totalBlocks = (int) (fs * 4.0 / block);
        const int onsetBlock = (int) (fs * onsetSeconds / block);

        std::vector<float> left ((size_t) block), right ((size_t) block);
        juce::Random rng (12345);

        for (int b = 0; b < totalBlocks; ++b)
        {
            const double t0 = (double) (b * block) / fs;

            for (int i = 0; i < block; ++i)
            {
                const double t = t0 + (double) i / fs;
                const double ramp = juce::jlimit (0.0, 1.0, (t - onsetSeconds) / 0.5);
                const double tone = 0.9 * ramp * std::sin (2.0 * kPi * toneHz * t);
                const double noise = 0.05 * (rng.nextDouble() * 2.0 - 1.0);
                left[(size_t) i] = (float) (tone + noise);
                right[(size_t) i] = left[(size_t) i];
            }

            const float* analysis[1] { left.data() };
            det.process (analysis, 1, block, true);

            std::vector<DetectedPeak> peaks;
            for (int i = 0; i < det.getNumPeaks(); ++i)
                peaks.push_back (det.getPeak (i));

            bank.update (peaks.data(), (int) peaks.size());

            float* ptrs[2] { left.data(), right.data() };
            bank.process (ptrs, stereo ? 2 : 1, block);

            result.maxNotches = juce::jmax (result.maxNotches, bank.getActiveCount());

            if (result.detectedAtBlock < 0 && bank.getActiveCount() > 0 && b >= onsetBlock)
                result.detectedAtBlock = b;

            for (int i = 0; i < block; ++i)
            {
                const float v = std::abs (left[(size_t) i]);
                result.maxAbsOutput = juce::jmax (result.maxAbsOutput, v);
                if (! std::isfinite (left[(size_t) i]) || ! std::isfinite (right[(size_t) i]))
                    result.allFinite = false;
            }

            ++result.blocks;
        }

        return result;
    }

    class RegressionTests : public juce::UnitTest
    {
    public:
        RegressionTests() : juce::UnitTest ("DSPRegression", "defeedback") {}

        void runTest() override
        {
            beginTest ("feedback onset is detected and suppressed within a bound");
            {
                for (double fs : { 44100.0, 48000.0 })
                {
                    auto r = runFeedbackLoop (fs, 64, true, 3200.0, 0.5);
                    expect (r.allFinite, "non-finite output");
                    expect (r.detectedAtBlock >= 0, "no detection at fs=" + juce::String (fs));
                    expect (r.maxNotches >= 1, "no notch deployed");
                    expect (r.maxAbsOutput < 4.0f, "unbounded output " + juce::String (r.maxAbsOutput));
                }
            }

            beginTest ("sample rate / block size matrix stays bounded");
            {
                for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0 })
                {
                    for (int block : { 32, 64, 128, 256, 512, 1024 })
                    {
                        auto r = runFeedbackLoop (fs, block, true, 2500.0, 0.25);
                        expect (r.allFinite, "non-finite fs=" + juce::String (fs) + " block=" + juce::String (block));
                        expect (r.maxAbsOutput < 4.0f, "unbounded fs=" + juce::String (fs) + " block=" + juce::String (block));
                    }
                }
            }

            beginTest ("mono and stereo both remain bounded");
            {
                auto mono = runFeedbackLoop (48000.0, 128, false, 1800.0, 0.5);
                expect (mono.allFinite, "mono non-finite");
                expect (mono.maxAbsOutput < 4.0f, "mono unbounded");

                auto stereo = runFeedbackLoop (48000.0, 128, true, 1800.0, 0.5);
                expect (stereo.allFinite, "stereo non-finite");
                expect (stereo.maxAbsOutput < 4.0f, "stereo unbounded");
            }

            beginTest ("clean multi-tone material does not trigger excessive notches");
            {
                const double fs = 48000.0;
                const int block = 128;

                DetectorConfig detCfg;
                detCfg.minProminenceDb = 12.0f;
                detCfg.onFrames = 8;
                detCfg.onConfidence = 0.62f; // conservative profile

                FeedbackDetector det;
                det.prepare (fs, 1, detCfg);

                AdaptiveNotchBank::Config bankCfg;
                bankCfg.maxCutDb = 9.0f * 0.7f;
                bankCfg.maxNotchesPerLane = 12;
                AdaptiveNotchBank bank;
                bank.prepare (fs, block, 1);
                bank.setConfig (bankCfg);

                std::vector<float> buf ((size_t) block);
                const double tones[] { 196.0, 294.0, 392.0, 587.33 };
                int maxNotches = 0;

                for (int b = 0; b < (int) (fs * 3.0 / block); ++b)
                {
                    for (int i = 0; i < block; ++i)
                    {
                        const double t = (double) (b * block + i) / fs;
                        double v = 0.0;
                        for (auto f : tones)
                            v += 0.2 * std::sin (2.0 * kPi * f * t);
                        buf[(size_t) i] = (float) v;
                    }

                    const float* analysis[1] { buf.data() };
                    det.process (analysis, 1, block, true);

                    std::vector<DetectedPeak> peaks;
                    for (int i = 0; i < det.getNumPeaks(); ++i)
                        peaks.push_back (det.getPeak (i));

                    bank.update (peaks.data(), (int) peaks.size());
                    float* ptrs[1] { buf.data() };
                    bank.process (ptrs, 1, block);

                    maxNotches = juce::jmax (maxNotches, bank.getActiveCount());
                }

                expect (maxNotches <= 4, "over-triggered: " + juce::String (maxNotches) + " notches");
            }
        }
    };
}

juce::UnitTest* regressionTests()
{
    static RegressionTests tests;
    return &tests;
}

} // namespace defeedback::tests
