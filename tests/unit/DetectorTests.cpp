#include "TestRegistry.h"

#include "dsp/FeedbackDetector.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

namespace defeedback::tests
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct DetectorRun
    {
        std::vector<DetectedPeak> peaks;
        int frames = 0;
    };

    DetectorRun runDetector (const std::vector<float>& signal, double sampleRate, const DetectorConfig& config)
    {
        FeedbackDetector detector;
        detector.prepare (sampleRate, 2, config);

        DetectorRun run;
        const int block = 256;
        const float* ptrs[1] { nullptr };

        for (int pos = 0; pos < (int) signal.size(); pos += block)
        {
            const int n = juce::jmin (block, (int) signal.size() - pos);
            ptrs[0] = signal.data() + pos;
            detector.process (ptrs, 1, n, true);
            ++run.frames;
        }

        for (int i = 0; i < detector.getNumPeaks(); ++i)
            run.peaks.push_back (detector.getPeak (i));

        return run;
    }

    std::vector<float> sine (double freq, double sampleRate, double seconds, float amp)
    {
        std::vector<float> s ((size_t) (sampleRate * seconds));
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = amp * (float) std::sin (2.0 * kPi * freq * (double) i / sampleRate);
        return s;
    }

    class DetectorTests : public juce::UnitTest
    {
    public:
        DetectorTests() : juce::UnitTest ("Detector", "defeedback") {}

        void runTest() override
        {
            const double fs = 48000.0;

            beginTest ("detects a sustained narrow tone");
            {
                DetectorConfig cfg;
                cfg.minProminenceDb = 8.0f;
                cfg.minFrequencyHz = 100.0f;
                cfg.minQ = 5.0f;

                auto run = runDetector (sine (1000.0, fs, 2.0, 0.5f), fs, cfg);
                expect (run.peaks.size() >= 1, "no peaks detected");

                if (! run.peaks.empty())
                {
                    const auto& p = run.peaks.front();
                    expect (std::abs (p.frequencyHz - 1000.0f) < 40.0f,
                            "freq=" + juce::String (p.frequencyHz));
                    expect (p.confidence > 0.3f, "confidence=" + juce::String (p.confidence));
                }
            }

            beginTest ("silence produces no peaks and no NaN");
            {
                std::vector<float> silence ((size_t) (fs * 1.0), 0.0f);
                auto run = runDetector (silence, fs, DetectorConfig {});
                expect (run.peaks.empty(), "peaks on silence");

                for (const auto& p : run.peaks)
                    expect (std::isfinite (p.frequencyHz) && std::isfinite (p.confidence), "non-finite peak");
            }

            beginTest ("denormal / hostile input stays finite");
            {
                std::vector<float> hostile ((size_t) (fs * 0.5));
                for (size_t i = 0; i < hostile.size(); ++i)
                    hostile[i] = (i % 7 == 0) ? std::numeric_limits<float>::quiet_NaN() : 1.0e-30f;

                auto run = runDetector (hostile, fs, DetectorConfig {});
                for (const auto& p : run.peaks)
                    expect (std::isfinite (p.frequencyHz) && std::isfinite (p.confidence), "non-finite peak");
            }

            beginTest ("persistence delays activation");
            {
                DetectorConfig cfg;
                cfg.onFrames = 20;
                cfg.minProminenceDb = 8.0f;

                FeedbackDetector detector;
                detector.prepare (fs, 1, cfg);

                const auto sig = sine (1000.0, fs, 0.05, 0.5f);
                const float* ptrs[1] { sig.data() };
                detector.process (ptrs, 1, (int) sig.size(), true);
                expectEquals (detector.getNumPeaks(), 0);
            }

            beginTest ("changing config does not allocate or crash");
            {
                FeedbackDetector detector;
                detector.prepare (fs, 2, DetectorConfig {});
                DetectorConfig cfg;
                cfg.minFrequencyHz = 200.0f;
                detector.setConfig (cfg);
                expectWithinAbsoluteError (detector.getConfig().minFrequencyHz, 200.0f, 0.001f);
            }
        }
    };
}

juce::UnitTest* detectorTests()
{
    static DetectorTests tests;
    return &tests;
}

} // namespace defeedback::tests
