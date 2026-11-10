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

[0.0.1-alpha]: https://github.com/MyEvent-Labs/de-feedback/releases/tag/v0.0.1-alpha
