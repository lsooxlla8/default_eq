# Implementation status — 0.1.0

## Implemented and exercised

- Exactly eight bands; Bell, low/high shelf, low/high cut, band-pass, notch;
  continuous 3–48 dB/oct slope for every type.
- Graph drag, position-aware first-click creation into an unused slot with an
  immediate drag gesture, Q wheel (slope wheel on low/high cuts), vertical Q
  drag on low/high cuts, Cmd-wheel slope, Cmd-click bypass,
  Cmd-drag drive, Cmd-right-click slope reset, Shift-click placement reset,
  Shift-drag threshold, Shift-wheel L/R or M/S placement, momentary Alt-click solo,
  embedded/context numeric input, relative group drag,
  group bypass/delete, and one Undo step per gesture.
- Per-band bypass, band-pass solo audition, continuous centered L–R placement
  switchable to continuous M–S placement, global Adaptive Q,
  dynamic Down/Up, threshold/range/ratio/attack/release, internal/external
  sidechain, transient SC audition, live GR and continuous 0–5 ms lookahead.
  Dynamic DSP and its lookahead latency are inactive at the 0 dB threshold
  default and activate at the first -0.1 dB step.
- Bypassed bands remain visible and selectable as outlined graph nodes; only
  explicit delete releases their numbered slot for clean recreation.
- Zero-latency minimum phase, independently switchable de-cramping, Linear Phase
  1024/2048/4096 taps, exact host latency reporting, aligned ramped bypass and
  guarded live latency changes.
- The first ten displayed `default_distortion` drive algorithms, spectrally
  isolated per band; Drive/algorithm-specific Character, compatible stored
  Secondary semantics without a dedicated face-panel control, Mix/Comp/bypass, deterministic per-band table
  Auto Gain and DC blocking. Drive and its table Auto Gain are enabled by
  default at a neutral 0 dB.
- Global stepped Off/2x/4x/8x oversampling quality for the nonlinear per-band
  drive path. Clean and dynamic EQ remain at the native rate; de-cramping owns
  the near-Nyquist correction, so selecting oversampling without active drive
  adds neither work nor latency.
- Always-both RTA with overlapped 2048/4096/8192 FFT, coherent-gain
  normalisation, fractional-octave smoothing, range/floor/speed/average/tilt,
  peak hold/freeze/show-hide and separate UI persistence. Audio-side capture is
  dormant while the editor/spectrum is hidden or the spectrum is frozen.
- Match capture/analyse plus Amount/Smoothing/range and one-transaction
  conversion to eight ordinary editable bands. Analysis FFT is off audio thread.
- Regular parameter-derived and Smart 500 ms measured Auto Gain with progress,
  schema-v8 single-state recall and v3/v5/v6/v7 migration, no published
  A/B/link-group/dynamic-enable parameters, corrupt-state rejection,
  mono/stereo, unit parsing, and paper/ink family UI with hidden Reduced Motion preference and unrestricted
  resize from 720 to 1200 px. Every editor opens at the family-standard 860 px
  width with the compact graph-only view; the right-aligned Advanced button
  expands the unified Band/Dynamic/RTA/Match workspace without resizing the graph.

## Deliberate boundary

- Natural Phase is not exposed because the audited upstream prototype was not
  validated. Linear Phase is the optional non-minimum-phase mode.
- Low/high-cut Q is stored, automatable, and edited by vertical graph drag, but
  the 0.1.0 cut topology is deliberately monotonic and non-resonant; Q does not
  yet alter its audible response.
- In Linear Phase, centered L/R bands are in the FIR. Continuously placed L/R
  or M/S bands use their minimum-phase post stage because one shared stereo FIR
  cannot encode asymmetric placement. Dynamic modulation and drive remain active post-FIR.
- Match EQ analysis data is intentionally not project state. Only the applied,
  editable-band result persists.
- There is no preset browser, preset file format, factory bank, or face-panel
  preset control, and the processor exposes zero factory programs to AU/VST3.
  Host project/state recall is fully supported.
