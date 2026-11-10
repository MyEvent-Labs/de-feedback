# Third-Party Notices

De-Feedback v0.0.1-alpha bundles and depends on the following third-party
software.

## JUCE

- Project: JUCE (https://github.com/juce-framework/JUCE)
- Version: 8.0.17-security (commit `e548e97a54a083acbfc330d584419fd074409045`)
- Licence: dual-licensed under the **AGPLv3 / GPLv3** and a **commercial licence**.

De-Feedback v0.0.1-alpha is distributed as free software under **GPLv3**.
That choice is what makes using JUCE (and the VST3 SDK it embeds) lawful here
without a paid JUCE commercial licence.

### Consequence for the project

The GPLv3 obligation is viral. If this repository is ever relicensed to a
proprietary licence, or distributed as a closed-source product, the maintainers
must purchase a JUCE commercial licence **before** that distribution. This is
recorded as a release blocker in `docs/ALPHA_LIMITATIONS.md`.

## VST3 SDK

- Project: Steinberg VST3 SDK (vendored inside JUCE)
- Licence: dual-licensed (GPLv3 / Steinberg proprietary). Under the GPLv3
  JUCE path the GPLv3 option applies.

## Audio Unit (AU) SDK

- macOS AUv2 support is provided through the Apple AU SDK headers used by JUCE.
  Use is subject to Apple's SDK terms.

## No third-party DSP libraries

All DSP in this repository is original. No third-party DSP, ML or spectral
libraries are linked.

## Test material

All test audio is synthesised at runtime by `tools/signal-generator`. No
third-party recordings are redistributed.
