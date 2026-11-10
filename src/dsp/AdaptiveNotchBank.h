#pragma once

#include "dsp/Biquad.h"
#include "dsp/FeedbackDetector.h"

#include <array>

namespace defeedback
{

/** Read-only view of one active notch, for UI/metering. */
struct NotchInfo
{
    float frequencyHz = 0.0f;
    float q           = 20.0f;
    float gainDb      = 0.0f;
    float confidence  = 0.0f;
    int   lane        = 0;
};

/** Bank of adaptive peaking/notch filters driven by detector candidates.

    Design notes
    ------------
    - Every allocated slot owns a Biquad. Unallocated slots are skipped
      entirely, so an idle bank is bit-transparent.
    - Parameter changes are smoothed per block; slots that are (re)allocated
      have their filter state reset so no stale energy surfaces as a click.
    - A slot retires by ramping its gain to zero, then is freed. Allocation
      never hard-jumps an in-use filter's depth.
    - The bank clamps to a hard per-notch limit (kHardMaxCutDb) regardless of
      what a caller requests.
*/
class AdaptiveNotchBank
{
public:
    static constexpr int kMaxNotchesPerLane = 12;
    static constexpr int kMaxLanes          = 2;
    static constexpr float kHardMaxCutDb    = 18.0f;

    struct Config
    {
        int   maxNotchesPerLane   = 12;
        bool  linked              = true;
        float minFrequencyHz      = 80.0f;
        float maxFrequencyHz      = 18000.0f;
        float maxCutDb            = 9.0f;    // user + mode limit, <= 18
        float strength            = 1.0f;    // 0.25 .. 2.0
        float qBias               = 1.0f;    // 0.5 .. 2.0
        float attackSeconds       = 0.02f;   // cut increases
        float releaseSeconds      = 4.0f;    // cut decreases / retire
        float frequencySeconds    = 0.05f;   // frequency smoothing
        float allocationTolerance = 0.05f;   // relative frequency match
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;
    void setConfig (const Config& config) noexcept { config_ = config; }
    const Config& getConfig() const noexcept { return config_; }
    void setFreeze (bool shouldFreeze) noexcept { freeze_ = shouldFreeze; }
    bool isFrozen() const noexcept { return freeze_; }

    /** Controller: assign / refresh / retire slots from detector peaks.
        RT-safe (no allocation). */
    void update (const DetectedPeak* peaks, int numPeaks) noexcept;

    /** In-place processing. RT-safe. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Ramp all notches out and free them. RT-safe. */
    void clearAll() noexcept;

    int getActiveCount() const noexcept;
    bool getNotchInfo (int index, NotchInfo& out) const noexcept;

private:
    struct Slot
    {
        Biquad filter;
        double targetFreq = 1000.0, currentFreq = 1000.0;
        double targetQ = 20.0, currentQ = 20.0;
        double targetGainDb = 0.0, currentGainDb = 0.0;
        float  confidence = 0.0f;
        int    staleFrames = 0;
        bool   allocated = false;
        bool   retiring = false;
        int    lane = 0;
    };

    void updateSmoothing (int numSamples) noexcept;
    void processLane (int lane, int channel, float* data, int numSamples) noexcept;
    Slot* findSlotNear (int lane, float freq) noexcept;
    Slot* findFreeSlot (int lane) noexcept;
    Slot* findRecyclableSlot (int lane) noexcept;

    Config config_ {};
    double sampleRate_ = 48000.0;
    int    numChannels_ = 2;
    bool   freeze_ = false;
    int    arrayIndexCounter_ = 0;

    std::array<std::array<Slot, kMaxNotchesPerLane>, kMaxLanes> slots_ {};
};

} // namespace defeedback
