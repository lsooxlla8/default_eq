# Architecture and source decision

Decision fixed before transfer: FreeEQ8 is the primary architectural and code
base; current `default_distortion` supplies the family design system and
author-owned reusable DSP/UI infrastructure; ZLEqualizer supplies the analyzer
smoothing/normalisation reference. The combined derivative is AGPL-3.0-only.

| Subsystem | Source | Exact revision | License | Reason | Changes in default_eq8 | Risk |
|---|---|---|---|---|---|---|
| JUCE processor, APVTS, filter/parameter topology | FreeEQ8 | `11376c1c496569c975e4d195c3fe5fd44b53d415` | GPL-3.0 | Closest complete vertical slice | Exactly 8 bands, sidechain bus, mono/stereo safety, versioned single audio state, no DRM/updater/presets/A-B | Medium |
| Minimum-phase EQ and response model | FreeEQ8 + new work | same | GPL-3.0 / AGPL-3.0-only | Useful RBJ base | Eight types including symmetric Tilt, continuous 3–48 dB/oct, 6 dB cut sections and monotonic fractional response, continuous per-band L–R or M–S placement, NaN guards | High near Nyquist |
| De-cramping | Faust `vaeffects.lib`, Vicanek matched-filter implementation | `ccc6030e60806011ae73c9502d9bca85ff2b79fa` | MIT | Published matched second-order fit with inspectable coefficients | C++ port, finite guards, smooth RBJ-to-matched blend limited to the upper spectrum, independent of oversampling | Medium |
| Dynamic EQ | FreeEQ8 + new work | FreeEQ8 revision above | GPL-3.0 / AGPL-3.0-only | Existing envelope/filter integration | Down/up, range, independent detector filters, external SC, transient audition, continuous 0–5 ms lookahead, live GR | High |
| Linear phase | FreeEQ8 + JUCE partitioned convolution + new work | FreeEQ8/JUCE revisions above | GPL-3.0 / JUCE upstream terms / AGPL-3.0-only | Preserve the validated magnitude/FIR model without paying for a monolithic FFT per host block | Background symmetric-FIR construction; wait-free IR ownership transfer; zero-additional-latency uniform partitions; 1024/2048/4096 taps and 512/1024/2048 samples latency; routed-band fallback and transition guard | High |
| Per-band drive | FreeEQ8 plus default_distortion; indirect upstream lineage from Vital and CHOW Tape Model/BYOD | FreeEQ8 revision above; default_distortion `d145fab57e943869939d6a987cc69f90d676a4ae`; Vital `636ca0ef517a4db087a6a08a6a8a5e704e21f836`; AnalogTapeModel `604372e4ffd9690c3e283362e4598cb43edbb475`; BYOD `1cf22b6ac802b9dc33cfc9f8dd6af5b3c3e40bc9` | GPL-3.0 / AGPL-3.0-only | Carry the family drive modes and controls without hiding nested upstream provenance | First 10 displayed algorithms and algorithm-specific Character; Soft/Hard use `std::tanh`/cubic/clamp rather than Vital's rational tanh; Tape uses a purpose-built feedback-memory shaper rather than the CHOW/BYOD Jiles-Atherton code; compatible stored Secondary semantics, always-on deterministic Auto Gain tables, spectral band extraction, DC blocking, and a true zero-Drive CPU/latency bypass | Medium |
| Global oversampling | JUCE + new integration | JUCE revision below | JUCE upstream terms / AGPL-3.0-only | Measured anti-alias filtering where nonlinear aliasing exists | Global 2x/4x/8x FIR equiripple quality selection, applied only around active per-band nonlinear drive; clean/dynamic EQ stays native-rate and incurs no oversampling latency | Medium |
| Analyzer transport and presentation | FreeEQ8 + default_distortion + ZLEqualizer | FreeEQ8/default_distortion revisions above; ZLEqualizer `26b0ed14cbbac254344e37d872235ce349b79c26` | GPL-3.0 / AGPL-3.0-only | Lock-free transport, family dual-spectrum roles, perceptual smoothing | Overlapped 2048/4096/8192 FFT; Hann coherent-gain normalisation; fractional-octave linear-power smoothing adapted from ZL; always both spectra; no piano/source selector | Low |
| Match EQ | FreeEQ8 + new work | FreeEQ8 revision above | GPL-3.0 / AGPL-3.0-only | Existing capture concept | Analysis FFT moved off audio thread; Amount/Smoothing/range; result becomes 8 editable bands in one Undo transaction | Medium |
| Global bypass | default_distortion | `d145fab57e943869939d6a987cc69f90d676a4ae` | AGPL-3.0-only | Proven latency-aligned ramp | Dynamic maximum latency and family parameter IDs | Low |
| Family UI | default_distortion current code and design system | same | AGPL-3.0-only | User-owned visual source of truth | EQ-first graph, two combined always-open workspaces, persistent Freq/Gain/Q/Slope fields, grayscale icon rows, global header controls, hover values, square monochrome controls, exact inversion and wordmark animation | Low |
| Framework | JUCE | `91ad83ae34a81e0833b1a2b0866f54846370ae53` | JUCE upstream terms | AU/VST3/Standalone and DSP primitives | Universal macOS build | External dependency |

## Runtime map

- Audio thread: cached atomic parameter snapshots, dirty-only filter updates,
  active dynamic detectors, optional drive-only nonlinear oversampling,
  partitioned linear convolution, meters, and conditional lock-free analyzer
  publication. No allocation, locks, file I/O, or analysis FFT.
- Background threads: linear-phase kernel construction and Match EQ analysis.
- Message/UI thread: analyzer FFT consumption, cached response calculation,
  drawing, gestures, preferences, and undo/redo. High-resolution analyzer
  publication starts only while an editor is visible and stops on hide or
  editor destruction. Its always-on Peak Hold resets on each host transport
  stop-to-play transition and editor reopen.
- State: schema v9 contains one unambiguous APVTS audio state, migrates the
  audible slot from legacy schema-v3 A/B projects, and discards obsolete hidden
  link-group and dynamic-enable properties. Schema-v9 separates a band's
  persistent graph presence from its audio bypass. Schema-v5 channel routes migrate to
  continuous placement, and its 0–100 drive controls migrate to the normalized
  algorithm semantics. Theme, window,
  RTA, Match-analysis controls, and Reduced Motion use a separate preferences
  file. Undo history, hover, drag, solo, audition, and FFT history are transient.

Natural Phase is intentionally absent: the audited upstream prototype was not a
validated convolution implementation. Minimum and selectable Linear Phase are
the supported phase modes.
