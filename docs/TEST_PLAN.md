# Test Plan

## Principle

Everything that matters is reproducible headlessly in CI with no audio
interface. Interactive host tests are documented, not assumed.

## 1. Unit tests (`defeedback_tests`)

| Area | Cases |
|------|-------|
| Biquad | identity transparency; peak depth at centre within 1.5 dB across 44.1/48/88.2/96 kHz and 120/1k/5k Hz; shallowness away from centre; higher Q is narrower; extreme/invalid parameters stay finite and bounded |
| Detector | sustained narrow tone detected within 40 Hz and confidence > 0.3; silence yields no peaks; NaN/denormal input stays finite; persistence delays activation; config changes are safe |
| Notch bank | allocation from peaks; cap never exceeded under 30 peaks; −18 dB hard cut enforced; output finite and bounded under sustained drive; `clearAll` fully releases; mono safe |
| StateIO | XML round-trip preserves parameters and version; malformed XML rejected; wrong root rejected; newer schema rejected; missing version migrated forward |
| DSP regression | feedback onset detected and suppressed within a bound at 44.1/48 kHz; sample-rate × block-size matrix (44.1/48/88.2/96 kHz × 32…1024) stays bounded; mono and stereo bounded; clean multi-tone material does not over-trigger (≤ 4 notches under a conservative profile) |

## 2. Plugin validation

- **pluginval** at strictness level 8, in-process, against the built VST3.
- **Steinberg `validator`** for VST3 where available.
- **`auval`** for the AU v2 component on macOS.
- **Standalone** launch and audio-routing smoke test.

## 3. Host smoke matrix (documented, hardware-dependent)

Windows: REAPER, a VST3 host / pluginval.
macOS: REAPER, Logic Pro or GarageBand (AU), pluginval.

Hosts not present in CI are listed as *not run* rather than assumed passing.

## 4. Soak test

A ≥ 2-hour offline/real-time-equivalent run with a repeating synthetic feedback
onset, repeated editor open/close (in host tests), repeated bypass/mode changes,
sample-rate/block-size re-initialisation and silence↔signal transitions, while
watching for unbounded memory growth.

## 5. Static analysis

- `clang-format --dry-run --Werror` against `.clang-format`.
- `clang-tidy` with `.clang-tidy` where the toolchain provides it.
- Compiler warnings at the JUCE recommended level.
- ASan/UBSan for the test binary via `-DDE_FEEDBACK_ENABLE_SANITIZERS=ON`.

## 6. Real-time safety review (manual, checklist)

Confirm for the audio path: no allocation, no locks, no reference-count churn,
no string construction, no logging, no exceptions, no file/network I/O, no
non-atomic parameter reads, no UI access, no unbounded loops. Also review
denormal handling, coefficient stability, buffer/channel assumptions and editor
lifetime.

## Pass/fail gates for `v0.0.1-alpha`

- Windows VST3 + Standalone build and both native CI jobs pass.
- macOS VST3 + AU + Standalone build.
- pluginval and basic validators pass.
- No allocation/lock/file/network on the steady-state audio path.
- Synthetic feedback demonstrates detection and controlled suppression.
- Clean fixtures do not cause clearly destructive default behaviour.
- Bypass / clear / mode changes click-free in tests.
- Mono/stereo and required rates/block sizes pass.
- State recall works; malformed state fails safe.
- Soak test passes.
- Code, security and real-time reviews complete; high-severity issues fixed.
