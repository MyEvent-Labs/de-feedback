#include "TestRegistry.h"

#include "dsp/AdaptiveNotchBank.h"

#include <cmath>
#include <vector>

namespace defeedback::tests
{

namespace
{
    DetectedPeak makePeak (float freq, float conf, float q = 20.0f)
    {
        DetectedPeak p;
        p.frequencyHz = freq;
        p.confidence = conf;
        p.q = q;
        p.prominenceDb = 20.0f;
        p.channel = 0;
        return p;
    }

    class NotchBankTests : public juce::UnitTest
    {
    public:
        NotchBankTests() : juce::UnitTest ("NotchBank", "defeedback") {}

        void runTest() override
        {
            const double fs = 48000.0;

            beginTest ("allocates notches from peaks");
            {
                AdaptiveNotchBank bank;
                bank.prepare (fs, 512, 2);

                DetectedPeak peaks[3] { makePeak (300.0f, 0.9f), makePeak (1200.0f, 0.8f), makePeak (4000.0f, 0.7f) };
                bank.update (peaks, 3);
                expectEquals (bank.getActiveCount(), 3);
            }

            beginTest ("recycles when full and never exceeds the cap");
            {
                AdaptiveNotchBank::Config cfg;
                cfg.maxNotchesPerLane = 12;
                AdaptiveNotchBank bank;
                bank.prepare (fs, 512, 1);
                bank.setConfig (cfg);

                std::vector<DetectedPeak> peaks;
                for (int i = 0; i < 30; ++i)
                    peaks.push_back (makePeak (150.0f + (float) i * 250.0f, 0.5f + 0.01f * (float) i));

                bank.update (peaks.data(), (int) peaks.size());
                expect (bank.getActiveCount() <= AdaptiveNotchBank::kMaxNotchesPerLane,
                        "count=" + juce::String (bank.getActiveCount()));
            }

            beginTest ("hard cut limit is enforced");
            {
                AdaptiveNotchBank::Config cfg;
                cfg.maxCutDb = 100.0f; // hostile
                cfg.strength = 2.0f;
                AdaptiveNotchBank bank;
                bank.prepare (fs, 512, 1);
                bank.setConfig (cfg);

                DetectedPeak p = makePeak (500.0f, 1.0f);
                bank.update (&p, 1);

                // Process long enough for gain to converge.
                std::vector<float> buf (512);
                for (int block = 0; block < 400; ++block)
                {
                    for (auto& s : buf)
                        s = (float) std::sin (0.2 * block);
                    float* ptrs[1] { buf.data() };
                    bank.process (ptrs, 1, (int) buf.size());
                }

                NotchInfo info;
                expect (bank.getNotchInfo (0, info));
                expect (std::abs (info.gainDb) <= AdaptiveNotchBank::kHardMaxCutDb + 0.5f,
                        "gain=" + juce::String (info.gainDb));
            }

            beginTest ("output remains finite and bounded");
            {
                AdaptiveNotchBank bank;
                bank.prepare (fs, 256, 2);

                DetectedPeak peaks[4] { makePeak (200.0f, 0.9f), makePeak (800.0f, 0.9f), makePeak (3000.0f, 0.9f), makePeak (9000.0f, 0.9f) };
                bank.update (peaks, 4);

                std::vector<float> left (256), right (256);
                for (int block = 0; block < 200; ++block)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        left[(size_t) i] = 0.5f * (float) std::sin (0.03 * (block * 256 + i));
                        right[(size_t) i] = left[(size_t) i];
                    }

                    float* ptrs[2] { left.data(), right.data() };
                    bank.process (ptrs, 2, 256);

                    for (int i = 0; i < 256; ++i)
                    {
                        expect (std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i]), "non-finite");
                        expect (std::abs (left[(size_t) i]) < 2.0f, "unbounded");
                    }
                }
            }

            beginTest ("clearAll releases every notch");
            {
                AdaptiveNotchBank bank;
                bank.prepare (fs, 256, 1);

                DetectedPeak p = makePeak (1000.0f, 1.0f);
                bank.update (&p, 1);
                expectEquals (bank.getActiveCount(), 1);

                bank.clearAll();

                std::vector<float> buf (256, 0.0f);
                for (int block = 0; block < 2000 && bank.getActiveCount() > 0; ++block)
                {
                    float* ptrs[1] { buf.data() };
                    bank.process (ptrs, 1, 256);
                }

                expectEquals (bank.getActiveCount(), 0);
            }

            beginTest ("mono processing is safe");
            {
                AdaptiveNotchBank bank;
                bank.prepare (fs, 128, 1);
                std::vector<float> buf (128, 0.25f);
                float* ptrs[1] { buf.data() };
                bank.process (ptrs, 1, 128);
                expect (std::isfinite (buf[64]));
            }

            beginTest ("linked stereo shares coefficients but not filter state");
            {
                // Regression: the bank used to drive both channels through one
                // Biquad, so each channel's output was computed from state
                // containing the other channel's samples. That produced a weak
                // notch and audible harmonic distortion on stereo material.
                const int block = 64;
                const int blocksToRun = 400;

                AdaptiveNotchBank::Config cfg;
                cfg.linked = true;
                cfg.maxCutDb = 9.0f;
                cfg.strength = 1.0f;
                cfg.attackSeconds = 0.001f;
                cfg.releaseSeconds = 0.1f;

                DetectedPeak p = makePeak (1000.0f, 1.0f);

                auto fillBlock = [&] (int b, std::vector<float>& buf)
                {
                    for (int i = 0; i < block; ++i)
                    {
                        const double t = (double) (b * block + i) / fs;
                        buf[(size_t) i] = 0.3f * (float) std::sin (2.0 * 3.14159265358979323846 * 1000.0 * t);
                    }
                };

                std::vector<float> mono ((size_t) (block * blocksToRun));
                {
                    AdaptiveNotchBank bank;
                    bank.prepare (fs, block, 1);
                    bank.setConfig (cfg);
                    std::vector<float> buf ((size_t) block);
                    for (int b = 0; b < blocksToRun; ++b)
                    {
                        fillBlock (b, buf);
                        bank.update (&p, 1);
                        float* ptrs[1] { buf.data() };
                        bank.process (ptrs, 1, block);
                        std::copy (buf.begin(), buf.end(), mono.begin() + (size_t) (b * block));
                    }
                }

                std::vector<float> left ((size_t) (block * blocksToRun));
                {
                    AdaptiveNotchBank bank;
                    bank.prepare (fs, block, 2);
                    bank.setConfig (cfg);
                    std::vector<float> lbuf ((size_t) block), rbuf ((size_t) block);
                    for (int b = 0; b < blocksToRun; ++b)
                    {
                        fillBlock (b, lbuf);
                        rbuf = lbuf;
                        bank.update (&p, 1);
                        float* ptrs[2] { lbuf.data(), rbuf.data() };
                        bank.process (ptrs, 2, block);
                        std::copy (lbuf.begin(), lbuf.end(), left.begin() + (size_t) (b * block));
                    }
                }

                // Compare the settled region only.
                const int settle = block * (blocksToRun / 2);
                float maxDiff = 0.0f;
                double outEnergy = 0.0;
                int count = 0;
                for (int i = settle; i < (int) mono.size(); ++i)
                {
                    maxDiff = juce::jmax (maxDiff, std::abs (left[(size_t) i] - mono[(size_t) i]));
                    outEnergy += (double) left[(size_t) i] * (double) left[(size_t) i];
                    ++count;
                }

                expect (maxDiff < 1.0e-4f, "stereo ch0 diverges from mono: " + juce::String (maxDiff));

                const double inRms = 0.3 / std::sqrt (2.0);
                const double outRms = std::sqrt (outEnergy / juce::jmax (1, count));
                const double gainDb = 20.0 * std::log10 (juce::jmax (1.0e-9, outRms / inRms));
                expect (gainDb < -6.0 && gainDb > -12.0,
                        "stereo notch depth wrong: " + juce::String (gainDb) + " dB");
            }
        }
    };
}

juce::UnitTest* notchBankTests()
{
    static NotchBankTests tests;
    return &tests;
}

} // namespace defeedback::tests
