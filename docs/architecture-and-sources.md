# Architecture and source decision

Decision fixed before code transfer: FreeEQ8 is the primary code base;
ZLEqualizer is an audit reference; current `default_distortion` is authoritative
for family UI and reusable author-owned infrastructure. The combined derivative
is AGPL-3.0-only.

| Subsystem | Source | Revision | License | Why | Required changes | Risk |
|---|---|---|---|---|---|---|
| JUCE processor, APVTS, 24-band topology | FreeEQ8 | `11376c1c496569c975e4d195c3fe5fd44b53d415` | GPLv3 | Closest working vertical slice | One open-source target; remove DRM/update code; version state | Medium |
| Minimum-phase SVF/RBJ filters | FreeEQ8 | same | GPLv3 | Existing tested filter engine | Add notch; measure response; de-cramping remains incomplete | High near Nyquist |
| Dynamic EQ | FreeEQ8 | same | GPLv3 | Per-band envelope and modulation already integrated | Add upward/range/lookahead/external per-band sidechain | High |
| Linear phase | FreeEQ8 | same | GPLv3 | Background kernel rebuild and explicit latency | Quality choices, transitions, feature parity, validation | High |
| Natural phase | FreeEQ8, audit only | same | GPLv3 | Candidate implementation exists | Do not enable before convolution/phase validation | Critical |
| Match EQ | FreeEQ8 | same | GPLv3 | Capture and correction path already integrated | Target capture model, editable-band conversion, undo apply | High |
| Analyzer FIFO | FreeEQ8 plus behavior from default_distortion | both revisions | GPLv3/AGPLv3 | FFT stays off audio thread | Dual pre/post overlay and full preferences still needed | Medium |
| Global bypass | default_distortion | `d145fab57e943869939d6a987cc69f90d676a4ae` | AGPLv3-only | Proven ramp plus latency-aligned dry path | Adapt maximum latency and parameter ID | Low |
| Family UI | default_distortion design system/current code | same | AGPLv3-only | User-owned source of visual truth | EQ-first surface, square nodes/controls, exact inversion | Medium |
| Advanced EQ behavior comparison | ZLEqualizer | `26b0ed14cbbac254344e37d872235ce349b79c26` | AGPLv3 | Mature feature inventory | No code/assets copied; independently adapt later | Low license, high implementation |

## Runtime map

- Audio thread: parameter snapshots, preallocated SVF/biquad chains, optional
  drive oversampler, dynamic envelopes, Match EQ convolution, meter writes, and
  lock-free analyzer publication.
- Background thread: linear-phase magnitude/kernel rebuild.
- Message/UI thread: FFT consumption, response calculation, drawing, gestures,
  preferences, presets, and undo/redo commands.
- State: APVTS audio parameters plus versioned A/B snapshots; UI preferences are
  held in a separate `PropertiesFile`; undo history is never serialized.
