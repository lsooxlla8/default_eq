# Phase 2: Behaviour-preserving refactor

Completed on 2026-08-30 against the Phase 1 safety net. This phase changes file
ownership only: parameter contracts, state schema, DSP formulas, processing
order, editor layout, and interaction behaviour remain unchanged.

## DSP equivalence gate

`dsp_equivalence` renders four deterministic 48 kHz scenarios covering static
minimum-phase EQ, dynamics with external sidechain and drive, T/S routing, and
linear-phase processing. The captured baseline contains 73,160 floats. Its
regression limits are a maximum sample delta of `1e-5` and RMS delta of `1e-6`.
All four refactor checkpoints matched the pre-refactor baseline exactly:
maximum delta `0`, RMS delta `0`.

The baseline file is a local build artifact rather than a tracked fixture:

```sh
build-release/DefaultEQ_SafetyNet dsp-write \
  build-release/phase2-dsp-baseline.bin
build-release/DefaultEQ_SafetyNet dsp-compare \
  build-release/phase2-dsp-baseline.bin
```

## Responsibility split

- Processor lifecycle and audio processing remain in `PluginProcessor.cpp`.
  Parameter creation/caching, state serialization, lookahead routing, and
  linear-phase response construction now live in dedicated processor files.
- Editor construction, timer, painting, keyboard handling, and layout remain in
  `PluginEditor.cpp`. Reusable control implementations and band-control binding
  now live in two dedicated editor files.
- Response-curve state and response calculation remain in
  `ResponseCurveComponent.cpp`. Drawing and pointer/menu interaction now live in
  separate translation units.
- `EQBand.h` now holds the band type and state. Inline processing, drive, and
  private DSP implementations are separated into responsibility-specific
  `.inl` files, preserving the exact compiled hot path.

## Checkpoint results

| Refactor checkpoint | DSP baseline | CTest | Formats | Native validation |
|---|---:|---:|---|---|
| Processor | exact | 13/13 | AU + VST3 built | `auval` passed |
| Editor | exact | 13/13 | AU + VST3 built | `auval` passed |
| Response curve | exact | 13/13 | AU + VST3 built | `auval` passed |
| EQ band | exact | 13/13 | AU + VST3 built | `auval` passed |

The macOS native validation command was:

```sh
auval -v aufx Dfeq Icss
```

`pluginval` was not installed in the local environment; the repository's
cross-platform release workflow remains the strict VST3 `pluginval` gate.
