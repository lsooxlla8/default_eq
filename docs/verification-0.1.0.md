# Verification record — 0.1.0

Verified locally on macOS on 2026-08-13 from a universal Release build.

## Build and host validation

- CMake Release: AU, VST3 and Standalone built successfully for `arm64` and
  `x86_64`.
- CTest: 9/9 passed.
- `auval -v aufx Deq1 Icss`: `AU VALIDATION SUCCEEDED`, including mono,
  stereo, 64/137/512/4096-frame renders and 22.05–192 kHz sample rates.
- AU, VST3 and Standalone bundles pass strict ad-hoc `codesign --verify`.
- No A/B state or hidden A/B link-group parameters are published to the host.
- AU/VST3 expose no named or usable factory programs. The AU SDK still prints
  its empty `HAS FACTORY PRESETS` section header, but there are no IDs below it.

## Measured DSP invariants

- Neutral stereo, Mid/Side and mono: worst delta below `2e-5`.
- Serialized project-state audio recall: worst sample delta `0` in the
  deterministic render test.
- External sidechain accumulated output: silent `615.826`, active `154.691`.
- Dynamic block-size levels: `0.0806030` at 64 samples, `0.0806018` at 257,
  `0.0806030` at 1024; relative spread `0.001%`.
- Hard Clip alias-energy probe: Off `13.4868`, 8x `0.114383` (`0.85%`).
- High-frequency bell analog-prototype error: RBJ `3.3813 dB`, de-cramped
  matched filter `2.2956 dB`.
- De-cramping low-frequency worst response delta: `0.00000 dB` in the
  low/mid-frequency probe.
- Economy Linear Phase impulse peak: sample `512`; reported latency `512`.
- Economy, Standard and High all produce finite, non-silent, bounded impulses
  on 257-sample host blocks and align at their reported 512/1024/2048 samples.
- Variable low-cut rejection is monotonic across measured 3, 6, 9, 12, 18,
  24, 36 and 48 dB/oct settings.
- An 8 kHz band's solo path rejects a 100 Hz program tone by more than 20x;
  its hard-clip drive changes the in-band tone while leaving 100 Hz within 2%.

## Reported latency

Values are additive when features are combined.

| Feature | Reported latency |
|---|---:|
| Minimum Phase, no lookahead, clean EQ | 0 samples |
| Linear Economy, 1024 taps | 512 samples |
| Linear Standard, 2048 taps | 1024 samples |
| Linear High, 4096 taps | 2048 samples |
| Dynamic lookahead | 0–5 ms; 0–240 samples at 48 kHz |
| Global oversampling 2x | 49 samples |
| Global oversampling 4x | 60 samples |
| Global oversampling 8x | 64 samples |

Oversampling is a global quality mode covering dynamic EQ and drive, so its
filter latency applies whenever a non-Off factor is selected.

## Visual QA

The live Standalone window was inspected at 720, 860 and 1200 px using Computer
Use. Band, Dynamic, Drive, RTA and Match workspaces remain readable and within
the window; the formerly separate sparse Global page was folded into the
header. The 720 px Dynamic page is the densest verified layout. Dark and light
screenshots confirm exact paper/ink role inversion.

- `docs/screenshots/default_equalizer-dark-final-720.jpeg`
- `docs/screenshots/default_equalizer-dark-final-860.jpeg`
- `docs/screenshots/default_equalizer-dark-1200.jpeg`
- `docs/screenshots/default_equalizer-light-860.jpeg`
- `docs/screenshots/default_equalizer-match-860.jpeg`
- `docs/screenshots/default_equalizer-light-final-2026-08-13.jpeg`

## Known boundaries

- Natural Phase is not exposed. Minimum Phase and optional Linear Phase are the
  supported modes.
- In Linear Phase, Stereo static bands are synthesized into the FIR. Per-band
  Left/Right/Mid/Side filters remain minimum-phase post stages because one
  shared stereo FIR cannot represent independent channel routing.
- Analyzer and Match FFT work is outside the audio thread. The optional Linear
  Phase convolution itself uses a preallocated real-time FFT path.
