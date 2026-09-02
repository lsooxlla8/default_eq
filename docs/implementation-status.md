# Development status — 0.4.0 (`default_eq`)

## Implemented and exercised

- Exactly eight bands and ten filter types. Resonant Low/High Cut and shaped
  filters use ZLEqualizer cascades with discrete 6/12/24/36/48/72/96 dB/oct
  slopes (12 minimum for Bell/Notch/Band Pass); Tilt uses ZL Flat Tilt.
  Classic resonance-free Low/High Cut retain the v0.1.0 continuous
  first-order-stage slope, now extending through 96 dB/oct.
- Graph drag, position-aware first-click creation into an unused slot with an
  immediate drag gesture, Q wheel (slope wheel only on classic cuts), vertical Q
  drag restricted to cut filters, Cmd/Ctrl-wheel Slope, Alt-wheel Character,
  Cmd-click bypass, Cmd-drag drive, Cmd-right-click Drive/Character reset,
  Alt-right-click slope reset, Shift-click selection toggle on nodes,
  Shift-Cmd-click placement reset, Shift-click Flat Tilt/resonant edge-cut
  creation or marquee selection on empty graph space,
  Shift-drag threshold, Shift-wheel L/R, M/S or T/S placement, momentary Alt-click solo,
  embedded/context numeric input, relative group drag,
  group bypass/delete, and one Undo step per gesture.
- Per-band bypass, band-pass solo audition, continuous centered L–R placement
  switchable to continuous M–S or transient–sustain placement, global Adaptive Q,
  dynamic Down/Up, a placement-aware stereo threshold/input-detector meter with editable
  fixed threshold, range/ratio/linked Speed, internal/external sidechain, live GR
  and continuous per-band 0–5 ms lookahead. Audio and reported latency follow
  the maximum active lookahead while every detector retains its own setting.
  Dynamic DSP and its lookahead latency are inactive at the 0 dB threshold
  default and activate at the first -0.1 dB step.
  Classic-cut dynamics modulate slope; resonant-cut dynamics modulate Q. The selected
  dynamic band exposes its full range as a monochrome fill and draggable square in the RTA.
- T/S uses one ZLSplitter-derived 75%-overlap FFT audio engine with aligned
  complementary outputs and matching warm internal/external detector splitters.
  Transient and sustain drive branches are oversampled independently before
  recombination. Strength defaults to 100%; Balance 0, Hold 50 and
  Smooth 50 retain upstream defaults. All four are host parameters and have no
  face-panel controls.
- Bypassed bands remain visible and selectable as outlined graph nodes; only
  explicit delete releases their numbered slot for clean recreation.
- Zero-latency minimum phase with always-on de-cramping, Linear Phase
  1024/2048/4096 taps, exact host latency reporting, aligned ramped bypass and
  guarded live latency changes.
- The retained eight `default_distortion` drive algorithms, spectrally
  isolated per band; Drive/algorithm-specific Character, compatible stored
  Secondary semantics without a dedicated face-panel control, deterministic
  always-on per-band table Auto Gain and DC blocking. Exactly `0.0 dB` fully
  disables nonlinear processing, oversampling work and its latency.
  Phase Distortion uses the same input-envelope-modulated delay, linear Depth,
  Tone mapping and tail release core as `default_distortion`.
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
- Global EQ Amount is a header control beside Shift: `-200..200%`, defaults to
  `100%`, and reaches exact dry unity at `0%`. Gain-bearing filters use direct
  signed dB scaling across the entire range, avoiding wet/dry phase reversal in
  both directions. Positive cut Amount moves the cutoff logarithmically from a
  neutral audio-band edge to the selected cutoff and beyond. Band-pass and
  notch intensity is bounded at its complete-filter state above `100%` instead
  of extrapolating through a phase reversal. Negative cut, band-pass and notch
  Amount stays at unity because an exact inverse is not a stable zero-latency
  IIR. Drive is inactive on the negative half.
- Regular Auto Gain derives one compensation value from the exact combined
  complex EQ response, including Amount, Shift, Adaptive Q and stereo routing.
  It is independent of programme audio and recomputes only after a relevant
  parameter change. Smart Gain compares latency-aligned input and pre-Output
  post-EQ loudness through BS.1770 K-weighting. It takes its first estimate at
  400 ms, refines it three times at 100 ms intervals, then locks and stops
  measuring. Audible parameter or latency changes start one new finite
  observation without forcing compensation to jump during analysis.
- Phase 6 performance work adds a transparent zero-band fast path, per-band
  dirty snapshots, a vectorised stable-output pass and stereo block kernels for
  steady-state cascades and classic cuts. Unchanged bands avoid parameter,
  routing and coefficient setup, and static sections stay on the block kernel
  inside mixed dynamic chains. The response graph now updates from parameter
  changes or new FFT frames, advances spectrum smoothing outside `paint()`,
  caches its static background, and skips unchanged control repaints.
  Schema-v9 single-state recall and v3/v5/v6/v7 migration, no published
  A/B/link-group/dynamic-enable parameters, corrupt-state rejection,
  mono/stereo, unit parsing, and paper/ink family UI with hidden Reduced Motion
  preference and resize from a clipping-safe 800 px minimum to 1200 px. Every
  editor opens at 800 px width with the Band/Dynamic workspace always open.
  Four draggable numeric fields expose Freq, Gain, Q and Slope below the graph.
  Output sits below the graph; the former header Output position is a global
  semitone Shift that preserves the frequency ratios between all bands.

## Deliberate boundary

- Natural Phase is not exposed because the audited upstream prototype was not
  validated. Linear Phase is the optional non-minimum-phase mode.
- Classic Low/High Cut ignore Q and use plain wheel for their continuous slope.
  Resonant cuts use ZLEqualizer Q and discrete cascades; their plain wheel edits
  Q and Cmd/Ctrl-wheel edits slope, matching Bell, shelves, Tilt, Band Pass and
  Notch. Upward wheel motion increases Slope in both the modified and classic-cut
  plain-wheel paths.
- In Linear Phase, centered L/R bands are in the FIR. Continuously placed L/R
  or M/S bands use their minimum-phase post stage because one shared stereo FIR
  cannot encode asymmetric placement. Dynamic modulation and drive remain active post-FIR.
- There is no preset browser, preset file format, factory bank, or face-panel
  preset control, and the processor exposes zero factory programs to AU/VST3.
  Host project/state recall is fully supported.
