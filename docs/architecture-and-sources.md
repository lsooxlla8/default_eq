# Architecture and source decision

Decision fixed before transfer: FreeEQ8 is the primary architectural and code
base; current `default_distortion` supplies the family design system and
author-owned reusable DSP/UI infrastructure; ZLEqualizer is an audit reference
only. The combined derivative is AGPL-3.0-only.

| Subsystem | Source | Exact revision | License | Reason | Changes in default_equalizer | Risk |
|---|---|---|---|---|---|---|
| JUCE processor, APVTS, filter/parameter topology | FreeEQ8 | `11376c1c496569c975e4d195c3fe5fd44b53d415` | GPL-3.0 | Closest complete vertical slice | Exactly 8 bands, sidechain bus, mono/stereo safety, versioned A/B state, no DRM/updater/presets | Medium |
| Minimum-phase EQ and response model | FreeEQ8 + new work | same | GPL-3.0 / AGPL-3.0-only | Useful RBJ base | Seven types, continuous 0–48 dB/oct stages, exact fractional-stage UI response, per-band L/R/M/S, NaN guards | High near Nyquist |
| De-cramping | Faust `vaeffects.lib`, Vicanek matched-filter implementation | `ccc6030e60806011ae73c9502d9bca85ff2b79fa` | MIT | Published matched second-order fit with inspectable coefficients | C++ port, finite guards, smooth RBJ-to-matched blend limited to the upper spectrum, independent of oversampling | Medium |
| Dynamic EQ | FreeEQ8 + new work | FreeEQ8 revision above | GPL-3.0 / AGPL-3.0-only | Existing envelope/filter integration | Down/up, range, independent detector filters, external SC, transient audition, 5 ms lookahead, live GR | High |
| Linear phase | FreeEQ8 + new work | same | GPL-3.0 | Triple-buffered background kernel rebuild | 1024/2048/4096 taps, 512/1024/2048 samples latency, routed-band fallback, transition guard | High |
| Per-band drive | FreeEQ8 plus default_distortion | FreeEQ8 revision above; default_distortion `d145fab57e943869939d6a987cc69f90d676a4ae` | GPL-3.0 / AGPL-3.0-only | Reuse working nonlinear path and author-owned family algorithms | 8 algorithms, Mix, output compensation, DC blocker, per-band bypass, nonlinear-only 2x/4x/8x oversampling | Medium |
| Analyzer transport | FreeEQ8 plus default_distortion behavior | both revisions above | GPL-3.0 / AGPL-3.0-only | Lock-free triple buffer and proven dual-spectrum presentation | Input/output/both, worker-side 1024/2048/4096 FFT, floor/range/speed/average/tilt/hold/freeze/piano, UI preferences | Low |
| Match EQ | FreeEQ8 + new work | FreeEQ8 revision above | GPL-3.0 / AGPL-3.0-only | Existing capture concept | Analysis FFT moved off audio thread; Amount/Smoothing/range; result becomes 8 editable bands in one Undo transaction | Medium |
| Global bypass | default_distortion | `d145fab57e943869939d6a987cc69f90d676a4ae` | AGPL-3.0-only | Proven latency-aligned ramp | Dynamic maximum latency and family parameter IDs | Low |
| Family UI | default_distortion current code and design system | same | AGPL-3.0-only | User-owned visual source of truth | EQ-first graph, six compact contextual workspaces, hover values, square monochrome controls, exact inversion | Low |
| Feature comparison only | ZLEqualizer | `26b0ed14cbbac254344e37d872235ce349b79c26` | AGPL-3.0 | Useful behavior inventory | No ZLEqualizer code, names, logos, or assets copied | Low |
| Framework | JUCE | `91ad83ae34a81e0833b1a2b0866f54846370ae53` | JUCE upstream terms | AU/VST3/Standalone and DSP primitives | Universal macOS build | External dependency |

## Runtime map

- Audio thread: sanitized parameter snapshots, preallocated filter chains,
  envelope followers, optional nonlinear oversampling, linear convolution,
  meters, and lock-free sample publication. No allocation, locks, file I/O, or
  analysis FFT.
- Background threads: linear-phase kernel construction and Match EQ analysis.
- Message/UI thread: analyzer FFT consumption, response calculation, drawing,
  gestures, preferences, and undo/redo.
- State: schema v3 contains two complete APVTS audio snapshots. Theme, window,
  RTA, Match-analysis controls, and Reduced Motion use a separate preferences
  file. Undo history, hover, drag, solo, audition, and FFT history are transient.

Natural Phase is intentionally absent: the audited upstream prototype was not a
validated convolution implementation. Minimum and selectable Linear Phase are
the supported phase modes.
