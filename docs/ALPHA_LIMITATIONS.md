# Alpha Limitations — v0.0.1

De-Feedback v0.0.1-alpha is a **testable engineering alpha**. It is not
concert-safe, production-proven, AI-powered, zero-latency-by-design, or
equivalent to any commercial product.

## Claims that are explicitly NOT made

- No guaranteed gain-before-feedback improvement.
- No dereverberation or denoising.
- No "AI" capability. The detector is classical and deterministic.
- No replacement for loudspeaker placement, gain structure, system tuning, EQ,
  delay alignment or a competent live-sound engineer.
- No claim of being equivalent to any competitor.

## Known limitations

1. **Detection is heuristic.** A sustained musical tone and a slow feedback
   buildup can look similar. Conservative mode trades detection speed for a much
   lower false-positive rate; Aggressive mode reacts faster and is more likely
   to cut legitimate tones.
2. **Tone vs feedback.** A single steady, non-growing tone can eventually be
   notched in Normal/Aggressive modes. This is inherent to energy-based
   detection without a trained classifier.
3. **No learned model.** A classifier interface is anticipated but not
   implemented. Do not describe this build as AI.
4. **Stereo linked by default.** Independent mode can move the stereo image;
   use it deliberately.
5. **Frequency range** is limited to roughly 80 Hz – 18 kHz and to below
   Nyquist.
6. **CPU.** Measured DSP cost is within the range reported in the final release
   notes; it is not a zero-cost plugin.
7. **macOS signing/notarisation.** The macOS alpha is unsigned unless signing
   credentials were available at build time. Gatekeeper instructions are in the
   README; do not disable system security globally.
8. **Validation coverage.** Host testing is limited to the hosts actually
   present on the build machines. Hosts not run are documented as not run.

## Licensing blocker

If this repository is ever distributed under a proprietary licence, a **JUCE
commercial licence must be purchased first**, because the current GPLv3 grant is
what makes the JUCE dependency lawful. This is a release blocker, not a
limitation to be worked around.

## Before live use

Test under controlled conditions on your own system and material. Keep a bypass
available. Do not rely on this alpha as the only protection against feedback.
