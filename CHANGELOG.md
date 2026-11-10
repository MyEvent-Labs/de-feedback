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

### Fixed
- Detector false-negative churn on sustained tones: the per-bin temporal
  spectral floor crept up to a steady tone and released its notch after a few
  seconds, producing repeated notch engage/release ramps (audible grit).
  Replaced with a frequency-local shoulder floor so a sustained tone holds its
  notch for as long as it persists.
- Broadband noise peaks could fill the detector track table and crowd out a
  real feedback tone; track admission is now strength-gated and the weakest
  same-channel track is evicted.
- Notch re-tuning/re-levelling is now heavily smoothed, reducing time-varying
  modulation artefacts.
- Slot recycling now only reuses a silent or retiring notch, so recycling can
  no longer click.

[0.0.1-alpha]: https://github.com/MyEvent-Labs/de-feedback/releases/tag/v0.0.1-alpha
