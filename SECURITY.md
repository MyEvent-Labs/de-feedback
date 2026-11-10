# Security Policy

## Supported versions

De-Feedback is pre-1.0. Only the latest `v0.0.1-alpha` tag receives fixes.

## Reporting a vulnerability

Please do not open a public issue for security problems. Report privately via
GitHub Security Advisories on the repository, or contact the maintainers
directly. Include:

- affected version/tag;
- platform and host;
- a reproduction (project file, signal, or steps);
- impact assessment.

We aim to acknowledge within a few days. As this is a free alpha, we cannot
offer a paid bug bounty.

## Scope and design guarantees relevant to security

- The plugin makes **no network calls**, performs **no telemetry**, and has **no
  licence or DRM** logic.
- The audio thread performs no file or network I/O.
- Preset/state loading fails safe on malformed input and never executes data.
- The build does not embed secrets. CI workflows use only the automatic
  `GITHUB_TOKEN`.

## Dependency policy

- Dependencies are pinned (JUCE to an immutable release tag).
- New dependencies must be justified, licence-compatible with GPLv3, and
  reviewed before merging.
- The pre-push checklist includes a dependency/secret scan.
