/*
    Offline distortion / artefact analyser for De-Feedback.

    Runs the exact detector + notch-bank chain on deterministic signals and
    reports energy that is NOT present in the input (distortion products,
    intermodulation, broadband splatter) plus notch activity over time.

    Usage: defeedback_analyze <signal> <seconds>
      signal: twotone | sine | sweep | noise
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
    const double fs = 48000.0;
    const int blockSize = 64;
    bool gVerbose = false;

    double db (double x) { return 20.0 * std::log10 (juce::jmax (1.0e-12, x)); }

    struct Result
    {
        double fundamentalDb = 0.0;
        double worstSpurDb = -200.0;
        double worstSpurHz = 0.0;
        double notchOnFraction = 0.0;
        int maxNotches = 0;
        int totalNotchesSeen = 0;
        bool finite = true;
    };

    void makeSignal (const juce::String& kind, std::vector<float>& left, std::vector<float>& right)
    {
        const int n = (int) left.size();
        juce::Random rng (4242);

        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            double v = 0.0;

            if (kind == "twotone")
                v = 0.25 * std::sin (2.0 * kPi * 1000.0 * t) + 0.25 * std::sin (2.0 * kPi * 1300.0 * t);
            else if (kind == "sine")
                v = 0.3 * std::sin (2.0 * kPi * 1000.0 * t);
            else if (kind == "sweep")
                v = 0.3 * std::sin (2.0 * kPi * (200.0 * t + 0.5 * (4000.0 / 8.0) * t * t));
            else
                v = 0.2 * (rng.nextDouble() * 2.0 - 1.0);

            left[(size_t) i] = (float) v;
            right[(size_t) i] = (float) v;
        }
    }

    Result analyse (const juce::String& kind, double seconds)
    {
        const int n = (int) (fs * seconds);

        std::vector<float> inL ((size_t) n), inR ((size_t) n);
        makeSignal (kind, inL, inR);

        std::vector<float> outL = inL, outR = inR;

        defeedback::FeedbackDetector det;
        det.prepare (fs, 2, defeedback::DetectorConfig {});

        defeedback::AdaptiveNotchBank bank;
        bank.prepare (fs, blockSize, 2);

        int notchBlocks = 0;
        int blocks = 0;
        Result r;

        for (int pos = 0; pos + blockSize <= n; pos += blockSize)
        {
            // Match the real plugin: the detector taps the INPUT, not the
            // post-notch output.
            const float* analysis[1] { inL.data() + pos };
            det.process (analysis, 1, blockSize, true);

            std::vector<defeedback::DetectedPeak> peaks;
            for (int i = 0; i < det.getNumPeaks(); ++i)
                peaks.push_back (det.getPeak (i));

            bank.update (peaks.data(), (int) peaks.size());

            float* ptrs[2] { outL.data() + pos, outR.data() + pos };
            bank.process (ptrs, 2, blockSize);

            const int active = bank.getActiveCount();
            r.maxNotches = juce::jmax (r.maxNotches, active);
            r.totalNotchesSeen += active;
            if (active > 0)
                ++notchBlocks;
            ++blocks;

            if (gVerbose && pos % (int) (fs / 4) == 0)
                std::fprintf (stderr, "t=%.2f peaks=%d notches=%d\n",
                              (double) pos / fs, det.getNumPeaks(), active);

            for (int i = 0; i < blockSize; ++i)
                if (! std::isfinite (outL[(size_t) (pos + i)]))
                    r.finite = false;
        }

        r.notchOnFraction = blocks > 0 ? (double) notchBlocks / blocks : 0.0;

        // Spectrum of the steady-state middle third (skip start/end transients).
        const int fftOrder = 15; // 32768
        const int fftSize = 1 << fftOrder;
        const int numBins = fftSize / 2 + 1;
        juce::dsp::FFT fft (fftOrder);

        auto spectrum = [&] (const std::vector<float>& x)
        {
            std::vector<double> mag ((size_t) numBins, 0.0);
            std::vector<float> data ((size_t) (fftSize * 2));
            const int start = (int) (x.size() / 3);
            const int segs = juce::jmax (1, (int) ((x.size() - (size_t) start - (size_t) fftSize) / (size_t) (fftSize / 2)));

            for (int s = 0; s < segs; ++s)
            {
                const int off = start + s * fftSize / 2;
                if (off + fftSize > (int) x.size())
                    break;

                for (int i = 0; i < fftSize; ++i)
                {
                    const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) (fftSize - 1));
                    data[(size_t) i] = (float) (x[(size_t) (off + i)] * w);
                }
                for (int i = fftSize; i < fftSize * 2; ++i)
                    data[(size_t) i] = 0.0f;

                fft.performRealOnlyForwardTransform (data.data(), true);

                for (int b = 0; b < numBins; ++b)
                {
                    const double re = data[(size_t) (2 * b)];
                    const double im = data[(size_t) (2 * b + 1)];
                    mag[(size_t) b] += std::sqrt (re * re + im * im);
                }
            }

            for (auto& m : mag)
                m /= (double) segs;

            return mag;
        };

        const auto inSpec = spectrum (inL);
        const auto outSpec = spectrum (outL);

        // Band-averaged subtraction so narrow signal bins do not dominate the
        // "spur" search and so legitimate input content is not counted.
        const int avgBins = 3;
        auto bandDb = [&] (const std::vector<double>& m, int b)
        {
            double sum = 0.0;
            int c = 0;
            for (int k = juce::jmax (0, b - avgBins); k <= juce::jmin (numBins - 1, b + avgBins); ++k)
            {
                sum += m[(size_t) k] * m[(size_t) k];
                ++c;
            }
            return 10.0 * std::log10 (juce::jmax (1.0e-20, sum / c));
        };

        // Fundamental reference: strongest input bin.
        int fundBin = 0;
        for (int b = 4; b < numBins; ++b)
            if (inSpec[(size_t) b] > inSpec[(size_t) fundBin])
                fundBin = b;

        r.fundamentalDb = 20.0 * std::log10 (juce::jmax (1.0e-12, inSpec[(size_t) fundBin]));

        for (int b = 6; b < numBins - 6; ++b)
        {
            const double inDb = bandDb (inSpec, b);
            const double outDb = bandDb (outSpec, b);
            // Only count energy that materially exceeds what was already there.
            const double added = outDb - inDb;

            if (inDb < r.fundamentalDb - 45.0) // quiet region: a new spur here is suspicious
            {
                const double spur = outDb - r.fundamentalDb;
                if (spur > r.worstSpurDb && added > 3.0)
                {
                    r.worstSpurDb = spur;
                    r.worstSpurHz = (double) b * fs / fftSize;
                }
            }
        }

        return r;
    }
}

int main (int argc, char** argv)
{
    const juce::String kind = argc > 1 ? juce::String (argv[1]) : "twotone";
    const double seconds = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 8.0;
    gVerbose = argc > 3 && juce::String (argv[3]) == "verbose";

    const auto r = analyse (kind, seconds);

    std::printf ("signal=%s finite=%s maxNotches=%d notchOn=%.1f%%\n",
                 kind.toRawUTF8(), r.finite ? "yes" : "NO",
                 r.maxNotches, r.notchOnFraction * 100.0);
    std::printf ("fundamental=%.1f dBFS  worst-added-spur=%.1f dBc @ %.0f Hz\n",
                 r.fundamentalDb, r.worstSpurDb, r.worstSpurHz);

    return 0;
}
