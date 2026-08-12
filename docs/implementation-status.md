# Implementation status — 0.1.0 development snapshot

## Working in the current build

- 24 simultaneously available bands; Bell, low/high shelf, high/low pass,
  band-pass, and notch.
- Direct node drag, Shift-drag Q, double-click band creation, context delete,
  per-band on/solo, channel routing, link groups, and numeric slider text boxes.
- Downward internal-sidechain dynamic EQ per band with threshold, ratio, attack,
  and release.
- Zero-latency minimum-phase path and a 2048-sample linear-phase path with
  background kernel rebuild and host latency reporting.
- Per-band drive with four upstream algorithms; 2x/4x/8x oversampling is skipped
  when no nonlinear drive is active.
- Adaptive Q formula `effectiveQ = clamp(baseQ * (1 + abs(gainDb) * 0.12),
  0.1, 24)` without overwriting the stored base Q.
- Pre/post analyzer selection, piano frequency overlay, Match EQ capture/apply,
  UndoManager-backed APVTS changes, two serialized A/B snapshots, Mid/Side, and
  click-free latency-aligned global bypass.
- Family paper/ink endpoints, exact inversion, monospace type, square controls
  and nodes, zero-radius controls, resize constraints, separate theme/window/
  reduced-motion preferences.

## Explicitly unfinished

- Smooth 0–48 dB/oct slope, de-cramping, Natural Phase validation, linear-phase
  quality choices and click-free live latency-mode transitions.
- Upward dynamic mode, range, lookahead, external per-band sidechain, sidechain
  filter controls/audition, and gain-reduction graphics.
- Per-band drive Mix/output compensation/DC blocker and imported
  `default_distortion` algorithm set.
- Full analyzer controls and simultaneous input/output rendering; Match EQ
  target capture, smoothing/range/amount UI, editable-band conversion, and one
  undoable apply transaction.
- Smart Auto Gain state machine; current Auto Gain is a smoothed RMS mode only.
- True per-band Stereo/Left/Right/Mid/Side independent of the global Stereo/M/S
  domain; multiselect/lasso/group operations; numeric unit parser.
- Click-free A/B crossfade; full action-level undo coalescing outside node drags;
  visual QA at all target widths; block-size determinism and reload tolerance
  measurements; drive alias/de-cramping measurements.

These items have no mock controls and must not be described as implemented.
