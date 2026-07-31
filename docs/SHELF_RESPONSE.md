# Shelf filter response — behaviour by release
_Measured 2026-07-31. Applies to **FreeEQ8 only**; see "Scope" below._
This document records the measured magnitude response of the Low Shelf and High
Shelf filter types across releases, the defect found in all releases up to and
including v2.3.0, and the candidate corrections with their measurements.
## Scope
Shelf coefficients are computed in `Source/DSP/Biquad.h`. That file is used by
**FreeEQ8 only**:
```cpp
// Source/DSP/EQBand.h
#if PROEQ8
    std::array<SvfBiquad, maxStages> svfFilters;   // ProEQ8 — Cytomic SVF
#else
    std::array<Biquad, maxStages> biquads;         // FreeEQ8 — RBJ biquad
#endif
```
**ProEQ8 is unaffected.** `SvfBiquad.h` does not use the shelf-slope formula and
already clamps `freqHz` to `sampleRate * 0.499` and `Q` to `[0.01, 100]`.
The dynamic-EQ sidechain (`Biquad scBiquad`) is present in both builds but is
hardcoded to `Bandpass, Q = 2.0, gain = 0.0`, which does not reach the shelf path.
## What "overshoot" means here
A shelf should be monotonic: flat at one end, a smooth transition, flat at the
other. On a +12 dB low shelf the response should read 0 dB at high frequencies
and +12 dB at DC, and should not exceed +12 dB anywhere.
Overshoot is the amount by which the response rises **past the set gain**. It is
a resonant peak at the corner frequency, caused by the second-order section
becoming underdamped. It also lengthens the impulse response, so it rings in time
as well as peaking in frequency.
Overshoot matters because it breaks the gain control's contract. A +12 dB shelf
with 7 dB of overshoot produces a ~19 dB peak, which is both unexpected and a
headroom hazard.
## Defect in all releases up to v2.3.0
RBJ's Audio EQ Cookbook defines the shelving alpha as:
```
alpha_S = sin(w0)/2 * sqrt( (A + 1/A)(1/S - 1) + 2 )
```
where `S` is a shelf-slope parameter valid only for **0 < S <= 1**. `S = 1` is the
steepest shelf that remains monotonic.
Every release from v0.3.0 through v2.3.0 contains:
```cpp
const double S = std::clamp(Q / 2.0, 0.1, 4.0);
```
`S` is permitted up to 4, outside the formula's domain. For `S > 1` the term
`(1/S - 1)` is negative, and once `(A + 1/A)(1/S - 1) < -2` the radicand goes
negative and `std::sqrt` returns NaN. That NaN propagates into `b0`, `b2`, `a0`
and `a2`, poisons the filter state, and reaches the output buffer permanently.
Failure threshold, for `S = 4` (any `Q >= 8`): NaN when `A + 1/A > 8/3`, i.e.
`|gain| > 13.78 dB`. Lower Q values fail at higher gains; the first failure in a
full sweep occurs at **Q = 3.80**.
Measured over 34,774,500 combinations (6 filter types x freq 20 Hz–20 kHz x
Q 0.1–24 x gain ±24 dB x 5 sample rates):
- **3,914,000 combinations produced non-finite coefficients — 11.3% of the space.**
This is reachable through normal UI interaction: shift-drag sets Q, drag sets
gain, and both ranges are exposed in full. It affects real-time playback, not
only offline export.
### Affected releases
All shipped releases contain the identical clamp:
`v0.3.0`, `v0.4.0`, `v0.5.0`, `v1.0.0`, `v1.1.0`, `v2.0.0`, `v2.0.1`, `v2.1.0`,
`v2.2.0`, `v2.3.0`.
## Table 1 — candidate corrections, worst-case overshoot
Low Shelf, +12 dB, corner 1 kHz, 48 kHz sample rate. Lower is better; 0.00 means
the response never exceeds the set gain.
```
    Q       OLD           OPTION A        CURRENT
        (<= v2.3.0)    (Q-form, cut)   (S clamped to 1)
-------------------------------------------------------
  0.71     0.00 dB        0.00 dB         0.00 dB
  1.00     0.00 dB        0.78 dB         0.00 dB
  2.00     0.00 dB        4.58 dB         0.00 dB
  4.00     1.38 dB        9.83 dB         0.00 dB
  8.00     7.01 dB       15.62 dB         0.00 dB
 24.00     7.01 dB       24.88 dB         0.00 dB
```
**OLD** (`S = clamp(Q/2, 0.1, 4.0)`) — clean through Q = 2, then unintended
peaking, then NaN. The 7.01 dB figure is identical at Q = 8, 16 and 24 because
`S` saturates at 4, so the control was already inert above Q = 8.
**OPTION A** (`alpha = sin(w0)/(2Q)`, RBJ's Q parameterisation) — **rejected.**
Eliminates NaN, but the response is no longer bounded by the set gain: at Q = 24
a +12 dB shelf peaks near +37 dB. Worse than OLD at every Q above 0.71.
**CURRENT** (`S = clamp(Q/2, 0.1, 1.0)`) — S constrained to RBJ's domain. NaN is
unreachable by construction, since `1/S - 1 >= 0` makes the radicand always
`>= 2`. Monotonic at every Q. Verified bit-identical to OLD for `Q <= 2`
(maximum coefficient difference exactly 0), so presets in that range are
unchanged.
## Table 2 — slope mapping, control range
`@2*f0` is the magnitude one octave above the corner. A varying value means the
Q control is still doing something; a frozen value means it is inert.
```
    Q |      S   oversh    @2*f0 |      S   oversh    @2*f0
      |   CURRENT (saturating)   |    LOG MAP (full range)
--------------------------------------------------------------
 0.10 |  0.100    0.00    +5.11  |  0.050    0.00    +5.53
 0.71 |  0.353    0.00    +3.45  |  0.389    0.00    +3.26
 2.00 |  1.000    0.00    +0.88  |  0.569    0.00    +2.41
 4.00 |  1.000    0.00    +0.88  |  0.689    0.00    +1.93
 8.00 |  1.000    0.00    +0.88  |  0.810    0.00    +1.49
24.00 |  1.000    0.00    +0.88  |  1.000    0.00    +0.88
```
**CURRENT** — `S` reaches 1.0 at Q = 2 and saturates. `@2*f0` freezes at +0.88 dB,
so Q values from 2 to 24 all produce the same shelf. Preserves `Q <= 2` exactly.
**LOG MAP** — `S = 0.05 + 0.95 * (ln Q - ln 0.1) / (ln 24 - ln 0.1)`, spreading the
full Q range across the valid `S` domain. Also 0.00 overshoot everywhere and also
NaN-free, but the control stays live end to end. Logarithmic rather than linear
because Q is perceptually a log parameter.
Cost: shelf presets shift. Small at typical settings (Q = 0.71 moves 3.45 to
3.26 dB at one octave up, under 0.2 dB) and larger at Q = 2 (0.88 to 2.41 dB).
Both mappings converge at Q = 24.
## Status
- **v0.3.0 – v2.3.0** — OLD behaviour. NaN reachable. Not corrected.
- **Unreleased (targeting v2.3.1)** — CURRENT behaviour staged: bounded,
  monotonic, NaN-free, `Q <= 2` unchanged.
- **LOG MAP** — measured and available, not applied. Decision pending, since it
  trades small preset drift for a working control across the full Q range.
## Verification
Three standalone harnesses, dependent only on `<cmath>`/`<algorithm>`:
- exhaustive sweep for non-finite coefficients and non-finite output
- magnitude-response measurement for asymptotic gain and worst-case overshoot
- old-versus-new coefficient comparison, to prove no change where OLD was valid
All three build against `Source/DSP/Biquad.h` directly with no JUCE dependency
and are suitable for the `FREEEQ8_BUILD_TESTS` target so CI catches regressions.
## Reference
Robert Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter
coefficients" — defines the shelving alpha and the `0 < S <= 1` domain.
