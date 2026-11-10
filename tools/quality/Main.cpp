/*
    De-Feedback quality harness.

    Runs the exact detector + adaptive notch chain through a set of scenarios
    and reports objective metrics. With --gate it exits non-zero when a metric
    falls outside the alpha thresholds, so it can be a CI quality gate.

    Usage: defeedback_quality [--gate] [--verbose]
*/

#include <juce_dsp/juce_dsp.h>

#include "dsp/AdaptiveNotchBank.h"
#include "dsp/FeedbackDetector.h"
#include "dsp/Parameters.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const double fs = 48000.0;
    const int    blockSize = 64;
    bool gGate = false;

    struct ModeParams
    {
        int   onFrames;
        float onConfidence;
        float offConfidence;
        float baseProminenceDb;
        float cutScale;
        float attackSeconds;
        bool  requireGrowth;
    };

    ModeParams modeParams (defeedback::parameters::ProtectionMode m)
    {
        switch (m)
        {
            case defeedback::parameters::ProtectionMode::conservative: return { 8, 0.62f, 0.34f, 12.0f, 0.70f, 0.05f, true };
            case defeedback::parameters::ProtectionMode::aggressive:   return { 3, 0.45f, 0.28f,  8.0f, 1.00f, 0.01f, false };
            case defeedback::parameters::ProtectionMode::normal:
            default:                                                   return { 5, 0.55f, 0.30f, 10.0f, 1.00f, 0.02f, false };
        }
    }

    defeedback::DetectorConfig detectorConfig (defeedback::parameters::ProtectionMode m, float sensitivity = 0.0f)
    {
        const auto p = modeParams (m);
        defeedback::DetectorConfig c;
        c.onFrames = p.onFrames;
        c.onConfidence = p.onConfidence;
        c.offConfidence = p.offConfidence;
        c.requireGrowth = p.requireGrowth;
        c.minProminenceDb = juce::jlimit (3.0f, 30.0f, p.baseProminenceDb - sensitivity);
        c.strongProminenceDb = c.minProminenceDb + 12.0f;
        return c;
    }

    defeedback::AdaptiveNotchBank::Config bankConfig (defeedback::parameters::ProtectionMode m, bool linked = true)
    {
        const auto p = modeParams (m);
        defeedback::AdaptiveNotchBank::Config c;
        c.linked = linked;
        c.maxCutDb = 9.0f * p.cutScale;
        c.attackSeconds = p.attackSeconds;
        c.releaseSeconds = 4.0f;
        return c;
    }

    struct ChainResult
    {
        std::vector<float> left;
        std::vector<float> right;
        int maxNotches = 0;
        int firstNotchBlock = -1;
        int lastActiveBlock = -1;
        bool finite = true;
    };

    // Runs the detector + bank chain (input tap, as in the plugin).
    ChainResult runChain (const std::vector<float>& inL,
                          const std::vector<float>& inR,
                          defeedback::parameters::ProtectionMode mode,
                          int channels,
                          bool linked = true)
    {
        defeedback::FeedbackDetector det;
        det.prepare (fs, channels, detectorConfig (mode));

        defeedback::AdaptiveNotchBank bank;
        bank.prepare (fs, blockSize, channels);
        bank.setConfig (bankConfig (mode, linked));

        ChainResult r;
        r.left = inL;
        r.right = inR;

        const int n = (int) inL.size();

        for (int pos = 0; pos + blockSize <= n; pos += blockSize)
        {
            const float* analysis[2] { inL.data() + pos, inR.data() + pos };
            det.process (analysis, linked ? 1 : channels, blockSize, linked);

            defeedback::DetectedPeak peaks[16];
            const int np = juce::jmin (16, det.getNumPeaks());
            for (int i = 0; i < np; ++i)
                peaks[i] = det.getPeak (i);

            bank.update (peaks, np);

            float* ptrs[2] { r.left.data() + pos, r.right.data() + pos };
            bank.process (ptrs, channels, blockSize);

            const int active = bank.getActiveCount();
            r.maxNotches = juce::jmax (r.maxNotches, active);
            if (active > 0)
            {
                if (r.firstNotchBlock < 0)
                    r.firstNotchBlock = pos / blockSize;
                r.lastActiveBlock = pos / blockSize;
            }

            for (int i = 0; i < blockSize; ++i)
                if (! std::isfinite (r.left[(size_t) (pos + i)]))
                    r.finite = false;
        }

        return r;
    }

    double rms (const std::vector<float>& x, int from, int to)
    {
        double e = 0.0;
        int c = 0;
        for (int i = from; i < to && i < (int) x.size(); ++i)
        {
            e += (double) x[(size_t) i] * (double) x[(size_t) i];
            ++c;
        }
        return c > 0 ? std::sqrt (e / c) : 0.0;
    }

    // Magnitude (linear) of the bin nearest hz, max over +/-1 bin.
    double binMag (const std::vector<float>& x, double hz, int from, int to)
    {
        const int order = 15, fftSize = 1 << order, numBins = fftSize / 2 + 1;
        juce::dsp::FFT fft (order);

        std::vector<float> data ((size_t) (fftSize * 2), 0.0f);
        int segs = 0;
        std::vector<double> mag ((size_t) numBins, 0.0);

        for (int off = from; off + fftSize <= to; off += fftSize / 2)
        {
            for (int i = 0; i < fftSize; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) (fftSize - 1));
                data[(size_t) i] = (float) (x[(size_t) (off + i)] * w);
            }
            for (int i = fftSize; i < fftSize * 2; ++i)
                data[(size_t) i] = 0.0f;

            fft.performRealOnlyForwardTransform (data.data(), true);

            for (int b = 0; b < numBins; ++b)
                mag[(size_t) b] += std::sqrt ((double) data[(size_t) (2 * b)] * data[(size_t) (2 * b)]
                                            + (double) data[(size_t) (2 * b + 1)] * data[(size_t) (2 * b + 1)]);
            ++segs;
        }

        if (segs == 0)
            return 0.0;

        const int b = juce::jlimit (1, numBins - 2, (int) std::llround (hz / fs * fftSize));
        double best = 0.0;
        for (int k = b - 1; k <= b + 1; ++k)
            best = juce::jmax (best, mag[(size_t) k] / segs);

        return best;
    }

    std::vector<float> genFeedbackOnset (double freq, double seconds, double onset)
    {
        const int n = (int) (fs * seconds);
        std::vector<float> s ((size_t) n);
        juce::Random rng (1234);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            const double ramp = juce::jlimit (0.0, 1.0, (t - onset) / 0.4);
            s[(size_t) i] = (float) (0.8 * ramp * std::sin (2.0 * kPi * freq * t)
                                   + 0.03 * (rng.nextDouble() * 2.0 - 1.0));
        }
        return s;
    }

    // A clean musical passage: notes with a few harmonics and envelopes.
    std::vector<float> genCleanMusic (double seconds)
    {
        const int n = (int) (fs * seconds);
        std::vector<float> s ((size_t) n, 0.0f);
        const double notes[] { 220.0, 261.63, 329.63, 392.0, 440.0, 523.25 };
        juce::Random rng (77);

        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            const double noteIdx = std::floor (t * 2.0);
            const double f0 = notes[(int) noteIdx % 6];
            const double env = std::exp (-std::fmod (t, 0.5) * 4.0);
            double v = 0.0;
            for (int h = 1; h <= 5; ++h)
                v += (0.18 / h) * std::sin (2.0 * kPi * f0 * h * t);
            s[(size_t) i] = (float) (v * env + 0.003 * (rng.nextDouble() * 2.0 - 1.0));
        }
        return s;
    }

    // A clean voiced-speech-like signal: harmonics modulated by formants.
    std::vector<float> genCleanSpeech (double seconds)
    {
        const int n = (int) (fs * seconds);
        std::vector<float> s ((size_t) n, 0.0f);
        juce::Random rng (99);

        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            const double f0 = 120.0 + 30.0 * std::sin (2.0 * kPi * 2.0 * t);
            double v = 0.0;
            for (int h = 1; h <= 40; ++h)
            {
                const double f = f0 * h;
                const double formant = std::exp (-std::pow ((f - 700.0) / 250.0, 2.0))
                                     + 0.8 * std::exp (-std::pow ((f - 1800.0) / 350.0, 2.0));
                v += 0.04 * formant * std::sin (2.0 * kPi * f * t);
            }
            const double env = 0.6 + 0.4 * std::sin (2.0 * kPi * 3.0 * t);
            s[(size_t) i] = (float) (v * env + 0.003 * (rng.nextDouble() * 2.0 - 1.0));
        }
        return s;
    }

    int gPass = 0, gFail = 0;

    void report (const juce::String& name, bool pass, const juce::String& detail)
    {
        std::printf ("%-34s %s  %s\n", name.toRawUTF8(), pass ? "PASS" : "FAIL", detail.toRawUTF8());
        pass ? ++gPass : ++gFail;

        if (! pass)
            gGate = gGate || true; // failure already recorded
    }

    void scenarioFeedbackOnset (defeedback::parameters::ProtectionMode mode, const juce::String& tag)
    {
        for (double freq : { 250.0, 1000.0, 3150.0, 6300.0 })
        {
            const auto sig = genFeedbackOnset (freq, 5.0, 0.5);
            const auto r = runChain (sig, sig, mode, 2);

            const double latency = r.firstNotchBlock >= 0 ? (double) r.firstNotchBlock * blockSize / fs - 0.5 : -1.0;
            const int from = (int) (fs * 3.0), to = (int) (fs * 4.5);
            const double toneDb = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (sig, freq, from, to)));
            const double outDb  = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (r.left, freq, from, to)));
            const double depth = toneDb - outDb;
            const double h2 = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (r.left, freq * 2.0, from, to))) - toneDb;

            const bool pass = r.finite && latency >= 0.0 && latency < 0.40 && depth > 3.0 && h2 < -40.0;
            report ("feedback_onset_" + tag + "_" + juce::String (juce::roundToInt (freq)) + "Hz",
                    pass,
                    "lat=" + juce::String (latency, 2) + "s depth=" + juce::String (depth, 1)
                    + "dB h2=" + juce::String (h2, 1) + "dBc notches=" + juce::String (r.maxNotches));
        }
    }

    void scenarioClean (const juce::String& name, const std::vector<float>& sig,
                        defeedback::parameters::ProtectionMode mode, int maxAllowed)
    {
        const auto r = runChain (sig, sig, mode, 2);
        const bool pass = r.finite && r.maxNotches <= maxAllowed;
        report (name, pass, "maxNotches=" + juce::String (r.maxNotches) + " (allowed " + juce::String (maxAllowed) + ")");
    }

    void scenarioStereoIdentity()
    {
        const auto sig = genFeedbackOnset (1000.0, 4.0, 0.3);
        const auto r = runChain (sig, sig, defeedback::parameters::ProtectionMode::normal, 2);

        float maxDiff = 0.0f;
        for (size_t i = 0; i < r.left.size(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (r.left[i] - r.right[i]));

        report ("stereo_identity_1000Hz", maxDiff < 1.0e-5f, "maxL-R=" + juce::String (maxDiff));
    }

    std::vector<float> genToneBurst (double freq, double seconds, double on, double off, float amp)
    {
        const int n = (int) (fs * seconds);
        std::vector<float> s ((size_t) n);
        juce::Random rng (55);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            double env = (t >= on && t <= off) ? 1.0 : 0.0;
            if (t >= on && t < on + 0.02)
                env = (t - on) / 0.02;
            if (t > off - 0.02 && t <= off)
                env = (off - t) / 0.02;
            s[(size_t) i] = (float) (0.03 * (rng.nextDouble() * 2.0 - 1.0) + amp * env * std::sin (2.0 * kPi * freq * t));
        }
        return s;
    }

    std::vector<float> genSustainedTone (double freq, double seconds, float amp)
    {
        const int n = (int) (fs * seconds);
        std::vector<float> s ((size_t) n);
        juce::Random rng (21);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            s[(size_t) i] = (float) (amp * std::sin (2.0 * kPi * freq * t) + 0.005 * (rng.nextDouble() * 2.0 - 1.0));
        }
        return s;
    }

    void scenarioRelease()
    {
        const auto sig = genToneBurst (2000.0, 9.0, 0.5, 2.5, 0.8f);
        const auto r = runChain (sig, sig, defeedback::parameters::ProtectionMode::normal, 2);

        const int toneEndBlock = (int) (2.5 * fs / blockSize);
        const int deadlineBlock = (int) ((2.5 + 6.0) * fs / blockSize);
        const bool pass = r.finite && r.firstNotchBlock >= 0 && r.firstNotchBlock < toneEndBlock
                       && r.lastActiveBlock >= 0 && r.lastActiveBlock <= deadlineBlock;

        report ("release_tone_burst", pass,
                "first=" + juce::String (r.firstNotchBlock) + " last=" + juce::String (r.lastActiveBlock)
                + " end=" + juce::String (toneEndBlock) + " deadline=" + juce::String (deadlineBlock));
    }

    void scenarioIndependentStereo()
    {
        const auto l = genFeedbackOnset (1000.0, 5.0, 0.3);
        const auto r = genFeedbackOnset (3000.0, 5.0, 0.3);
        const auto res = runChain (l, r, defeedback::parameters::ProtectionMode::normal, 2, false);

        const int from = (int) (fs * 3.0), to = (int) (fs * 4.5);
        const double inL = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (l, 1000.0, from, to)));
        const double outL = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (res.left, 1000.0, from, to)));
        const double inR = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (r, 3000.0, from, to)));
        const double outR = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (res.right, 3000.0, from, to)));

        const bool pass = res.finite && (inL - outL) > 3.0 && (inR - outR) > 3.0;
        report ("independent_stereo_L1000_R3000", pass,
                "depthL=" + juce::String (inL - outL, 1) + "dB depthR=" + juce::String (inR - outR, 1)
                + "dB notches=" + juce::String (res.maxNotches));
    }

    void scenarioSustainedTone()
    {
        const auto sig = genSustainedTone (1000.0, 6.0, 0.2f);
        const auto r = runChain (sig, sig, defeedback::parameters::ProtectionMode::conservative, 2);

        const int from = (int) (fs * 3.0), to = (int) (fs * 5.0);
        const double inDb = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (sig, 1000.0, from, to)));
        const double outDb = 20.0 * std::log10 (juce::jmax (1.0e-12, binMag (r.left, 1000.0, from, to)));
        const double depth = inDb - outDb;

        report ("sustained_pure_tone_conservative", r.finite && depth <= 7.0,
                "depth=" + juce::String (depth, 1) + "dB notches=" + juce::String (r.maxNotches));
    }

    void dumpIndependent()
    {
        const auto l = genFeedbackOnset (1000.0, 4.0, 0.3);
        const auto r = genFeedbackOnset (3000.0, 4.0, 0.3);
        defeedback::FeedbackDetector det;
        det.prepare (fs, 2, detectorConfig (defeedback::parameters::ProtectionMode::normal));

        for (int pos = 0; pos + blockSize <= (int) l.size(); pos += blockSize)
        {
            const float* analysis[2] { l.data() + pos, r.data() + pos };
            det.process (analysis, 2, blockSize, false);

            if ((pos / blockSize) % 12 == 0)
            {
                std::printf ("t=%.2f peaks=%d:", (double) pos / fs, det.getNumPeaks());
                for (int i = 0; i < det.getNumPeaks(); ++i)
                    std::printf (" ch%d %.0fHz(%.2f)", det.getPeak (i).channel,
                                 det.getPeak (i).frequencyHz, det.getPeak (i).confidence);
                std::printf ("\n");
            }
        }
    }

    void dumpMusic()
    {
        const auto sig = genCleanMusic (4.0);
        defeedback::FeedbackDetector det;
        det.prepare (fs, 2, detectorConfig (defeedback::parameters::ProtectionMode::conservative));

        for (int pos = 0; pos + blockSize <= (int) sig.size(); pos += blockSize)
        {
            const float* analysis[1] { sig.data() + pos };
            det.process (analysis, 1, blockSize, true);

            if ((pos / blockSize) % 12 == 0)
            {
                std::printf ("t=%.2f peaks=%d:", (double) pos / fs, det.getNumPeaks());
                for (int i = 0; i < det.getNumPeaks(); ++i)
                    std::printf (" %.0fHz(%.2f)", det.getPeak (i).frequencyHz, det.getPeak (i).confidence);
                std::printf ("\n");
            }
        }
    }

    void scenarioCPU (double seconds)
    {
        const auto sig = genFeedbackOnset (2000.0, seconds, 0.2);

        const auto start = juce::Time::getHighResolutionTicks();
        const auto r = runChain (sig, sig, defeedback::parameters::ProtectionMode::normal, 2);
        const auto wall = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);

        const double rt = seconds / juce::jmax (1.0e-9, wall);
        const double loadPct = 100.0 / rt;
        report ("cpu_48k_64_stereo", loadPct < 12.0,
                "rt=" + juce::String (rt, 1) + "x load=" + juce::String (loadPct, 2) + "%");
    }
}

int main (int argc, char** argv)
{
    bool dump = false, dumpInd = false;
    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) == "--gate") gGate = true;
        if (juce::String (argv[i]) == "--dump") dump = true;
        if (juce::String (argv[i]) == "--dumpind") dumpInd = true;
    }

    if (dump)
    {
        dumpMusic();
        return 0;
    }

    if (dumpInd)
    {
        dumpIndependent();
        return 0;
    }

    using defeedback::parameters::ProtectionMode;

    std::printf ("De-Feedback quality harness\n");
    std::printf ("============================\n\n");

    std::printf ("-- feedback onset --\n");
    scenarioFeedbackOnset (ProtectionMode::normal, "normal");
    scenarioFeedbackOnset (ProtectionMode::conservative, "conservative");

    std::printf ("\n-- clean material (Conservative) --\n");
    scenarioClean ("clean_music_conservative", genCleanMusic (8.0), ProtectionMode::conservative, 2);
    scenarioClean ("clean_speech_conservative", genCleanSpeech (8.0), ProtectionMode::conservative, 2);

    std::printf ("\n-- release / independent stereo / sustained tone --\n");
    scenarioRelease();
    scenarioIndependentStereo();
    scenarioSustainedTone();

    std::printf ("\n-- stereo / cpu --\n");
    scenarioStereoIdentity();
    scenarioCPU (6.0);

    std::printf ("\n============================\n");
    std::printf ("%d passed, %d failed\n", gPass, gFail);

    return (gGate && gFail > 0) ? 1 : 0;
}
