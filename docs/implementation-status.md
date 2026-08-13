# Implementation status — 0.1.0

## Implemented and exercised

- Exactly eight bands; Bell, low/high shelf, low/high cut, band-pass, notch;
  continuous 0–48 dB/oct slope for every type.
- Graph drag, double-click creation into an unused slot, Q wheel, embedded and
  context numeric input, Shift multiselect, relative group drag, group bypass
  and delete, primary/secondary selection shapes, one Undo step per gesture.
- Per-band bypass, transient solo, Stereo/Left/Right/Mid/Side, Adaptive Q,
  dynamic Down/Up, threshold/range/ratio/attack/release, internal/external
  sidechain, transient SC audition, live GR and optional 5 ms lookahead.
- Zero-latency minimum phase, independently switchable de-cramping, Linear Phase
  1024/2048/4096 taps, exact host latency reporting, aligned ramped bypass and
  guarded live latency changes.
- Eight per-band drive algorithms including four adapted from
  `default_distortion`; Drive/Mix/Comp/bypass, DC blocking, 2x/4x/8x
  nonlinear-only oversampling.
- RTA input/output/both with 1024/2048/4096 FFT, range, floor, speed, averaging,
  tilt, peak hold, freeze, piano overlay, show/hide, and separate UI persistence.
- Match capture/analyse plus Amount/Smoothing/range and one-transaction
  conversion to eight ordinary editable bands. Analysis FFT is off audio thread.
- Regular parameter-derived and Smart 500 ms measured Auto Gain, full A/B state,
  Copy A→B/B→A, schema-v3 recall/migration, corrupt-state rejection, mono/stereo,
  unit parsing, and paper/ink family UI with Reduced Motion and unrestricted
  resize from 720 to 1200 px.

## Deliberate boundary

- Natural Phase is not exposed because the audited upstream prototype was not
  validated. Linear Phase is the optional non-minimum-phase mode.
- In Linear Phase, Stereo bands are in the FIR. Per-band L/R/M/S bands use their
  routed minimum-phase post stage because one shared stereo FIR cannot encode
  independent routing. Dynamic modulation and drive remain active post-FIR.
- Match EQ analysis data is intentionally not project state. Only the applied,
  editable-band result persists.
- There is no preset browser, preset file format, factory bank, or face-panel
  preset control, and the processor exposes zero factory programs to AU/VST3.
  Host project/state recall is fully supported.
