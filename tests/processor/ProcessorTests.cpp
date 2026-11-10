#include "TestRegistry.h"

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

namespace defeedback::tests
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    class ProcessorTests : public juce::UnitTest
    {
    public:
        ProcessorTests() : juce::UnitTest ("Processor", "defeedback") {}

        void runTest() override
        {
            const double fs = 48000.0;
            const int block = 512;

            auto setParam = [this] (DeFeedbackProcessor& p, const juce::String& id, float real)
            {
                auto* param = p.apvts.getParameter (id);
                expect (param != nullptr, "missing parameter " + id);
                if (param != nullptr)
                    param->setValueNotifyingHost (param->convertTo0to1 (real));
            };

            // Runs a 1 kHz sine through the processor and returns the output RMS
            // over the settled final third.
            auto runTone = [&] (DeFeedbackProcessor& p, float amplitude)
            {
                p.prepareToPlay (fs, block);
                juce::MidiBuffer midi;
                const int blocks = 120;
                double energy = 0.0;
                int count = 0;

                for (int b = 0; b < blocks; ++b)
                {
                    juce::AudioBuffer<float> buf (2, block);
                    for (int i = 0; i < block; ++i)
                    {
                        const float v = amplitude * (float) std::sin (2.0 * kPi * 1000.0 * (double) (b * block + i) / fs);
                        buf.setSample (0, i, v);
                        buf.setSample (1, i, v);
                    }

                    p.processBlock (buf, midi);

                    if (b >= blocks * 2 / 3)
                        for (int i = 0; i < block; ++i)
                        {
                            const double x = buf.getSample (0, i);
                            energy += x * x;
                            ++count;
                        }
                }

                return count > 0 ? std::sqrt (energy / count) : 0.0;
            };

            beginTest ("input trim changes the output level");
            {
                DeFeedbackProcessor p;
                setParam (p, parameters::bypass, 0.0f);
                setParam (p, parameters::inputTrimDb, 0.0f);
                const double a = runTone (p, 0.25f);

                DeFeedbackProcessor p2;
                setParam (p2, parameters::bypass, 0.0f);
                setParam (p2, parameters::inputTrimDb, 12.0f);
                const double b = runTone (p2, 0.25f);

                expect (a > 0.0, "base level is zero");
                const double gainDb = 20.0 * std::log10 (juce::jmax (1.0e-9, b / juce::jmax (1.0e-9, a)));
                expect (std::abs (gainDb - 12.0) < 1.5, "input trim gain=" + juce::String (gainDb) + " dB");
            }

            beginTest ("output trim changes the output level");
            {
                DeFeedbackProcessor p;
                setParam (p, parameters::outputTrimDb, 0.0f);
                const double a = runTone (p, 0.25f);

                DeFeedbackProcessor p2;
                setParam (p2, parameters::outputTrimDb, -12.0f);
                const double b = runTone (p2, 0.25f);

                const double gainDb = 20.0 * std::log10 (juce::jmax (1.0e-9, b / juce::jmax (1.0e-9, a)));
                expect (std::abs (gainDb - (-12.0)) < 1.5, "output trim gain=" + juce::String (gainDb) + " dB");
            }

            beginTest ("strength changes notch depth on a detected tone");
            {
                DeFeedbackProcessor pWeak;
                setParam (pWeak, parameters::mode, 1.0f);       // Normal
                setParam (pWeak, parameters::strength, 0.25f);
                setParam (pWeak, parameters::maxCutDb, 9.0f);
                const double weak = runTone (pWeak, 0.3f);

                DeFeedbackProcessor pStrong;
                setParam (pStrong, parameters::mode, 1.0f);
                setParam (pStrong, parameters::strength, 2.0f);
                setParam (pStrong, parameters::maxCutDb, 9.0f);
                const double strong = runTone (pStrong, 0.3f);

                // A deeper notch must leave less of the tone.
                expect (strong < weak * 0.9,
                        "strength did not change depth: weak=" + juce::String (weak, 4)
                        + " strong=" + juce::String (strong, 4));
            }

            beginTest ("bypass passes the dry signal unchanged");
            {
                DeFeedbackProcessor p;
                setParam (p, parameters::bypass, 1.0f);
                const double dry = runTone (p, 0.25f);

                // 0.25 amplitude sine RMS = 0.25 / sqrt(2).
                const double expected = 0.25 / std::sqrt (2.0);
                expect (std::abs (dry - expected) < 0.01, "bypass RMS=" + juce::String (dry, 4));
            }
        }
    };
}

juce::UnitTest* processorTests()
{
    static ProcessorTests tests;
    return &tests;
}

} // namespace defeedback::tests
