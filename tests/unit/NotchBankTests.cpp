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
        }
    };
}

juce::UnitTest* notchBankTests()
{
    static NotchBankTests tests;
    return &tests;
}

} // namespace defeedback::tests
