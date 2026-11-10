#pragma once

#include "dsp/DetectorConfig.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <vector>

namespace defeedback
{

/** A single likely-feedback peak proposed by the detector. */
struct DetectedPeak
{
    float frequencyHz  = 0.0f;
    float confidence   = 0.0f;   // 0..1
    float prominenceDb = 0.0f;
    float q            = 20.0f;  // estimated from peak width
    float levelDb      = -120.0f;
    int   channel      = 0;      // 0/1 for independent analysis, 0 for linked
};

/** Deterministic spectral feedback detector.

    Features used: persistent narrow-band energy, prominence above a tracked
    per-bin spectral floor, spectral narrowness (Q estimate) and growth rate,
    with a harmonicity check that discounts tones resembling a harmonic
    series. Candidates require persistence with hysteresis so notch filters do
    not chatter.

    This is explicitly NOT a trained model. v0.0.1 ships a classical
    deterministic detector; see FeedbackClassifier.h for the interface
    intended for a future learned classifier.

    Thread-safety: prepare()/reset()/process() are audio-thread safe once
    prepared. The whole object is single-threaded; UI reads must go through
    the processor's atomics, never through this object directly.
*/
class FeedbackDetector
{
public:
    FeedbackDetector();

    void prepare (double sampleRate, int numChannels, const DetectorConfig& config);
    void reset() noexcept;

    /** RT-safe. Config changes are copied without allocation. */
    void setConfig (const DetectorConfig& config) noexcept { config_ = config; }
    const DetectorConfig& getConfig() const noexcept { return config_; }

    /** Feed an analysis tap. `channels` may be null when numChannels is 0.
        When linked is true a pre-summed mono signal is expected in channels[0]
        and only that lane is analysed. Allocation-free and bounded. */
    void process (const float* const* channels, int numChannels, int numSamples, bool linked) noexcept;

    int getNumPeaks() const noexcept { return numPeaks_; }
    const DetectedPeak& getPeak (int index) const noexcept { return peaks_[(size_t) index]; }

private:
    static constexpr int kMaxPeaks    = 16;
    static constexpr int kMaxTracks   = 24;
    static constexpr int kMaxChannels = 2;

    struct Track
    {
        bool  active      = false;
        float frequency   = 0.0f;
        float confidence  = 0.0f;
        float prominence  = 0.0f;
        float q           = 20.0f;
        float levelDb     = -120.0f;
        int   channel     = 0;
        int   onStreak    = 0;
        int   offStreak   = 0;
        int   lastSeenFrame = 0;
    };

    void analyseFrame (int channel, int frameIndex) noexcept;
    void updateTracksForChannel (int channel, int frameIndex) noexcept;
    float estimateWidthBins (int peakBin) const noexcept;
    float harmonicityFactor (int peakBin, int channel) const noexcept;

    DetectorConfig config_ {};
    double sampleRate_ = 48000.0;
    int    numChannels_ = 2;
    int    fftSize_ = 2048;
    int    fftOrder_ = 11;
    int    hopSize_ = 1024;
    int    numBins_ = 0;

    std::unique_ptr<juce::dsp::FFT> fft_;

    // Per-channel circular analysis buffers (preallocated).
    std::array<std::vector<float>, kMaxChannels> ring_;
    std::array<int, kMaxChannels> ringWrite_ {};
    std::array<int, kMaxChannels> sinceHop_ {};

    std::vector<float> window_;
    std::vector<float> fftData_;          // 2 * fftSize
    std::vector<float> magDb_;            // numBins
    std::vector<float> prevMagDb_;        // numBins, for growth
    std::array<std::vector<float>, kMaxChannels> floorDb_;

    // Per-frame scratch, preallocated.
    struct FramePeak
    {
        int   bin = 0;
        float magDb = -120.0f;
        float prominenceDb = 0.0f;
        float q = 20.0f;
        float prom01 = 0.0f;
        float narrow01 = 0.0f;
        float growth01 = 0.0f;
        float inst = 0.0f;
    };
    std::vector<FramePeak> framePeaks_;

    std::array<Track, kMaxTracks> tracks_ {};
    int numTracks_ = 0;
    int frameCounter_ = 0;

    std::array<DetectedPeak, kMaxPeaks> peaks_ {};
    int numPeaks_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeedbackDetector)
};

} // namespace defeedback
