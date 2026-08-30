# Architecture and source decision

Decision fixed before transfer: FreeEQ8 is the primary architectural and code
base; current `default_distortion` supplies the family design system and
author-owned reusable DSP/UI infrastructure; ZLEqualizer supplies the analyzer
smoothing/normalisation reference. The combined derivative is AGPL-3.0-only.

| Subsystem | Source | Exact revision | License | Reason | Changes in default_eq | Risk |
|---|---|---|---|---|---|---|
| JUCE processor, APVTS, filter/parameter topology | FreeEQ8 | `11376c1c496569c975e4d195c3fe5fd44b53d415` | GPL-3.0 | Closest complete vertical slice | Exactly 8 bands, sidechain bus, mono/stereo safety, versioned single audio state, no DRM/updater/presets/A-B | Medium |
| Minimum-phase EQ and response model | ZLEqualizer + default_eq v0.1.0 | ZLEqualizer `02c517e35f0ef8460c15815f303051dffdb0895a` | AGPL-3.0 | Matched coefficients, exact discrete high-order cascades and Flat Tilt, plus the preferred original cut behavior | Resonant cut and shaped filters use ZL cascades; Tilt uses ZL's pivot-normalised Flat Tilt bank; classic Low/High Cut use the v0.1.0 resonance-free continuously variable first-order-stage path | High near Nyquist |
| Transient/sustain routing | ZLSplitter | `2f50824ab925eeff7950986eac640dab43c3ce67` | AGPL-3.0 | Auditable median-mask separation with complementary outputs | One stereo 75%-overlap FFT audio splitter plus warm internal/external detector splitters, 5x5 median mask, two-hop hold delay, host-exposed hidden controls, latency-aligned T/S branches and per-band placement | High |
| De-cramping | Faust `vaeffects.lib`, Vicanek matched-filter implementation | `ccc6030e60806011ae73c9502d9bca85ff2b79fa` | MIT | Published matched second-order fit with inspectable coefficients | C++ port, finite guards, smooth RBJ-to-matched blend limited to the upper spectrum, independent of oversampling | Medium |
| Dynamic EQ | FreeEQ8 + ZLEqualizer + new work | FreeEQ8 revision above; ZLEqualizer `02c517e35f0ef8460c15815f303051dffdb0895a` | GPL-3.0 / AGPL-3.0-only | Existing envelope/filter integration plus ZL range interaction | Down/up, placement-aware L/R, M/S and T/S peak detection, calibrated stereo threshold meter, editable fixed threshold, range fill with draggable square, ratio, linked Speed timing, Q-matched detector filters, external SC, individual continuous 0–5 ms lookahead under one maximum host latency, and live GR; classic-cut slope, resonant-cut Q and inverse Band Pass Q modulation | High |
| Linear phase | FreeEQ8 + JUCE partitioned convolution + new work | FreeEQ8/JUCE revisions above | GPL-3.0 / JUCE upstream terms / AGPL-3.0-only | Preserve the validated magnitude/FIR model without paying for a monolithic FFT per host block | Background symmetric-FIR construction; wait-free IR ownership transfer; zero-additional-latency uniform partitions; 1024/2048/4096 taps and 512/1024/2048 samples latency; routed-band fallback and transition guard | High |
| Per-band drive | FreeEQ8 plus the author-owned default_distortion project | FreeEQ8 revision above; default_distortion `d145fab57e943869939d6a987cc69f90d676a4ae` | GPL-3.0 / AGPL-3.0-only | Carry the family drive modes and controls while keeping reused code boundaries explicit | Eight displayed algorithms and algorithm-specific Character; Phase Distortion shares default_distortion's input-envelope delay core and linear depth law; Soft Clip uses `std::tanh` and an author-owned bounded cubic morph; Tape is an independently written feedback-memory shaper; compatible stored Secondary semantics, EQ-specific deterministic Auto Gain tables, spectral band extraction, DC blocking, and a true zero-Drive CPU/latency bypass | Medium |
| Global oversampling | JUCE + new integration | JUCE revision below | JUCE upstream terms / AGPL-3.0-only | Measured anti-alias filtering where nonlinear aliasing exists | Global 2x/4x/8x FIR equiripple quality selection, applied only around active per-band nonlinear drive; T/S transient and sustain branches have independent oversampling passes before recombination; clean/dynamic EQ stays native-rate and incurs no oversampling latency | Medium |
| Analyzer transport and presentation | FreeEQ8 + default_distortion + ZLEqualizer | FreeEQ8/default_distortion revisions above; ZLEqualizer `26b0ed14cbbac254344e37d872235ce349b79c26` | GPL-3.0 / AGPL-3.0-only | Lock-free transport, family dual-spectrum roles, perceptual smoothing | Overlapped 2048/4096/8192 FFT; Hann coherent-gain normalisation; fractional-octave linear-power smoothing adapted from ZL; always both spectra; no piano/source selector | Low |
| Global bypass | default_distortion | `d145fab57e943869939d6a987cc69f90d676a4ae` | AGPL-3.0-only | Proven latency-aligned ramp | Dynamic maximum latency and family parameter IDs | Low |
| Family UI | default_distortion current code and design system | same | AGPL-3.0-only | User-owned visual source of truth | EQ-first graph, one always-open Band/Dynamic workspace, persistent Freq/Gain/Q/Slope fields, grayscale icon rows, global header controls, hover values, square monochrome controls, exact inversion and wordmark animation | Low |
| Framework | JUCE | `91ad83ae34a81e0833b1a2b0866f54846370ae53` | JUCE upstream terms | AU/VST3/Standalone and DSP primitives | Universal macOS build | External dependency |

## Runtime map

- Audio thread: cached atomic parameter snapshots, dirty-only filter updates,
  active dynamic detectors, optional drive-only nonlinear oversampling,
  partitioned linear convolution, meters, and conditional lock-free analyzer
  publication. No allocation, locks, file I/O, or analysis FFT.
- Background thread: linear-phase kernel construction.
- Message/UI thread: analyzer FFT consumption, cached response calculation,
  drawing, gestures, preferences, and undo/redo. High-resolution analyzer
  publication starts only while an editor is visible and stops on hide or
  editor destruction. Its always-on Peak Hold resets on each host transport
  stop-to-play transition and editor reopen.
- State: schema v9 contains one unambiguous APVTS audio state, migrates the
  audible slot from legacy schema-v3 A/B projects, and discards obsolete hidden
  link-group and dynamic-enable properties. Schema-v9 separates a band's
  persistent graph presence from its audio bypass. Schema-v5 channel routes migrate to
  continuous L/R, M/S or T/S placement, and its 0–100 drive controls migrate to the normalized
  algorithm semantics. Theme, window,
  RTA settings and Reduced Motion use a separate preferences
  file. Undo history, hover, drag, solo, audition, and FFT history are transient.

Natural Phase is intentionally absent: the audited upstream prototype was not a
validated convolution implementation. Minimum and selectable Linear Phase are
the supported phase modes.
