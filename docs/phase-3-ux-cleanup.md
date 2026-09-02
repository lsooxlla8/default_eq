# Phase 3: Interaction and UX cleanup

Completed on 2026-08-30. This milestone changes editor interaction only; it does
not change the host parameter contract, state schema, DSP formulas, or audio
processing order.

## Behaviour

- Right-clicking Q resets resonant Low Cut and resonant High Cut to `0.75`.
  Every other filter type retains the existing `1.0` Q reset. A mixed
  multi-selection resolves the reset independently for each band's type.
- Shift-right-clicking a graph band resets its dynamic Threshold to the neutral
  `0.0 dB` default. It no longer duplicates placement reset.
- Shift-Cmd-click remains the graph gesture for resetting placement to centre.
- Delete and Backspace delete every selected band as one undo transaction.
- Cmd/Ctrl-A is intentionally left unassigned: hosts such as REAPER intercept
  it, and the plug-in does not need a competing select-all shortcut.
- The node pointer hit radius is increased from 12 to 20 pixels while the
  visible node size remains unchanged.
- The final wheel mapping is Cmd/Ctrl-wheel for Slope and Alt-wheel for
  Character. Upward wheel motion increases Slope, including the plain-wheel
  path used by classic Low/High Cut.

## Regression coverage

`dsp_integration` now checks the resonant/non-resonant Q reset values, Threshold
reset isolation from placement, selected-band deletion through both keyboard
keys, and the 20-pixel hit boundary.

The Phase 2 deterministic render remains the audio-equivalence gate:

```sh
build-release/DefaultEQ_SafetyNet dsp-compare \
  build-release/phase2-dsp-baseline.bin
ctest --test-dir build-release --output-on-failure
auval -v aufx Dfeq Icss
```
