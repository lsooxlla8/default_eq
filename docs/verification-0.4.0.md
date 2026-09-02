# Verification record — 0.4.0 release candidate

Verified locally on macOS 26.5.2 arm64 on 2026-09-02 from the source prepared
for tag `v0.4.0`. The cross-platform results are supplied by the tagged GitHub
release workflow.

## Build and regression gates

- Apple clang 21.0.0 and CMake 4.3.3 Release build.
- CTest: 14/14 passed.
- Host regression locks all 173 published parameters.
- Automation fuzz, DSP integration/equivalence, memory and editor-layout gates
  passed.
- The wheel interaction contract is covered directly: Cmd/Ctrl-wheel selects
  Slope, Alt-wheel selects Character, and the platform's upward wheel delta maps
  to an increasing Slope step.

## DSP and performance

- The pre-performance-pass reference contains 73,160 float samples.
- Final comparison: maximum delta `1.71363354e-07`; RMS delta
  `1.65923738e-08`. The corresponding budgets are `1e-5` and `1e-6`.
- Processor object: 1,104,328 bytes against a 1,130,000-byte budget.
- Prepared retained-instance RSS: 27,033,600 bytes against a 40 MiB budget.
- The repeatable processor benchmark and its before/after matrix are recorded
  in [`phase-6-performance.md`](phase-6-performance.md).

## Plug-in formats

- AU and VST3 Release bundles built successfully.
- Both binaries contain arm64 and x86_64 slices.
- Both bundles pass strict ad-hoc `codesign --verify`.
- `auval -v aufx Dfeq Icss`: `AU VALIDATION SUCCEEDED`.
- A local strict pluginval run was not performed because pluginval is not
  installed. `.github/workflows/release.yml` provides the strictness-level-10
  VST3 validation gate on macOS, Linux, and Windows.

## Documentation and visual records

- README interaction text, build side effects, product name and current-status
  links were audited against the implementation.
- The current README screenshot is
  `docs/screenshots/ui-aligned-rail-tooltip-860.png`.
- Version 0.1 verification and screenshots are retained under
  `docs/archive/0.1.0`; discarded pre-0.3 layout and Match-workspace captures
  are retained under `docs/archive/pre-0.3-ui` and explicitly marked obsolete.
- The 0.5.0 interface replacement is specified separately in `todo.md`, the
  interactive prototype, and the family design-system document; it is not part
  of the 0.4.0 plug-in UI.

## Manual-check boundary

No DAW or editor GUI was opened for this record. Layout is covered by the
offscreen 1x/2x regression renderer, but physical wheel direction in a host and
the current screenshot were not manually recaptured during this pass.
