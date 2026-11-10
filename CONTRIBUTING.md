# Contributing

Thanks for helping with De-Feedback. This is an early alpha; please keep the
project's honesty constraints intact.

## Ground rules

- **Never overstate the product.** No "AI", no guaranteed gain-before-feedback,
  no "zero latency" unless host-reported latency is exactly zero and measured
  processing is within the block. No competitor comparisons in user-facing copy.
- **Preserve the real-time contract.** The audio thread must never allocate,
  lock, do file/network I/O, log, or touch the UI. See `docs/ARCHITECTURE.md`.
- **Fail safe.** Invalid state, NaN/Inf input or an analyser failure must result
  in passthrough, never unexpected muting or gain.

## Workflow

1. Branch from `main`.
2. Build and test locally:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```
3. Run `pluginval` at strictness 8 against the built VST3.
4. Keep diffs focused; update docs and `CHANGELOG.md` with behaviour changes.
5. Formatting: run `clang-format` (config in `.clang-format`).

## Commit style

Use clear, present-tense messages. Reference issues where possible.

## Licence

By contributing you agree your contribution is licensed under GPLv3, matching
the project. Do not add dependencies that would change the licence obligations
without discussion first.
