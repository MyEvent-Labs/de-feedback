#include "dsp/FeedbackDetector.h"

#include <algorithm>
#include <cmath>

namespace defeedback
{

namespace
{
    constexpr float kMinDb = -140.0f;

    inline float binToHz (int bin, double sampleRate, int fftSize) noexcept
    {
        return (float) ((double) bin * sampleRate / (double) fftSize);
    }

    inline int hzToBin (float hz, double sampleRate, int fftSize) noexcept
    {
        return (int) std::llround ((double) hz * (double) fftSize / sampleRate);
    }
}

FeedbackDetector::FeedbackDetector() = default;

void FeedbackDetector::prepare (double sampleRate, int numChannels, const DetectorConfig& config)
{
    sampleRate_  = sampleRate > 0.0 ? sampleRate : 48000.0;
    numChannels_ = juce::jlimit (1, kMaxChannels, numChannels);
    config_      = config;

    fftOrder_ = juce::jlimit (8, 14, config_.fftOrder);
    fftSize_  = 1 << fftOrder_;
    const int overlap = juce::jmax (1, config_.overlapFactor);
    hopSize_  = juce::jmax (1, fftSize_ / overlap);
    numBins_  = fftSize_ / 2 + 1;

    fft_ = std::make_unique<juce::dsp::FFT> (fftOrder_);

    window_.resize ((size_t) fftSize_);
    for (int n = 0; n < fftSize_; ++n)
    {
        const auto w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) n / (double) (fftSize_ - 1));
        window_[(size_t) n] = (float) w;
    }

    fftData_.assign ((size_t) (fftSize_ * 2), 0.0f);
    magDb_.assign ((size_t) numBins_, kMinDb);
    for (auto& p : prevMagDb_)
        p.assign ((size_t) numBins_, kMinDb);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        ring_[ch].assign ((size_t) fftSize_, 0.0f);
        floorDb_[ch].assign ((size_t) numBins_, kMinDb);
        ringWrite_[ch] = 0;
        sinceHop_[ch]  = 0;
    }

    framePeaks_.reserve ((size_t) numBins_ / 2 + 8);
    framePeaks_.clear();

    reset();
}

void FeedbackDetector::reset() noexcept
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        std::fill (ring_[ch].begin(), ring_[ch].end(), 0.0f);
        std::fill (floorDb_[ch].begin(), floorDb_[ch].end(), kMinDb);
        ringWrite_[ch] = 0;
        sinceHop_[ch]  = 0;
    }

    std::fill (magDb_.begin(), magDb_.end(), kMinDb);
    for (auto& p : prevMagDb_)
        std::fill (p.begin(), p.end(), kMinDb);

    for (auto& lane : tracks_)
        for (auto& t : lane)
            t = Track {};

    numTracks_.fill (0);
    numPeaks_  = 0;
    frameCounter_ = 0;
}

void FeedbackDetector::process (const float* const* channels, int numChannels, int numSamples, bool linked) noexcept
{
    numPeaks_ = 0;

    if (numSamples <= 0 || channels == nullptr || numChannels <= 0)
        return;

    const int lanes = linked ? 1 : juce::jmin (numChannels, kMaxChannels);

    for (int lane = 0; lane < lanes; ++lane)
    {
        const float* src = channels[lane];
        if (src == nullptr)
            continue;

        auto& ring = ring_[lane];
        int w = ringWrite_[lane];

        for (int n = 0; n < numSamples; ++n)
        {
            float x = src[n];
            if (! std::isfinite (x))
                x = 0.0f;

            ring[(size_t) w] = x;
            w = (w + 1) % fftSize_;
            ++sinceHop_[lane];

            if (sinceHop_[lane] >= hopSize_)
            {
                sinceHop_[lane] -= hopSize_;
                ringWrite_[lane] = w;
                analyseFrame (lane, frameCounter_++);
            }
        }

        ringWrite_[lane] = w;
    }

    for (int ch = 0; ch < numChannels_ && numPeaks_ < kMaxPeaks; ++ch)
    {
        for (int t = 0; t < numTracks_[(size_t) ch] && numPeaks_ < kMaxPeaks; ++t)
        {
            const auto& tr = tracks_[(size_t) ch][(size_t) t];
            if (! tr.active)
                continue;

            DetectedPeak p;
            p.frequencyHz  = tr.frequency;
            p.confidence   = tr.confidence;
            p.prominenceDb = tr.prominence;
            p.q            = tr.q;
            p.levelDb      = tr.levelDb;
            p.channel      = tr.channel;
            peaks_[(size_t) numPeaks_++] = p;
        }
    }
}

void FeedbackDetector::analyseFrame (int channel, int frameIndex) noexcept
{
    const auto& ring = ring_[channel];
    const int w = ringWrite_[channel];

    for (int n = 0; n < fftSize_; ++n)
        fftData_[(size_t) n] = ring[(size_t) ((w + n) % fftSize_)] * window_[(size_t) n];

    for (int n = fftSize_; n < fftSize_ * 2; ++n)
        fftData_[(size_t) n] = 0.0f;

    fft_->performRealOnlyForwardTransform (fftData_.data(), true);

    auto& floorDb = floorDb_[channel];

    const float norm = 2.0f / (float) fftSize_;

    for (int b = 0; b < numBins_; ++b)
    {
        const float re = fftData_[(size_t) (2 * b)];
        const float im = fftData_[(size_t) (2 * b + 1)];
        const float mag = std::sqrt (re * re + im * im) * norm;
        const float db  = juce::Decibels::gainToDecibels (mag, kMinDb);

        magDb_[(size_t) b] = db;
    }

    // Spectral floor: the mean level of two narrow shoulder regions a
    // frequency-proportional distance either side of each bin. Taking the
    // lower shoulder keeps a narrow tone on a slope detectable, while
    // broadband material has shoulders as loud as the bin itself.
    {
        for (int b = 0; b < numBins_; ++b)
        {
            const int delta = juce::jlimit (config_.floorOffsetBins, config_.floorOffsetMaxBins, b / 12);

            auto shoulderMean = [&] (int from, int to)
            {
                const int lo = juce::jlimit (0, numBins_ - 1, from);
                const int hi = juce::jlimit (0, numBins_ - 1, to);
                float sum = 0.0f;
                int count = 0;
                for (int k = lo; k <= hi; ++k)
                {
                    sum += magDb_[(size_t) k];
                    ++count;
                }
                return count > 0 ? sum / (float) count : magDb_[(size_t) b];
            };

            const float lo = shoulderMean (b - delta - 3, b - delta);
            const float hi = shoulderMean (b + delta, b + delta + 3);
            floorDb[(size_t) b] = juce::jmin (lo, hi);
        }
    }

    framePeaks_.clear();

    const int loBin = juce::jmax (config_.guardBins, hzToBin (config_.minFrequencyHz, sampleRate_, fftSize_));
    const int hiBin = juce::jmin (numBins_ - 1 - config_.guardBins, hzToBin (config_.maxFrequencyHz, sampleRate_, fftSize_));

    const float binHz = (float) (sampleRate_ / (double) fftSize_);

    for (int b = loBin; b <= hiBin; ++b)
    {
        const float mag = magDb_[(size_t) b];

        if (mag <= magDb_[(size_t) juce::jmax (0, b - 1)] || mag < magDb_[(size_t) juce::jmin (numBins_ - 1, b + 1)])
            continue;

        const float prominence = mag - floorDb[(size_t) b];
        if (prominence < config_.minProminenceDb)
            continue;

        const float widthBins = estimateWidthBins (b);
        const float narrow = juce::jlimit (0.0f, 1.0f,
            1.0f - widthBins / juce::jmax (1.0f, config_.maxPeakWidthBins));

        const float growthDb = juce::jlimit (-12.0f, 24.0f, mag - prevMagDb_[(size_t) channel][(size_t) b]);
        const float growth = juce::jlimit (0.0f, 1.0f, growthDb / 6.0f);

        const float prom = juce::jlimit (0.0f, 1.0f,
            (prominence - config_.minProminenceDb) / juce::jmax (1.0f, config_.strongProminenceDb - config_.minProminenceDb));

        const float q = juce::jlimit (config_.minQ, config_.maxQ,
            (float) b / juce::jmax (1.0f, widthBins));

        (void) binHz;

        FramePeak fp;
        fp.bin          = b;
        fp.magDb        = mag;
        fp.prominenceDb = prominence;
        fp.q            = q;
        fp.prom01       = prom;
        fp.narrow01     = narrow;
        fp.growth01     = growth;
        framePeaks_.push_back (fp);
    }

    // Second pass: score harmonicity against the COMPLETE set of frame peaks.
    // (Scoring while peaks are still being collected meant a fundamental never
    // saw its own harmonics, so harmonic musical notes were not discounted.)
    for (auto& fp : framePeaks_)
    {
        // Only bother with harmonicity for peaks that stand meaningfully above
        // the floor; weak peaks cannot activate anyway and this keeps the
        // O(peaks^2) scoring cheap on broadband material.
        const float harmonic = fp.prom01 > 0.05f ? harmonicityFactor (fp.bin, channel) : 1.0f;
        fp.inst = juce::jlimit (0.0f, 1.0f,
            (0.5f * fp.prom01 + 0.3f * fp.narrow01 + 0.2f * fp.growth01) * harmonic);
    }

    std::copy (magDb_.begin(), magDb_.end(), prevMagDb_[(size_t) channel].begin());

    updateTracksForChannel (channel, frameIndex);
}

float FeedbackDetector::estimateWidthBins (int peakBin) const noexcept
{
    const float peak = magDb_[(size_t) peakBin];
    const float target = peak - 6.0f;

    const int maxWalk = 32; // cap: protects against a flat/noisy -6 dB plateau

    int left = peakBin;
    while (left > 0 && (peakBin - left) < maxWalk && magDb_[(size_t) (left - 1)] > target)
        --left;

    int right = peakBin;
    while (right < numBins_ - 1 && (right - peakBin) < maxWalk && magDb_[(size_t) (right + 1)] > target)
        ++right;

    return (float) (right - left + 1);
}

float FeedbackDetector::harmonicityFactor (int peakBin, int channel) const noexcept
{
    const float f0 = binToHz (peakBin, sampleRate_, fftSize_);
    if (f0 <= 0.0f)
        return 1.0f;

    constexpr int kMaxHarmonic = 12;
    int relatives = 0;
    int strongPeaks = 0;

    for (const auto& fp : framePeaks_)
    {
        if (fp.bin == peakBin || fp.prominenceDb < config_.minProminenceDb)
            continue;

        // Ignore the immediate skirt of this peak.
        if (std::abs (fp.bin - peakBin) < 2)
            continue;

        // Only a relative close in level to the peak counts at all; weak noise
        // peaks must not be mistaken for a harmonic series. Both the harmonic
        // count and its denominator use this comparable-level set.
        if (fp.magDb < magDb_[(size_t) peakBin] - config_.harmonicWindowDb)
            continue;

        ++strongPeaks;

        const float f = binToHz (fp.bin, sampleRate_, fftSize_);
        if (f <= 0.0f)
            continue;

        // Any small-integer ratio counts, not just integer multiples: a
        // partial harmonic subset (e.g. odd harmonics of a note) is still a
        // harmonic series and must be discounted.
        // O(K) check for a small-integer ratio p/q: for each q, test whether
        // ratio*q is close to an integer p in range. This is equivalent to the
        // p/q double loop but ~K times cheaper.
        const float ratio = f / f0;
        bool related = false;
        for (int q = 1; q <= kMaxHarmonic && ! related; ++q)
        {
            const float p = std::round (ratio * (float) q);
            if (p < 1.0f || p > (float) kMaxHarmonic)
                continue;

            const float rr = p / (float) q;
            if (std::abs (ratio - rr) <= config_.harmonicTolerance * rr)
                related = true;
        }

        if (related)
            ++relatives;
    }

    (void) channel;

    // Leave a tone with at most one harmonic relative alone (it could be
    // feedback); discount a richer harmonic series hard, as musical notes and
    // voiced speech are harmonic.
    if (strongPeaks < 2 || relatives < 2)
        return 1.0f;

    const float frac = juce::jlimit (0.0f, 1.0f, (float) relatives / (float) strongPeaks);
    return juce::jlimit (0.1f, 1.0f, 1.0f - config_.harmonicityPenalty * frac * 1.5f);
}

void FeedbackDetector::updateTracksForChannel (int channel, int frameIndex) noexcept
{
    // Each channel owns its own track table, so two channels never compete
    // for slots and cannot evict each other's real tracks.
    auto& channelTracks = tracks_[(size_t) channel];
    int&  channelCount  = numTracks_[(size_t) channel];

    std::array<bool, kMaxTracks> matched {};
    std::fill (matched.begin(), matched.end(), false);

    for (const auto& fp : framePeaks_)
    {
        const float freq = binToHz (fp.bin, sampleRate_, fftSize_);

        int best = -1;
        float bestDist = 1.0e9f;
        for (int t = 0; t < channelCount; ++t)
        {
            const float dist = std::abs (channelTracks[(size_t) t].frequency - freq) / juce::jmax (20.0f, freq);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = t;
            }
        }

        const float tolerance = 0.03f;

        if (best >= 0 && bestDist <= tolerance)
        {
            auto& tr = channelTracks[(size_t) best];
            matched[(size_t) best] = true;

            // Heavy smoothing keeps a settled notch from being re-tuned or
            // re-levelled every frame; that modulation is audible as grit.
            tr.frequency  = tr.frequency * 0.88f + freq * 0.12f;
            tr.prominence = tr.prominence * 0.8f + fp.prominenceDb * 0.2f;
            tr.q          = tr.q * 0.88f + fp.q * 0.12f;
            tr.levelDb    = tr.levelDb * 0.8f + fp.magDb * 0.2f;
            tr.confidence = tr.confidence * 0.9f + fp.inst * 0.1f;
            tr.lastSeenFrame = frameIndex;

            if (fp.inst >= config_.offConfidence)
            {
                ++tr.onStreak;
                tr.offStreak = 0;
            }
            else
            {
                ++tr.offStreak;
                tr.onStreak = 0;
            }

            // Track growth evidence so a steady non-growing tone can be held
            // off in the Conservative profile.
            if (fp.growth01 >= config_.growthThreshold01)
                ++tr.growthStreak;
            else if (tr.growthStreak > 0)
                --tr.growthStreak;

            if (! tr.active)
                tr.active = tr.onStreak >= config_.onFrames
                         && tr.confidence >= config_.onConfidence
                         && (! config_.requireGrowth || tr.growthStreak >= config_.growthFrames);
            else if (tr.confidence < config_.offConfidence && tr.offStreak >= config_.offFrames)
                tr.active = false;
        }
        else
        {
            // Do not let broadband noise peaks flood the track table. Only
            // start tracking a peak that is already reasonably strong, and if
            // the table is full evict the weakest track so a real feedback
            // tone can always be tracked through broadband material.
            if (fp.inst >= config_.onConfidence * 0.5f)
            {
                int target = -1;

                if (channelCount < kMaxTracks)
                {
                    target = channelCount;
                    ++channelCount;
                }
                else
                {
                    float weakest = fp.inst;
                    for (int t = 0; t < channelCount; ++t)
                    {
                        if (! matched[(size_t) t] && channelTracks[(size_t) t].confidence < weakest)
                        {
                            weakest = channelTracks[(size_t) t].confidence;
                            target = t;
                        }
                    }
                }

                if (target >= 0)
                {
                    auto& tr = channelTracks[(size_t) target];
                    tr = Track {};
                    tr.channel       = channel;
                    tr.frequency     = freq;
                    tr.prominence    = fp.prominenceDb;
                    tr.q             = fp.q;
                    tr.levelDb       = fp.magDb;
                    tr.confidence    = fp.inst * 0.5f;
                    tr.lastSeenFrame = frameIndex;
                    tr.onStreak      = 1;
                    tr.growthStreak  = (fp.growth01 >= config_.growthThreshold01) ? 1 : 0;
                    matched[(size_t) target] = true;
                }
            }
        }
    }

    for (int t = 0; t < channelCount; ++t)
    {
        auto& tr = channelTracks[(size_t) t];
        if (matched[(size_t) t])
            continue;

        tr.confidence *= 0.85f;
        ++tr.offStreak;
        tr.onStreak = 0;

        if (tr.confidence < config_.offConfidence && tr.offStreak >= config_.offFrames)
            tr.active = false;
    }

    int write = 0;
    for (int t = 0; t < channelCount; ++t)
    {
        const auto& tr = channelTracks[(size_t) t];
        const bool stale = (frameIndex - tr.lastSeenFrame) > (config_.offFrames * 2);
        if (tr.active || ! stale)
        {
            if (write != t)
                channelTracks[(size_t) write] = tr;
            ++write;
        }
    }
    channelCount = write;
}

} // namespace defeedback
