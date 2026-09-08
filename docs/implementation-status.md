# Development status — 0.5.3 (`default_eq`)

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
  creation with immediate dragging, right-drag marquee selection on empty graph space,
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
- Zero-latency minimum phase with ZL-derived filter cascades, Linear Phase
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
  drive path. Clean and dynamic EQ remain at the native rate; their ZL-derived
  filter design is independent of drive oversampling, so selecting oversampling
  without active drive adds neither work nor latency.
- Always-both RTA with selectable 4096/8192/16384 FFT size, coherent-gain
  normalisation, resolution-matched fractional-octave smoothing, an adjustable
  −24 to +24 dB ceiling (default 0 dB), adjustable floor, averaging time, and
  4.5 dB/oct default tilt. Averaging also controls all Spectral Statistics;
  Tonal Balance keeps its independent relative scale.
  Peak Hold is always active and clears on every host transport stop-to-play
  transition and editor reopen. Audio-side capture is dormant while the editor
  is hidden.
- Clicking the wordmark opens an in-editor panel exactly over the RTA. It holds
  Auto/White/Black theme, Auto/manual RTA gain range, the exposed analyzer settings,
  the hover-card toggle, a compact shortcut reference and a bottom diagnostic
  row for centroid, true output crest factor, spectral tilt, L/R correlation
  and ten-band tonal balance. The four numeric statistics include compact
  graphical scales. Shortcuts use an airy 4x2 summary plus a dedicated 3x2
  complete interaction map, both aligned to the cross-layer grid. White and Black
  each have independently persisted Background and Ink colours editable through
  an in-panel HSV picker. Auto is the factory theme and switches at local
  08:00/20:00. AUTO RTA gain range begins at ±6 dB, expands one step whenever
  a drag re-enters an edge
  zone, and only contracts when a new editor is opened or the settings panel is
  closed, based on the current bands' maximum gain.
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
  mono/stereo and unit parsing. The 0.5.0 paper/ink interface uses an embedded
  JetBrains Mono family, a 752x454 reference frame, fixed-aspect
  640x386–2400x1449 resizing, the retained production graph/RTA, and the
  always-open contextual workspace. Four draggable numeric cells expose
  Frequency, Gain, Q and Slope
  below the graph; zero, single and multi-selection states are explicit, with
  `MULTI` shown only for values that differ across the selection. Output stays
  below the graph; the header position remains a global semitone Shift that
  preserves the frequency ratios between all bands.

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
