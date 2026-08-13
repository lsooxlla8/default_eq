# Implementation status — 0.1.0

## Implemented and exercised

- Exactly eight bands; Bell, low/high shelf, low/high cut, band-pass, notch;
  continuous 3–48 dB/oct slope for every type.
- Graph drag, double-click creation into an unused slot, Q wheel, embedded and
  context numeric input, Shift multiselect, relative group drag, group bypass
  and delete, primary/secondary selection shapes, one Undo step per gesture.
- Per-band bypass, band-pass solo audition, continuous centered L–R placement
  switchable to continuous M–S placement, Adaptive Q,
  dynamic Down/Up, threshold/range/ratio/attack/release, internal/external
  sidechain, transient SC audition, live GR and continuous 0–5 ms lookahead.
- Zero-latency minimum phase, independently switchable de-cramping, Linear Phase
  1024/2048/4096 taps, exact host latency reporting, aligned ramped bypass and
  guarded live latency changes.
- The first ten displayed `default_distortion` drive algorithms, spectrally
  isolated per band; Drive/algorithm-specific Character, original vertical
  Secondary where applicable, Mix/Comp/bypass, deterministic per-band table
  Auto Gain and DC blocking. Drive is enabled by default at a neutral 0 dB.
- Global 2x/4x/8x FIR oversampling covering dynamic EQ and nonlinear drive.
- Always-both RTA with overlapped 2048/4096/8192 FFT, coherent-gain
  normalisation, fractional-octave smoothing, range/floor/speed/average/tilt,
  peak hold/freeze/show-hide and separate UI persistence.
- Match capture/analyse plus Amount/Smoothing/range and one-transaction
  conversion to eight ordinary editable bands. Analysis FFT is off audio thread.
- Regular parameter-derived and Smart 500 ms measured Auto Gain with progress,
  schema-v6 single-state recall and v3/v5 migration, no published A/B/link-group
  parameters, corrupt-state rejection,
  mono/stereo, unit parsing, and paper/ink family UI with hidden Reduced Motion preference and unrestricted
  resize from 720 to 1200 px.

## Deliberate boundary

- Natural Phase is not exposed because the audited upstream prototype was not
  validated. Linear Phase is the optional non-minimum-phase mode.
- In Linear Phase, centered L/R bands are in the FIR. Continuously placed L/R
  or M/S bands use their minimum-phase post stage because one shared stereo FIR
  cannot encode asymmetric placement. Dynamic modulation and drive remain active post-FIR.
- Match EQ analysis data is intentionally not project state. Only the applied,
  editable-band result persists.
- There is no preset browser, preset file format, factory bank, or face-panel
  preset control, and the processor exposes zero factory programs to AU/VST3.
  Host project/state recall is fully supported.
