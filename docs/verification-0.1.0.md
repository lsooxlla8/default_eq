# Verification record — 0.1.0

Verified locally on macOS on 2026-08-13 from a universal Release build.

## Build and host validation

- CMake Release: AU, VST3 and Standalone built successfully for `arm64` and
  `x86_64`.
- CTest: 9/9 passed.
- `auval -v aufx Deq1 Icss`: `AU VALIDATION SUCCEEDED`, including mono,
  stereo, 64/137/512/4096-frame renders and 22.05–192 kHz sample rates.
- AU, VST3 and Standalone bundles pass strict ad-hoc `codesign --verify`.
- AU/VST3 expose no named or usable factory programs. The AU SDK still prints
  its empty `HAS FACTORY PRESETS` section header, but there are no IDs below it.

## Measured DSP invariants

- Neutral stereo, Mid/Side and mono: worst delta below `2e-5`.
- Serialized project-state audio recall: worst sample delta `0` in the
  deterministic render test.
- External sidechain accumulated output: silent `615.826`, active `154.691`.
- Dynamic block-size levels: `0.0806030` at 64 samples, `0.0806018` at 257,
  `0.0806030` at 1024; relative spread `0.001%`.
- Hard Clip alias-energy probe: Off `1830.48`, 8x `2.77624` (`0.15%`).
- High-frequency bell analog-prototype error: RBJ `3.3813 dB`, de-cramped
  matched filter `2.2956 dB`.
- De-cramping low-frequency worst response delta: `0.00000 dB` in the
  low/mid-frequency probe.
- Economy Linear Phase impulse peak: sample `511`; reported latency `512`.
  This is the expected half-sample rounding of a symmetric even-length FIR.

## Reported latency

Values are additive when features are combined.

| Feature | Reported latency |
|---|---:|
| Minimum Phase, no lookahead, clean EQ | 0 samples |
| Linear Economy, 1024 taps | 512 samples |
| Linear Standard, 2048 taps | 1024 samples |
| Linear High, 4096 taps | 2048 samples |
| Dynamic lookahead | 5 ms; 240 samples at 48 kHz |
| Drive oversampling 2x | 3 samples |
| Drive oversampling 4x | 4 samples |
| Drive oversampling 8x | 5 samples |

Oversampling contributes no latency when no nonlinear per-band drive is
actually active.

## Visual QA

The live Standalone window was inspected at 720, 860 and 1200 px. Band,
Dynamic, Drive, RTA, Match and Global workspaces remain readable and within the
window. The 720 px RTA page is the densest verified layout. Dark and light
screenshots confirm exact paper/ink role inversion.

- `docs/screenshots/default_equalizer-dark-final-720.jpeg`
- `docs/screenshots/default_equalizer-dark-final-860.jpeg`
- `docs/screenshots/default_equalizer-dark-1200.jpeg`
- `docs/screenshots/default_equalizer-light-860.jpeg`
- `docs/screenshots/default_equalizer-match-860.jpeg`

## Known boundaries

- Natural Phase is not exposed. Minimum Phase and optional Linear Phase are the
  supported modes.
- In Linear Phase, Stereo static bands are synthesized into the FIR. Per-band
  Left/Right/Mid/Side filters remain minimum-phase post stages because one
  shared stereo FIR cannot represent independent channel routing.
- Analyzer and Match FFT work is outside the audio thread. The optional Linear
  Phase convolution itself uses a preallocated real-time FFT path.
