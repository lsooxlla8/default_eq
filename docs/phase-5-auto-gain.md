# Phase 5: Auto Gain

Completed on 2026-09-01. This phase replaces the approximate Regular Gain
coverage table and the RMS/peak Smart Gain detector. The public parameter and
state contracts remain unchanged.

## Regular Gain

- The compensation is derived from the combined complex response of all active
  bands at 192 logarithmically spaced frequencies from 20 Hz to the usable
  audio-band limit.
- Band order, filter type, frequency, Q, gain, slope, Amount, Shift, Adaptive Q
  and L/R or M/S placement are included in the transfer calculation. Cancelling
  filters therefore cancel before compensation is derived.
- T/S bands are evaluated as equal-power transient and sustain reference
  branches because their actual programme split is deliberately outside a
  programme-independent calculation.
- No input or output samples are inspected. A dirty flag recomputes the target
  only after a relevant parameter or sample-rate change; normal audio blocks
  reuse the cached value.

## Smart Gain

- The untouched input is delayed by the complete reported plug-in latency and
  compared with the post-EQ/drive signal before Auto Gain and manual Output.
- Both streams pass through the same BS.1770 K-weighting filters. The first
  estimate is taken after 400 ms and refined three times at 100 ms intervals.
  Smart Gain then locks and stops measuring until audible state changes.
- Silence still consumes the finite observation. If no usable LUFS estimate is
  available, the current compensation is retained and measurement stops.
- Audible parameter and latency changes reset both observations. The current
  compensation is held while the replacement observation is collected, so
  entering Smart or editing a band does not drift toward a stale target.
- Manual Output remains outside the comparison loop.

## Regression coverage

`dsp_integration` checks K-weighting calibration, combined-response cancellation,
stereo placement, Regular programme independence, 17 Smart Gain spectra and
filter extremes, finite-observation lock, manual Output isolation,
analysis-period gain hold and latency warm-up. The stationary Smart Gain budget
is 0.35 LUFS; the measured worst case after the finite observation is 0.09 LUFS.

The complete Phase 1 safety net remains the release gate:

```sh
ctest --test-dir build-release --output-on-failure
auval -v aufx Dfeq Icss
```
