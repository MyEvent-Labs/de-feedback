# Changelog

All notable changes to De-Feedback are documented here. This project adheres to
[Semantic Versioning](https://semver.org/).

## [0.0.1-alpha] — unreleased / first alpha

### Added
- First public engineering alpha of MyEvent Labs De-Feedback.
- Deterministic FFT spectral feedback detector with persistence and harmonicity
  checks.
- Adaptive notch bank: up to 12 peaking-biquad notches per channel, linked
  stereo by default, click-free add/remove, −18 dB hard per-notch limit.
- Conservative / Normal / Aggressive protection modes.
- Strength, Sensitivity, Max Cut, Recovery, Freeze and Clear controls.
- Input/output trims, input/output metering, real-time spectrum with active
  notch markers.
- Versioned state/preset save and recall that fails safe on malformed state.
- VST3 and Standalone on Windows; VST3, AU v2 and Standalone on macOS.
- Headless unit and DSP regression test harness.
- Offline signal-generator and benchmark tools.
- Native Windows and macOS CI workflows.

### Known limitations
See [`docs/ALPHA_LIMITATIONS.md`](docs/ALPHA_LIMITATIONS.md).

### Changed
- Detector harmonicity is now scored in a second pass over *all* frame peaks
  (previously it only saw peaks found so far, so a fundamental never saw its
  own harmonics) and matches any small-integer frequency ratio, not just
  integer multiples (so odd-harmonic subsets are still recognised as harmonic).
  A relative must also be within 15 dB of the peak to count. Measured on a
  clean musical passage under the Conservative profile: false-positive notches
  5 -> 0, with no loss of feedback-onset detection.
- Front-end metering: input/output **peak, RMS and peak-hold** meters, a
  dedicated **gain-reduction** meter, and a live **active-notch list** showing
  each notch's frequency and reduction depth.
- Added `tools/quality`, an objective scenario harness (detection latency and
  depth, harmonic distortion, false positives on clean music/speech, stereo
  identity, CPU) that runs as a CI gate via `--gate`. Coverage now includes a
  80 Hz - 15 kHz onset sweep, harmonic and multi-mode feedback, drifting
  feedback, independent stereo, release, transient rejection and sustained
  tones (28 checks).
- Detector detects high-frequency feedback (> 8 kHz) reliably: frame peaks are
  sorted strongest-first and capped to the strongest 16, and the track-admission
  floor no longer scales with a strict mode's threshold. 10 kHz onset 1.97 s ->
  0.07 s; 15 kHz 4.3 s -> 0.41 s (Normal), now also detected in Conservative.
- Peak frequencies are parabolically interpolated to sub-bin accuracy, so a
  notch lands on the tone (full notch depth).
- Growth gate (Conservative) now uses rise-since-creation rather than per-frame
  growth, which mis-handled slow, bin-centred tones.

### Fixed
- **Stereo harmonic distortion and a weak notch (root cause of the audible
  high-frequency grit).** In linked stereo the adaptive bank drove both
  channels through a single biquad, so each channel's output was computed from
  filter state containing the other channel's samples. On a stereo 1 kHz tone
  this produced a 2nd harmonic around -38 dBc and left the notch only ~1.4 dB
  deep instead of the requested 9 dB. Each channel now has its own filter state
  while sharing coefficients. Measured on the stereo 1 kHz tone: 2nd harmonic
  -162 dBc, notch depth -9.0 dB. Mono was unaffected, which is why earlier
  mono-heavy tests missed it.
- Detector churn on sustained tones: the per-bin temporal spectral floor crept
  up to a steady tone and released its notch after a few seconds, re-detecting
  and re-notching it. Replaced with a frequency-local shoulder floor so a
  sustained tone holds its notch for as long as it persists (measured notch-on
  for a steady tone: 39% -> 98%).
- Broadband noise peaks could fill the detector track table and crowd out a
  real feedback tone; track admission is now strength-gated and the weakest
  same-channel track is evicted.
- Notch re-tuning/re-levelling is now heavily smoothed, reducing time-varying
  modulation artefacts.
- Slot recycling now only reuses a silent or retiring notch, so recycling can
  no longer click.

[0.0.1-alpha]: https://github.com/MyEvent-Labs/de-feedback/releases/tag/v0.0.1-alpha
