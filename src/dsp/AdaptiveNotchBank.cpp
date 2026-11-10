#include "dsp/AdaptiveNotchBank.h"

#include <algorithm>
#include <cmath>

namespace defeedback
{

namespace
{
    inline double smoothingAlpha (int numSamples, double sampleRate, double tauSeconds) noexcept
    {
        if (tauSeconds <= 0.0 || sampleRate <= 0.0)
            return 1.0;
        const double x = (double) numSamples / (sampleRate * tauSeconds);
        return std::clamp (1.0 - std::exp (-x), 0.0, 1.0);
    }

    constexpr int kRetireFrames = 30;
}

void AdaptiveNotchBank::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    sampleRate_  = sampleRate > 0.0 ? sampleRate : 48000.0;
    numChannels_ = juce::jlimit (1, kMaxLanes, numChannels);
    reset();
}

void AdaptiveNotchBank::reset() noexcept
{
    for (auto& lane : slots_)
        for (auto& s : lane)
            s = Slot {};

    arrayIndexCounter_ = 0;
}

void AdaptiveNotchBank::clearAll() noexcept
{
    for (auto& lane : slots_)
    {
        for (auto& s : lane)
        {
            if (s.allocated)
            {
                s.retiring = true;
                s.targetGainDb = 0.0;
            }
        }
    }
}

AdaptiveNotchBank::Slot* AdaptiveNotchBank::findSlotNear (int lane, float freq) noexcept
{
    auto& slots = slots_[(size_t) lane];
    Slot* best = nullptr;
    float bestDist = 1.0e9f;

    for (auto& s : slots)
    {
        if (! s.allocated || s.retiring)
            continue;

        const float dist = std::abs ((float) s.currentFreq - freq) / juce::jmax (20.0f, freq);
        if (dist <= config_.allocationTolerance && dist < bestDist)
        {
            bestDist = dist;
            best = &s;
        }
    }

    return best;
}

AdaptiveNotchBank::Slot* AdaptiveNotchBank::findFreeSlot (int lane) noexcept
{
    for (auto& s : slots_[(size_t) lane])
        if (! s.allocated)
            return &s;

    return nullptr;
}

AdaptiveNotchBank::Slot* AdaptiveNotchBank::findRecyclableSlot (int lane) noexcept
{
    Slot* best = nullptr;
    float bestScore = 1.0e9f;

    for (auto& s : slots_[(size_t) lane])
    {
        if (! s.allocated)
            continue;

        const float score = s.retiring ? -1.0f : s.confidence;
        if (score < bestScore)
        {
            bestScore = score;
            best = &s;
        }
    }

    return best;
}

void AdaptiveNotchBank::update (const DetectedPeak* peaks, int numPeaks) noexcept
{
    for (auto& lane : slots_)
        for (auto& s : lane)
            if (s.allocated)
                ++s.staleFrames;

    if (freeze_)
        return;

    const int maxPerLane = juce::jlimit (1, kMaxNotchesPerLane, config_.maxNotchesPerLane);
    const int lanes = config_.linked ? 1 : juce::jmin (numChannels_, kMaxLanes);

    (void) maxPerLane;

    for (int i = 0; i < numPeaks; ++i)
    {
        const auto& peak = peaks[i];

        const int lane = config_.linked ? 0 : juce::jlimit (0, lanes - 1, peak.channel);

        float freq = juce::jlimit (config_.minFrequencyHz, config_.maxFrequencyHz, peak.frequencyHz);
        freq = juce::jlimit (10.0f, (float) (sampleRate_ * 0.49), freq);

        const float q = juce::jlimit (3.0f, 60.0f, peak.q * juce::jlimit (0.5f, 2.0f, config_.qBias));

        const float conf = juce::jlimit (0.0f, 1.0f, peak.confidence);
        const float cut = juce::jlimit (0.0f, juce::jmin (config_.maxCutDb, kHardMaxCutDb),
                                        config_.maxCutDb * conf * juce::jlimit (0.25f, 2.0f, config_.strength));

        auto* slot = findSlotNear (lane, freq);

        if (slot == nullptr)
        {
            slot = findFreeSlot (lane);
            if (slot == nullptr)
                slot = findRecyclableSlot (lane);

            if (slot != nullptr)
            {
                const int laneOfSlot = lane;
                slot->filter.reset();
                slot->currentFreq = slot->targetFreq = (double) freq;
                slot->currentQ    = slot->targetQ    = (double) q;
                slot->currentGainDb = 0.0;
                slot->targetGainDb  = (double) -cut;
                slot->allocated = true;
                slot->retiring  = false;
                slot->lane      = laneOfSlot;
            }
        }

        if (slot != nullptr)
        {
            slot->targetFreq  = (double) freq;
            slot->targetQ     = (double) q;
            slot->targetGainDb = (double) -cut;
            slot->confidence  = conf;
            slot->staleFrames = 0;
        }
    }

    for (auto& lane : slots_)
    {
        for (auto& s : lane)
        {
            if (s.allocated && ! s.retiring && s.staleFrames > kRetireFrames)
            {
                s.retiring = true;
                s.targetGainDb = 0.0;
            }
        }
    }
}

void AdaptiveNotchBank::updateSmoothing (int numSamples) noexcept
{
    const double aFreq = smoothingAlpha (numSamples, sampleRate_, config_.frequencySeconds);
    const double aAttack = smoothingAlpha (numSamples, sampleRate_, config_.attackSeconds);
    const double aRelease = smoothingAlpha (numSamples, sampleRate_, config_.releaseSeconds);

    for (auto& lane : slots_)
    {
        for (auto& s : lane)
        {
            if (! s.allocated)
                continue;

            s.currentFreq += (s.targetFreq - s.currentFreq) * aFreq;
            s.currentQ    += (s.targetQ - s.currentQ) * aFreq;

            const double aGain = (std::abs (s.targetGainDb) > std::abs (s.currentGainDb)) ? aAttack : aRelease;
            s.currentGainDb += (s.targetGainDb - s.currentGainDb) * aGain;

            if (s.retiring && std::abs (s.currentGainDb) < 0.01)
            {
                s.filter.reset();
                s = Slot {};
                continue;
            }

            const auto coeffs = Biquad::makePeak (sampleRate_, s.currentFreq, s.currentQ, s.currentGainDb);

            if (std::isfinite (coeffs.b0) && std::isfinite (coeffs.b1) && std::isfinite (coeffs.b2)
                && std::isfinite (coeffs.a1) && std::isfinite (coeffs.a2))
                s.filter.setCoeffs (coeffs);
            else
                s.filter.setCoeffs (Biquad::makeIdentity());
        }
    }
}

void AdaptiveNotchBank::processLane (int lane, int channel, float* data, int numSamples) noexcept
{
    if (channel < 0 || data == nullptr)
        return;

    auto& laneSlots = slots_[(size_t) lane];

    for (auto& s : laneSlots)
    {
        if (! s.allocated)
            continue;

        for (int n = 0; n < numSamples; ++n)
            data[n] = (float) s.filter.processSample ((double) data[n]);

        s.filter.sanitiseState();
    }
}

void AdaptiveNotchBank::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    updateSmoothing (numSamples);

    if (config_.linked)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            processLane (0, ch, channels[ch], numSamples);
    }
    else
    {
        const int lanes = juce::jmin (numChannels, kMaxLanes);
        for (int lane = 0; lane < lanes; ++lane)
            processLane (lane, lane, channels[lane], numSamples);
    }
}

int AdaptiveNotchBank::getActiveCount() const noexcept
{
    int count = 0;
    for (const auto& lane : slots_)
        for (const auto& s : lane)
            if (s.allocated && ! s.retiring)
                ++count;

    return count;
}

bool AdaptiveNotchBank::getNotchInfo (int index, NotchInfo& out) const noexcept
{
    if (index < 0)
        return false;

    int seen = 0;
    for (const auto& lane : slots_)
    {
        for (const auto& s : lane)
        {
            if (! s.allocated || s.retiring)
                continue;

            if (seen == index)
            {
                out.frequencyHz = (float) s.currentFreq;
                out.q           = (float) s.currentQ;
                out.gainDb      = (float) s.currentGainDb;
                out.confidence  = s.confidence;
                out.lane        = s.lane;
                return true;
            }

            ++seen;
        }
    }

    return false;
}

} // namespace defeedback
