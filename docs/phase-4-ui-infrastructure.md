# Phase 4: UI infrastructure and resizing

Completed on 2026-08-31. This phase replaces the patched popup-menu dependency,
finishes context-menu placement, and makes the editor independently resizable
without changing parameters, state schema, or DSP.

## Context menu

- CMake no longer patches JUCE during configure; the JUCE submodule remains
  clean before and after configuration.
- Project-owned geometry places the L/C/R/M/S/T/S row under the pointer whenever
  screen edges permit it and clamps the complete main menu inside the display.
- The main menu is a single 185-pixel-high column, so it does not need scrolling
  on supported desktop work areas.
- Saturation remains a hover submenu. It is a separate project-owned popup that
  explicitly chooses the right or left side from available screen space and
  clamps its vertical position to the display.

## Editor layout

- The editor resizes freely between 640x400 and 2400x1600 without a fixed aspect
  ratio. The default 800x464 metrics remain unchanged.
- UI scale is derived from the limiting width or height, from 0.74x to 1.5x.
  This keeps controls usable in wide and tall windows while the graph absorbs
  additional space.
- Width and height are persisted independently in the existing application
  preferences and restored the next time an editor opens.
- JUCE keeps drawing in logical coordinates; fonts and controls are recalculated
  at integer bounds and render through the host's native HiDPI scale.

## Regression coverage

`editor_layout_regression` locks child-bound snapshots for minimum, default,
maximum, wide, and tall layouts. Every layout checks that visible controls stay
inside the editor and do not overlap, that the response graph remains usable,
and that offscreen 1x and 2x screenshots contain rendered structure. It also
checks editor-size persistence and context-menu anchoring/flipping geometry.

```sh
ctest --test-dir build-release -R editor_layout_regression --output-on-failure
build-release/DefaultEQ_SafetyNet dsp-compare \
  build-release/phase2-dsp-baseline.bin
ctest --test-dir build-release --output-on-failure
auval -v aufx Dfeq Icss
```
