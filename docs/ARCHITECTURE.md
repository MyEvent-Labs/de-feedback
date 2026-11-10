# Architecture

De-Feedback is a JUCE 8 / C++20 audio plugin with a strict split between a
realtime DSP core and a message-thread UI.

## Repository layout

```
/CMakeLists.txt              top-level build
/cmake/DeFeedbackJuce.cmake  pinned JUCE fetch (with local-path override)
/src/
  PluginProcessor.*          AudioProcessor, APVTS, processBlock orchestration
  PluginEditor.*             editor layout and parameter attachments
  dsp/
    Biquad.*                 peaking biquad + TDF2 state
    DetectorConfig.h         detector tuning struct
    FeedbackDetector.*       FFT spectral detector
    AdaptiveNotchBank.*      adaptive notch controller + filters
    SpectrumAnalyzer.*       lock-free audio->UI spectrum bridge
    Parameters.h             stable parameter IDs
  ui/
    LookAndFeel.*            palette + custom painting
    SpectrumView.*           spectrum + notch marker display
    Meter.*                  input/output meter
  state/StateIO.*            versioned state serialisation + migration
/tests/                      headless unit / DSP regression harness
/tools/                      offline signal generator + benchmark
/docs/                       this documentation
/.github/workflows/          native Windows and macOS CI
```

## Targets

- `defeedback_core` — static library containing all DSP and state code, with no
  GUI or plugin-format dependency. Both the plugin and the headless tests link
  it, so tests exercise the exact production DSP.
- `DeFeedback` — the JUCE plugin target, producing VST3, AU (macOS) and
  Standalone.
- `defeedback_tests` — console test runner.
- `defeedback_signal_generator`, `defeedback_benchmark` — offline tools.

## Threading model

| Thread | Responsibility |
|--------|----------------|
| Audio  | `processBlock`: sanitise, trim, run detector, update the notch bank, filter, meter, publish. |
| Message | Editor layout, repaint, parameter attachments, preset files. |
| UI timer | SpectrumView / Meter pull from lock-free data and repaint. |

There are exactly two cross-thread paths, both lock-free:

1. **Audio → UI metering/notch display.** `UiSnapshot` is a set of `std::atomic`
   scalars/arrays. The audio thread stores; the UI loads. Values may tear across
   fields by one block, which is acceptable for a display and never blocks audio.
2. **Audio → UI spectrum.** `SpectrumAnalyzer` uses a `juce::AbstractFifo`
   single-producer/single-consumer ring of samples. The UI thread performs the
   FFT; if the FIFO overflows the oldest samples are dropped.

Command flow UI → audio (Clear notches) is a single `std::atomic<bool>` flag
consumed at the top of `processBlock`. No locks anywhere on the audio path.

## Lifetime and lifecycle

- All detector, bank, FFT, FIFO and filter buffers are allocated in
  `prepareToPlay`. Steady-state `processBlock` performs no allocation, no locks,
  no file/network I/O and no logging.
- `releaseResources` clears DSP state.
- Parameter changes are read as atomics at block rate; no non-atomic parameter
  access occurs on the audio thread.
- The editor owns its `LookAndFeel` and clears it in the destructor so there is
  no use-after-free when the editor closes.

## State

`AudioProcessorValueTreeState` owns parameter state. `StateIO` adds an explicit
`stateVersion` and fails safe: malformed XML, a wrong root type, or a schema
newer than the running build causes the state to be ignored and defaults kept.
Older schemas are migrated forward.

## Extension point: future classifier

`FeedbackDetector` is deliberately deterministic. A future learned classifier
would implement the same "give me peaks with confidence" contract; the notch
bank and processor would not change. v0.0.1 does **not** ship such a model and
makes no AI claim.
