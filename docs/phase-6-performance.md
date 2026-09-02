# Phase 6: Performance

Completed on 2026-09-01. The optimisations preserve the Phase 1 host, state,
automation, memory and DSP safety nets while reducing both fixed processor work
and the steady-state filter cost.

## Processor

- A transparent zero-band instance returns after input sanitisation when EQ,
  drive, dynamics, analysis, solo, latency, Auto Gain and manual Output require
  no work.
- Parameter listeners maintain a per-band dirty mask. Parameter snapshots,
  routing lists, filter setup, drive setup, dynamic/lookahead state and Regular
  Gain response are rebuilt only after a relevant change.
- Stable manual Output and Auto Gain use one vectorised scale/clamp pass instead
  of running smoothers and branches for every sample.
- Stable stereo filter cascades process L/R together with SIMD while retaining
  double-precision coefficients and state. Static L/R and M/S routing use
  preallocated block scratch storage; no audio-thread allocation was added.
- Mixed static/dynamic chains keep the original serial band order, but static
  sections still use the block kernel rather than falling back to the dynamic
  sample loop.
- Classic cut stages have a stereo block kernel. Tilt and variable-slope stage
  counts, coefficients and responses are unchanged.
- Detector input, playhead queries, global-bypass capture, meter publication
  and analyzer work are skipped when their consumers are inactive. Public
  dynamic-gain values continue to update without an open editor.
- Smart Gain invalidation is event-driven. Its finite K-weighted observation
  locks after the initial result and three refinements instead of measuring
  indefinitely.

## Editor and RTA

- The editor timer is the single RTA update source. The response graph repaints
  only for a new FFT frame or changed response/dynamic state.
- Spectrum averaging and peak decay advance when a new FFT frame arrives, not
  from `paint()`.
- The background, grid, labels and border are cached until size, theme or
  display range changes.
- Smart Gain, placement and threshold-meter controls skip repaints when their
  displayed state has not changed.
- The audio-thread spectrum snapshot now copies the circular capture buffer as
  two contiguous ranges. Its write cursor is producer-local; only completed
  slot publication remains atomic.

## Cost decomposition and benchmark

`DefaultEQ_PerformanceBenchmark` is a manual Release benchmark at 48 kHz stereo.
It prepares the source outside the measured processor call, subtracts an
independently measured buffer-restore median, warms each processor, and reports
the median of seven trials. It is not a CTest timing gate because power state
and host load affect absolute timings.

```sh
cmake --build build-release --target DefaultEQ_PerformanceBenchmark
./build-release/DefaultEQ_PerformanceBenchmark
```

Before the filter-kernel pass, an active instance had about 7–10 ns/sample of
fixed work. One static Bell added another 7–9 ns/sample. Eight Bells were almost
entirely filter work (about 70 of 77 ns/sample at a 512-sample block), and RTA
capture added about 4–6 ns/sample.

At a 512-sample block on the same machine and build:

| Scenario | Before | Final | Reduction |
| --- | ---: | ---: | ---: |
| Clean | 0.56 ns/sample | 0.31 ns/sample | 45% |
| Output only | 7.88 ns/sample | 0.63 ns/sample | 92% |
| 1 Bell | 15.13 ns/sample | 4.76 ns/sample | 69% |
| 8 Bells | 76.64 ns/sample | 33.40 ns/sample | 56% |
| 1 Bell, M/S | 15.18 ns/sample | 5.26 ns/sample | 65% |
| 8 Bells + RTA | 80.98 ns/sample | 35.31 ns/sample | 56% |

The small-block result improves more strongly: eight Bells at 32 samples fell
from 80.70 to 25.16 ns/sample, a 69% reduction.

The final diagnostic matrix is:

| Scenario | 32 | 64 | 128 | 512 |
| --- | ---: | ---: | ---: | ---: |
| Clean | 1.69 | 0.95 | 0.55 | 0.31 |
| Output only | 2.73 | 1.59 | 1.03 | 0.63 |
| 1 Bell | 6.60 | 5.49 | 5.07 | 4.76 |
| 8 Bells | 25.16 | 29.36 | 31.84 | 33.40 |
| 1 Bell, M/S | 7.19 | 6.01 | 5.51 | 5.26 |
| 8 Bells + RTA | 27.43 | 31.29 | 33.76 | 35.31 |
| 48 dB/oct classic cut | 14.50 | 15.78 | 17.24 | 18.72 |
| Tilt | 18.94 | 22.04 | 23.84 | 25.22 |
| 1 dynamic Bell | 19.58 | 18.20 | 18.39 | 16.97 |
| 8 Bells, one dynamic | 40.71 | 43.27 | 44.77 | 45.75 |
| 1 driven Bell, 1x | 19.14 | 18.18 | 19.11 | 18.92 |
| 1 driven Bell, 4x | 271.88 | 271.11 | 274.59 | 273.43 |

The 4x drive case is dominated by the existing FIR oversampling and nonlinear
kernel. Replacing that oversampler or changing its order would alter response,
latency or saturation behaviour, so it is deliberately outside this
DSP-equivalent pass. An attempted dynamic coefficient-update shortcut was also
discarded after the equivalence reference exposed a changed coefficient-ramp
trajectory.

## Regression evidence

- The pre-optimisation DSP reference contains 73,160 samples. Final comparison:
  maximum delta `1.71363354e-07`, RMS delta `1.65923738e-08`, versus budgets of
  `1e-5` and `1e-6` respectively.
- All 14 CTest gates pass, including host parameters, automation fuzz, DSP
  integration/equivalence, memory and editor layout.
- The final processor object is 1,104,328 bytes and prepared retained-instance
  RSS is 27,033,600 bytes, within the existing 1,130,000-byte and 40 MiB budgets.
- Both Release plugin binaries are universal arm64/x86_64 and pass strict code
  signature verification. Apple `auval` passes the AU.
