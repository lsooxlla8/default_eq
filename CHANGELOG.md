# Changelog

## 0.4.0

- Added host-parameter, automation-fuzz, memory, DSP-equivalence, and editor-layout
  regression gates.
- Split the processor, editor, response-curve, and band implementations by
  responsibility without changing the published parameter contract.
- Added Delete/Backspace band deletion, Threshold reset, larger graph hit areas,
  and revised wheel modifiers for Slope and Character.
- Reworked project-owned context-menu placement, editor resizing, persisted
  window size, HiDPI rendering, and arbitrary-aspect-ratio layout.
- Replaced Regular Auto Gain with an exact parameter-derived combined-response
  estimate and changed Smart Gain to a finite, latency-aligned LUFS observation.
- Added per-band dirty state, clean-instance fast paths, SIMD/block filter
  processing, and event-driven editor/RTA updates while preserving the intended
  DSP response.
- Archived obsolete pre-0.3 visual and verification records and documented the
  current implementation, performance baselines, and validation gates.
- Added the interactive reference prototype and design-system specification for
  the planned 0.5.0 interface replacement.

## 0.3.0

- Added transient/sustain routing, placement-aware dynamics, and external-sidechain handling.
- Added resonant cuts, Flat Tilt, global frequency Shift, and multi-band selection/editing.
- Reworked dynamic controls, filter interaction, Auto Gain, Smart Gain, and the compact interface.
- Removed Match EQ while retaining the analyzer with internal settings.
- Renamed the plug-in and release packages to `default_eq`.

## 0.2.0

- Added the eight-band compact interface, Tilt, per-band drive, and Linear Phase modes.

## 0.1.0

- Initial public release.
