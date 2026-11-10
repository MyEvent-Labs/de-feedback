#include "TestRegistry.h"

#include "dsp/Biquad.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>

namespace defeedback::tests
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double measureGainDb (const Biquad::Coeffs& c, double sampleRate, double freq)
    {
        Biquad b;
        b.setCoeffs (c);
        b.reset();

        const int n = (int) sampleRate; // one second
        double sumIn = 0.0, sumOut = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double x = std::sin (2.0 * kPi * freq * (double) i / sampleRate);
            const double y = b.processSample (x);

            if (i > n / 2)
            {
                sumIn += x * x;
                sumOut += y * y;
            }
        }

        if (sumIn <= 0.0)
            return 0.0;

        return 10.0 * std::log10 (sumOut / sumIn);
    }

    class BiquadTests : public juce::UnitTest
    {
    public:
        BiquadTests() : juce::UnitTest ("Biquad", "defeedback") {}

        void runTest() override
        {
            beginTest ("identity is transparent");
            {
                Biquad b;
                b.setCoeffs (Biquad::makeIdentity());
                b.reset();
                expectWithinAbsoluteError (b.processSample (1.0), 1.0, 1.0e-12);
                expectWithinAbsoluteError (b.processSample (-0.5), -0.5, 1.0e-12);
            }

            beginTest ("peak depth matches requested gain at centre");
            {
                for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0 })
                {
                    for (double f : { 120.0, 1000.0, 5000.0 })
                    {
                        const auto c = Biquad::makePeak (fs, f, 20.0, -9.0);
                        const auto gain = measureGainDb (c, fs, f);
                        expect (std::abs (gain - (-9.0)) < 1.5,
                                "fs=" + juce::String (fs) + " f=" + juce::String (f) + " gain=" + juce::String (gain));
                    }
                }
            }

            beginTest ("peak is shallow away from centre");
            {
                const auto fs = 48000.0;
                const auto c = Biquad::makePeak (fs, 1000.0, 20.0, -12.0);
                const auto gainFar = measureGainDb (c, fs, 4000.0);
                expect (gainFar > -2.0, "far gain=" + juce::String (gainFar));
            }

            beginTest ("higher Q is narrower");
            {
                const auto fs = 48000.0;
                const auto wide = Biquad::makePeak (fs, 1000.0, 5.0, -12.0);
                const auto narrow = Biquad::makePeak (fs, 1000.0, 40.0, -12.0);
                const auto wideGain = measureGainDb (wide, fs, 1200.0);
                const auto narrowGain = measureGainDb (narrow, fs, 1200.0);
                expect (narrowGain > wideGain, "narrow=" + juce::String (narrowGain) + " wide=" + juce::String (wideGain));
            }

            beginTest ("extreme parameters stay finite and bounded");
            {
                const double rates[] { 44100.0, 48000.0, 88200.0, 96000.0 };
                const double freqs[] { 1.0, 10.0, 100000.0, -50.0 };
                const double qs[] { 0.01, 1000.0, 0.0 };
                const double gains[] { -100.0, 40.0, 0.0 };

                for (auto fs : rates)
                    for (auto f : freqs)
                        for (auto q : qs)
                            for (auto g : gains)
                            {
                                const auto c = Biquad::makePeak (fs, f, q, g);
                                Biquad b;
                                b.setCoeffs (c);
                                b.reset();

                                for (int i = 0; i < 2000; ++i)
                                {
                                    const double y = b.processSample (std::sin (0.01 * i));
                                    expect (std::isfinite (y), "non-finite output");
                                    expect (std::abs (y) < 100.0, "unbounded output");
                                }
                            }
            }
        }
    };
}

juce::UnitTest* biquadTests()
{
    static BiquadTests tests;
    return &tests;
}

} // namespace defeedback::tests
