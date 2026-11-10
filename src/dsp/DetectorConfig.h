#pragma once

namespace defeedback
{

/** Tunable parameters for the deterministic spectra detector.

    All values are conservative by default: the detector would rather miss a
    marginal event than cut a legitimate musical tone.
*/
struct DetectorConfig
{
    // Analysis resolution.
    int   fftOrder            = 11;      // 2048-point FFT
    int   overlapFactor       = 2;       // 50% overlap (hop = fftSize/2)

    // Frequency region of interest.
    float minFrequencyHz      = 80.0f;
    float maxFrequencyHz      = 18000.0f;

    // Frequency-local spectral floor: the level of two narrow shoulder regions
    // a mildly frequency-proportional distance either side of each bin. A
    // narrow tone sits well above its shoulders; broadband material has
    // shoulders as loud as the bin itself, so it produces no prominence.
    int   floorOffsetBins     = 6;
    int   floorOffsetMaxBins  = 32;

    // Peak prominence above the tracked spectral floor.
    float minProminenceDb     = 10.0f;
    float strongProminenceDb  = 22.0f;

    // Narrowness: a feedback peak is narrow relative to its neighbourhood.
    float maxPeakWidthBins    = 6.0f;

    // Persistence / hysteresis.
    int   onFrames            = 5;       // frames above score to activate
    int   offFrames           = 40;      // frames below score to release
    float onConfidence        = 0.55f;
    float offConfidence       = 0.30f;

    // Growth rate damping: a peak that is not growing is less likely feedback.
    float growthWeight        = 0.35f;

    // Growth-evidence gate. With this on, a track must show positive growth
    // for `growthFrames` frames before it activates, so a steady non-growing
    // tone (flute, organ, sustained pad) is not notched. Feedback onset grows
    // and passes. Used by the Conservative profile.
    bool  requireGrowth       = false;
    int   growthFrames        = 4;
    float growthThreshold01   = 0.15f;

    // Harmonicity: penalise tones that look like a harmonic series. A relative
    // only counts if it is within this window of the peak's own level, so weak
    // noise peaks cannot masquerade as harmonics of a real tone.
    float harmonicityPenalty  = 0.45f;
    float harmonicTolerance   = 0.05f;   // relative frequency tolerance
    float harmonicWindowDb    = 15.0f;

    // Ignore bins very close to DC / Nyquist.
    int   guardBins           = 2;

    // Estimated notch Q from peak width, clamped by the bank.
    float minQ                = 8.0f;
    float maxQ                = 45.0f;
};

} // namespace defeedback
