# DSP Design

This document describes what v0.0.1 actually does. It is deliberately modest:
the alpha is a classical deterministic feedback suppressor, not a trained model.

## Signal path

```
Input
  -> denormal protection (ScopedNoDenormals) + NaN/Inf sanitiser
  -> input trim (smoothed)
  -> analysis tap (pre-suppression)
  -> likelihood detector (FFT features + persistence)
  -> adaptive notch controller (allocate / refresh / retire)
  -> smoothed notch filter bank
  -> output trim (smoothed)
  -> click-free wet/dry bypass crossfade
  -> output
```

The detector taps the **input**, before suppression, so it sees the true system
behaviour rather than its own correction.

## Filter: peaking biquad

Each notch is a second-order peaking EQ (RBJ cookbook) with a negative gain, not
a true infinite-depth notch. This gives:

- unconditional stability at any depth;
- a predictable, user-facing maximum-cut control;
- a far lower Q than a deep classic notch would need.

Processing is Transposed Direct Form II in `double`. Coefficients are clamped:
frequency to `[10 Hz, 0.49·Fs]`, Q to `[0.1, 100]`, gain to `[-80, +24] dB`.
Non-finite coefficients fall back to identity.

## Detector

For each channel (or the mono sum in linked mode) a 2048-point Hann-windowed FFT
runs with 75% overlap (hop = fftSize/4), which keeps onset latency low. Each
channel keeps its own analysis state (ring buffer, spectral floor, growth
history and track table), so channels never compete for slots. Per bin:

1. **Spectral floor** is estimated per bin from the level of two narrow
   *shoulder* regions a mildly frequency-proportional distance either side of
   the bin. A narrow tone sits well above its shoulders; broadband material
   (noise, dense music) has shoulders as loud as the bin itself and so
   produces no prominence. This is frequency-local: unlike a per-bin temporal
   floor it does not creep up to a sustained tone, so a steady note keeps its
   notch instead of churning it on and off.
2. **Peak picking** selects local maxima above `minProminenceDb` over the floor
   within the 80 Hz – 18 kHz region of interest. Each peak's frequency is
   parabolically interpolated for sub-bin accuracy, and peaks are scored
   strongest-first and capped to the strongest 16 before tracking, so a
   high-frequency tone is not starved by broadband noise peaks.
3. **Features** per peak:
   - prominence above the floor;
   - narrowness (a −6 dB width estimate → a Q estimate);
   - growth (change vs the previous frame at that bin, in dB/frame), used as a
     confidence term and, in Conservative mode, as a rise-since-creation gate
     that rejects steady, non-growing tones.
4. **Harmonicity penalty** discounts a peak if several other strong frame peaks
   line up at harmonic ratios of it — musical tones and voiced speech look like
   a harmonic series, feedback usually does not.
5. **Instantaneous confidence** = `0.5·prominence + 0.3·narrowness + 0.2·growth`,
   multiplied by the harmonicity factor.

Peaks are matched to frequency tracks across frames. A track activates only
after `onFrames` consecutive frames above a threshold and confidence above
`onConfidence`; it releases after `offFrames` below `offConfidence`. This is the
hysteresis that stops chatter.

Protection mode changes the thresholds:

| Mode | onFrames | onConfidence | base prominence | cut scale | attack |
|------|----------|--------------|-----------------|-----------|--------|
| Conservative | 8 | 0.62 | 12 dB | 0.70 | 50 ms |
| Normal | 5 | 0.55 | 10 dB | 1.00 | 20 ms |
| Aggressive | 3 | 0.45 | 8 dB | 1.00 | 10 ms |

Sensitivity (−6…+6) shifts the prominence threshold: `base − sensitivity`,
clamped to `[3, 30] dB`.

## Notch bank

- Up to 12 slots per lane; one lane in linked mode (applied to all channels),
  two lanes in independent mode (one channel each).
- Unallocated slots are skipped entirely: an idle bank is bit-transparent, and
  even an allocated slot at 0 dB gain is an exact identity filter.
- Parameters (frequency, Q, gain) are smoothed per block. Gain uses a fast
  attack and a slow release.
- A newly allocated slot starts at 0 dB and ramps to its target depth, so
  adding a notch never hard-jumps a filter.
- A slot retires by ramping to 0 dB, then is reset and freed. If the bank is
  full, the lowest-confidence (or already-retiring) slot is recycled.
- Hard limit: no single notch may exceed −18 dB, regardless of parameter values.

## Stereo

Linked (default) analyses the mono sum and applies identical notches to both
channels, preserving image stability. Independent analysis is opt-in via the
Advanced panel.

In linked mode the two channels **share coefficients but not filter state**:
`AdaptiveNotchBank::Slot` holds one biquad per channel. Sharing a single biquad
state across channels is incorrect — each channel's output would be computed
from state containing the other channel's samples, producing a weak notch and
harmonic distortion on stereo material. A regression test asserts that channel
0 of a linked stereo run equals a mono run and that the notch reaches its full
depth.

## Latency

There is **no algorithmic lookahead**. The detector analyses the current and
previous samples only; it does not delay the audio path. The plugin reports
`setLatencySamples(0)`. FFT analysis introduces no latency because it runs on
the tap, not in a parallel processing path.

## Real-time safety

- No allocation, locks, file/network I/O or logging in steady-state
  `processBlock`. All buffers are sized in `prepareToPlay`.
- Bounded loops over channels, samples, bins and the fixed 12-slot bank.
- No exceptions cross the processing boundary.
- Filter state is sanitised and can never become non-finite.
- Default behaviour on invalid state is passthrough.
