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
#include "dsp/Biquad.h"
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
        std::vector<std::pair<double, double>> probes;   // (Hz, output linear magnitude)
        std::vector<std::pair<double, double>> inProbes; // (Hz, input linear magnitude)
        double maxJump = 0.0;      // max |out[n]-out[n-1]| (click indicator)
        double refDiffDb = -999.0; // output vs a reference fixed biquad
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
            else if (kind == "sine" || kind == "raw" || kind == "fixed" || kind == "bankonly" || kind == "bankoneblock")
                v = 0.3 * std::sin (2.0 * kPi * 1000.0 * t);
            else if (kind == "sweep")
                v = 0.3 * std::sin (2.0 * kPi * (200.0 * t + 0.5 * (4000.0 / 8.0) * t * t));
            else
                v = 0.2 * (rng.nextDouble() * 2.0 - 1.0);

            left[(size_t) i] = (float) v;
            right[(size_t) i] = (float) v;
        }
    }

    Result analyse (const juce::String& kind, double seconds, bool freezeAfterSettle)
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

        defeedback::Biquad fixedFilter;
        fixedFilter.setCoeffs (defeedback::Biquad::makePeak (fs, 1000.0, 20.0, -9.0));
        fixedFilter.reset();

        for (int pos = 0; pos + blockSize <= n; pos += blockSize)
        {
            if (freezeAfterSettle && pos >= (int) (fs * 1.0))
                bank.setFreeze (true);

            if (kind == "raw")
            {
                ++blocks;
                continue;
            }

            if (kind == "fixed")
            {
                for (int i = 0; i < blockSize; ++i)
                    outL[(size_t) (pos + i)] = (float) fixedFilter.processSample ((double) inL[(size_t) (pos + i)]);
                ++blocks;
                continue;
            }

            if (kind == "bankoneblock")
            {
                if (pos != 0)
                {
                    ++blocks;
                    continue;
                }

                defeedback::DetectedPeak p;
                p.frequencyHz = 1000.0f;
                p.confidence = 1.0f;
                p.q = 20.0f;
                p.prominenceDb = 30.0f;
                p.channel = 0;
                bank.update (&p, 1);
                float* ptrs[2] { outL.data(), outR.data() };
                bank.process (ptrs, 2, n);
                r.maxNotches = bank.getActiveCount();
                ++notchBlocks;
                ++blocks;
                continue;
            }

            if (kind == "bankonly")
            {
                defeedback::DetectedPeak p;
                p.frequencyHz = 1000.0f;
                p.confidence = 1.0f;
                p.q = 20.0f;
                p.prominenceDb = 30.0f;
                p.channel = 0;
                bank.update (&p, 1);
                float* ptrs[2] { outL.data() + pos, outR.data() + pos };
                bank.process (ptrs, 2, blockSize);

                if (gVerbose && pos % (int) (fs / 2) == 0)
                {
                    defeedback::NotchInfo info;
                    if (bank.getNotchInfo (0, info))
                        std::fprintf (stderr, "t=%.1f notch f=%.3f q=%.3f gain=%.4f\n",
                                      (double) pos / fs, info.frequencyHz, info.q, info.gainDb);
                }

                if (gVerbose && pos == 200000)
                {
                    for (int k = -2; k <= 2; ++k)
                        std::fprintf (stderr, "  n=%d in=%.7f out=%.7f\n",
                                      k, inL[(size_t) (pos + k)], outL[(size_t) (pos + k)]);
                }

                const int active = bank.getActiveCount();
                if (active > 0)
                    ++notchBlocks;
                r.maxNotches = juce::jmax (r.maxNotches, active);
                ++blocks;
                continue;
            }

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

        for (size_t i = 1; i < outL.size(); ++i)
            r.maxJump = juce::jmax (r.maxJump, std::abs ((double) outL[i] - (double) outL[i - 1]));

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

        // Specific distortion probes. These frequencies carry no input energy,
        // so anything present is intermodulation / harmonic distortion.
        auto levelAtHz = [&] (const std::vector<double>& m, double hz)
        {
            const int b = juce::jlimit (1, numBins - 2, (int) std::llround (hz / fs * fftSize));
            double best = 0.0;
            for (int k = b - 1; k <= b + 1; ++k)
                best = juce::jmax (best, m[(size_t) k]);
            return best;
        };

        std::vector<double> probeHz;
        if (kind == "twotone")
            probeHz = { 300.0, 700.0, 1600.0, 2300.0 };
        else if (kind == "sine" || kind == "raw" || kind == "fixed" || kind == "bankonly" || kind == "bankoneblock")
            probeHz = { 1000.0, 2000.0, 3000.0, 4000.0 };

        for (auto hz : probeHz)
        {
            r.probes.push_back ({ hz, levelAtHz (outSpec, hz) });
            r.inProbes.push_back ({ hz, levelAtHz (inSpec, hz) });
        }

        {
            auto refCoeffs = defeedback::Biquad::makePeak (fs, 1000.0, 20.0, -9.0);
            defeedback::Biquad refFilter;
            refFilter.setCoeffs (refCoeffs);
            refFilter.reset();
            (void) refCoeffs;
            double diffEnergy = 0.0, sigEnergy = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double ref = refFilter.processSample ((double) inL[(size_t) i]);
                const double d = (double) outL[(size_t) i] - ref;
                diffEnergy += d * d;
                sigEnergy += (double) outL[(size_t) i] * (double) outL[(size_t) i];
            }
            if (sigEnergy > 0.0 && diffEnergy > 0.0)
                r.refDiffDb = 10.0 * std::log10 (diffEnergy / sigEnergy);

            double refEnergy = 0.0;
            for (int i = 0; i < n; ++i)
                refEnergy += (double) inL[(size_t) i] * (double) inL[(size_t) i];
            std::fprintf (stderr, "outRMS=%.2f dB  refRMS=%.2f dB  rawRMS=%.2f dB\n",
                          10.0 * std::log10 (juce::jmax (1e-12, sigEnergy / n)),
                          10.0 * std::log10 (juce::jmax (1e-12, refEnergy / n)),
                          10.0 * std::log10 (juce::jmax (1e-12, refEnergy / n)));
        }

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
    const bool freeze = argc > 4 && juce::String (argv[4]) == "freeze";

    const auto r = analyse (kind, seconds, freeze);

    std::printf ("signal=%s finite=%s maxNotches=%d notchOn=%.1f%%\n",
                 kind.toRawUTF8(), r.finite ? "yes" : "NO",
                 r.maxNotches, r.notchOnFraction * 100.0);
    std::printf ("fundamental=%.1f dBFS  worst-added-spur=%.1f dBc @ %.0f Hz\n",
                 r.fundamentalDb, r.worstSpurDb, r.worstSpurHz);
    std::printf ("maxJump=%.4f  refDiff=%.1f dB\n", r.maxJump, r.refDiffDb);

    for (size_t i = 0; i < r.probes.size(); ++i)
        std::printf ("  probe %7.0f Hz : in %+7.1f dBc  out %+7.1f dBc\n",
                     r.probes[i].first,
                     20.0 * std::log10 (juce::jmax (1.0e-12, r.inProbes[i].second)) - r.fundamentalDb,
                     20.0 * std::log10 (juce::jmax (1.0e-12, r.probes[i].second)) - r.fundamentalDb);

    return 0;
}
