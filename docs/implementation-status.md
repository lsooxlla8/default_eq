# Development status — 0.2.0 (`default_eq8`)

## Implemented and exercised

- Exactly eight bands; Bell, low/high shelf, low/high cut, band-pass, notch and
  symmetric Tilt (`+6 dB` means `-6/+6 dB` around the pivot);
  continuous 3–48 dB/oct slope for every type.
- Graph drag, position-aware first-click creation into an unused slot with an
  immediate drag gesture, Q wheel (slope wheel on low/high cuts), vertical Q
  drag on low/high cuts, band-pass and notch, Cmd-wheel slope, Cmd-click bypass,
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
- Zero-latency minimum phase with always-on de-cramping, Linear Phase
  1024/2048/4096 taps, exact host latency reporting, aligned ramped bypass and
  guarded live latency changes.
- The first ten displayed `default_distortion` drive algorithms, spectrally
  isolated per band; Drive/algorithm-specific Character, compatible stored
  Secondary semantics without a dedicated face-panel control, deterministic
  always-on per-band table Auto Gain and DC blocking. Exactly `0.0 dB` fully
  disables nonlinear processing, oversampling work and its latency.
- Global stepped Off/2x/4x/8x oversampling quality for the nonlinear per-band
  drive path. Clean and dynamic EQ remain at the native rate; de-cramping owns
  the near-Nyquist correction, so selecting oversampling without active drive
  adds neither work nor latency.
- Always-both RTA fixed at High resolution, with coherent-gain normalisation,
  fractional-octave smoothing, a fixed 0 dB ceiling, adjustable floor,
  averaging time (65 ms default, 1.60 s maximum), and 4.5 dB/oct default tilt.
  Peak Hold is always active and clears on every host transport stop-to-play
  transition and editor reopen. Audio-side capture is dormant while the editor
  is hidden.
- Match capture/analyse plus Amount/Smoothing/range, a transient main/sidechain
  analysis-source switch, and one-transaction conversion to eight ordinary
  editable bands. Analysis FFT is off audio thread.
- Global EQ Amount is a header control beside Output: `-200..200%`, defaults to
  `100%`, and reaches exact dry unity at `0%`. Gain-bearing filters use direct
  signed dB scaling across the entire range, avoiding wet/dry phase reversal in
  both directions. Positive cut Amount moves the cutoff logarithmically from a
  neutral audio-band edge to the selected cutoff and beyond. Band-pass and
  notch intensity is bounded at its complete-filter state above `100%` instead
  of extrapolating through a phase reversal. Negative cut, band-pass and notch
  Amount stays at unity because an exact inverse is not a stable zero-latency
  IIR. Drive is inactive on the negative half.
- Regular parameter-derived and Smart 500 ms measured Auto Gain with progress,
  schema-v9 single-state recall and v3/v5/v6/v7 migration, no published
  A/B/link-group/dynamic-enable parameters, corrupt-state rejection,
  mono/stereo, unit parsing, and paper/ink family UI with hidden Reduced Motion
  preference and resize from a clipping-safe 960 px minimum to 1200 px. Every
  editor opens at 960 px
  width with an always-open workspace. One right-aligned button switches the
  combined Band/Dynamic page (including Drive) and the combined Match/RTA page; every
  newly opened editor starts on Band/Dynamic instead of persisting the last
  page. Four
  draggable numeric fields expose Freq, Gain, Q and Slope below the graph.

## Deliberate boundary

- Natural Phase is not exposed because the audited upstream prototype was not
  validated. Linear Phase is the optional non-minimum-phase mode.
- Low/high-cut Q is stored, automatable, edited by vertical graph drag, and
  controls an independent cutoff resonance with `Q = 1` as the neutral point.
  The 3–48 dB/oct control morphs between complete Butterworth orders, so every
  setting remains a true cut without a shelf floor or order-dependent
  compensation hump. Band-pass and notch use the same vertical-Q gesture.
  Bell uses distributed edge sections for a flatter top and steeper skirt;
  shelf and tilt select phase-coherent Linkwitz-Riley transition orders instead
  of reusing Q as a fake slope control.
- In Linear Phase, centered L/R bands are in the FIR. Continuously placed L/R
  or M/S bands use their minimum-phase post stage because one shared stereo FIR
  cannot encode asymmetric placement. Dynamic modulation and drive remain active post-FIR.
- Match EQ analysis data is intentionally not project state. Only the applied,
  editable-band result persists.
- There is no preset browser, preset file format, factory bank, or face-panel
  preset control, and the processor exposes zero factory programs to AU/VST3.
  Host project/state recall is fully supported.
