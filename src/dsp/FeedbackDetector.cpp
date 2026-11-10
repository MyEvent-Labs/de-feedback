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
    prevMagDb_.assign ((size_t) numBins_, kMinDb);

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
    std::fill (prevMagDb_.begin(), prevMagDb_.end(), kMinDb);

    for (auto& t : tracks_)
        t = Track {};

    numTracks_ = 0;
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

    for (int t = 0; t < numTracks_ && numPeaks_ < kMaxPeaks; ++t)
    {
        const auto& tr = tracks_[t];
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

        if (db < floorDb[(size_t) b])
            floorDb[(size_t) b] = db;
        else
            floorDb[(size_t) b] += (db - floorDb[(size_t) b]) * 0.02f;

        magDb_[(size_t) b] = db;
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

        const float growthDb = juce::jlimit (-12.0f, 24.0f, mag - prevMagDb_[(size_t) b]);
        const float growth = juce::jlimit (0.0f, 1.0f, growthDb / 6.0f);

        const float prom = juce::jlimit (0.0f, 1.0f,
            (prominence - config_.minProminenceDb) / juce::jmax (1.0f, config_.strongProminenceDb - config_.minProminenceDb));

        const float harmonic = harmonicityFactor (b, channel);

        float inst = 0.5f * prom + 0.3f * narrow + 0.2f * growth;
        inst *= harmonic;
        inst = juce::jlimit (0.0f, 1.0f, inst);

        const float q = juce::jlimit (config_.minQ, config_.maxQ,
            (float) b / juce::jmax (1.0f, widthBins));

        (void) binHz;

        FramePeak fp;
        fp.bin          = b;
        fp.magDb        = mag;
        fp.prominenceDb = prominence;
        fp.q            = q;
        fp.inst         = inst;
        framePeaks_.push_back (fp);
    }

    std::copy (magDb_.begin(), magDb_.end(), prevMagDb_.begin());

    updateTracksForChannel (channel, frameIndex);
}

float FeedbackDetector::estimateWidthBins (int peakBin) const noexcept
{
    const float peak = magDb_[(size_t) peakBin];
    const float target = peak - 6.0f;

    int left = peakBin;
    while (left > 0 && magDb_[(size_t) (left - 1)] > target)
        --left;

    int right = peakBin;
    while (right < numBins_ - 1 && magDb_[(size_t) (right + 1)] > target)
        ++right;

    return (float) (right - left + 1);
}

float FeedbackDetector::harmonicityFactor (int peakBin, int channel) const noexcept
{
    const float f0 = binToHz (peakBin, sampleRate_, fftSize_);
    if (f0 <= 0.0f)
        return 1.0f;

    const int peers = 8;
    int relatives = 0;
    int strongPeaks = 0;

    for (const auto& fp : framePeaks_)
    {
        if (fp.bin == peakBin)
            continue;

        const float f = binToHz (fp.bin, sampleRate_, fftSize_);
        if (f <= 0.0f)
            continue;

        if (fp.prominenceDb >= config_.minProminenceDb)
            ++strongPeaks;

        const float ratio = f / f0;
        for (int k = 2; k <= peers; ++k)
        {
            const float near = (float) k;
            if (std::abs (ratio - near) <= config_.harmonicTolerance * near
                && fp.prominenceDb >= config_.minProminenceDb * 1.5f)
            {
                ++relatives;
                break;
            }

            const float sub = 1.0f / (float) k;
            if (std::abs (ratio - sub) <= config_.harmonicTolerance
                && fp.prominenceDb >= config_.minProminenceDb * 1.5f)
            {
                ++relatives;
                break;
            }
        }
    }

    (void) channel;

    if (strongPeaks < 2 || relatives == 0)
        return 1.0f;

    const float frac = juce::jlimit (0.0f, 1.0f, (float) relatives / (float) peers);
    return juce::jlimit (0.1f, 1.0f, 1.0f - config_.harmonicityPenalty * frac);
}

void FeedbackDetector::updateTracksForChannel (int channel, int frameIndex) noexcept
{
    std::array<bool, kMaxTracks> matched {};
    std::fill (matched.begin(), matched.end(), false);

    for (const auto& fp : framePeaks_)
    {
        const float freq = binToHz (fp.bin, sampleRate_, fftSize_);

        int best = -1;
        float bestDist = 1.0e9f;
        for (int t = 0; t < numTracks_; ++t)
        {
            const auto& tr = tracks_[(size_t) t];
            if (tr.channel != channel)
                continue;

            const float dist = std::abs (tr.frequency - freq) / juce::jmax (20.0f, freq);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = t;
            }
        }

        const float tolerance = 0.03f;

        if (best >= 0 && bestDist <= tolerance)
        {
            auto& tr = tracks_[(size_t) best];
            matched[(size_t) best] = true;

            tr.frequency  = tr.frequency * 0.7f + freq * 0.3f;
            tr.prominence = tr.prominence * 0.6f + fp.prominenceDb * 0.4f;
            tr.q          = tr.q * 0.7f + fp.q * 0.3f;
            tr.levelDb    = tr.levelDb * 0.6f + fp.magDb * 0.4f;
            tr.confidence = tr.confidence * 0.6f + fp.inst * 0.4f;
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

            if (! tr.active)
                tr.active = tr.onStreak >= config_.onFrames && tr.confidence >= config_.onConfidence;
            else if (tr.confidence < config_.offConfidence && tr.offStreak >= config_.offFrames)
                tr.active = false;
        }
        else if (numTracks_ < kMaxTracks)
        {
            auto& tr = tracks_[(size_t) numTracks_];
            tr = Track {};
            tr.channel       = channel;
            tr.frequency     = freq;
            tr.prominence    = fp.prominenceDb;
            tr.q             = fp.q;
            tr.levelDb       = fp.magDb;
            tr.confidence    = fp.inst * 0.5f;
            tr.lastSeenFrame = frameIndex;
            tr.onStreak      = 1;
            matched[(size_t) numTracks_] = true;
            ++numTracks_;
        }
    }

    for (int t = 0; t < numTracks_; ++t)
    {
        auto& tr = tracks_[(size_t) t];
        if (matched[(size_t) t])
            continue;

        tr.confidence *= 0.85f;
        ++tr.offStreak;
        tr.onStreak = 0;

        if (tr.confidence < config_.offConfidence && tr.offStreak >= config_.offFrames)
            tr.active = false;
    }

    int write = 0;
    for (int t = 0; t < numTracks_; ++t)
    {
        const auto& tr = tracks_[(size_t) t];
        const bool stale = (frameIndex - tr.lastSeenFrame) > (config_.offFrames * 2);
        if (tr.active || ! stale)
        {
            if (write != t)
                tracks_[(size_t) write] = tr;
            ++write;
        }
    }
    numTracks_ = write;
}

} // namespace defeedback
