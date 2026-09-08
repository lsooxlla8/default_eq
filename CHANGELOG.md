# Changelog

## 0.5.3

- Restored screen-level contextual menus so they are not clipped by the plug-in window.
- Positioned the main menu from the click and the saturation menu from its live hover side.
- Made the saturation menu compact and dismiss it when the pointer leaves its trigger and menu.
- Refreshed the Retina interface screenshot.

## 0.5.2

- Aligned the band parameter row.
- Added RTA Ceiling and shared averaging for all Spectral Statistics.
- Kept Tonal Balance independent of RTA Slope, Floor, and Ceiling.
- Enabled immediate dragging when creating Tilt and resonant Cut filters with Shift.

## 0.5.1

- Corrected the upstream attribution boundaries after auditing the current DSP:
  FreeEQ8 remains an original codebase with modified portions still present,
  while the unused Faust matched-filter implementation was removed.
- Simplified binary packages to one release README, one consolidated legal file,
  and a double-click install script for each platform.
- Replaced the README interface image with a sharp `1504 x 908` Retina render.

## 0.5.0

- Rebuilt the editor around the approved fixed-ratio `default_` interface with
  embedded JetBrains Mono typography and sharp HiDPI rendering.
- Added an RTA-sized settings and diagnostics panel with automatic light/dark
  themes, custom palettes, analyzer controls, spectral statistics, shortcuts,
  and automatic/manual RTA gain ranges from ±6 to ±36 dB.
- Preserved the production DSP, parameters, state, latency, automation, graph
  gestures, multi-selection, contextual band menu, and dynamic range editing.
- Expanded regression coverage for the new editor, analyzer settings, themes,
  range behaviour, statistics, and 4096/8192/16384-point FFT modes.

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
