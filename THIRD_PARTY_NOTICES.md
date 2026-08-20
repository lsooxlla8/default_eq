# Third-party notices

## FreeEQ8

Primary architectural and code base:

- repository: https://github.com/GareBear99/FreeEQ8
- revision: `11376c1c496569c975e4d195c3fe5fd44b53d415`
- copyright: Gary Doman / GareBear99 and contributors
- license: GNU GPL version 3

This derivative changes product identity, exact band count, build targets, JUCE
version, license/update infrastructure, state serialization, bypass/latency,
filters, dynamic processing, interaction, and UI. Original history is retained.

## default_distortion

Reused author-owned family design, click-free latency-aligned bypass, Smart
Auto Gain interaction, and the first ten user-facing drive algorithms/control
semantics (Soft Clip through Sine Erosion):

- repository: https://github.com/lsooxlla8/default_distortion
- revision: `d145fab57e943869939d6a987cc69f90d676a4ae`
- files referenced: `docs/plugin-family-design-system.md`,
  `Source/GlobalBypass.h`, `Source/DistortionEngine.cpp`
- copyright: icanseesounds
- license: GNU AGPL version 3 only

The bypass was moved to `Source/DSP/GlobalBypass.h`, detached from distortion
parameters, and integrated with equalizer latency reporting.

### Indirect drive-algorithm provenance

`default_distortion` itself contains third-party-derived implementations. The
following provenance is retained here because the corresponding user-facing
drive modes and control semantics reached `default_equalizer` through that
project.

#### Vital Soft Clip and Hard Clip lineage

- repository: https://github.com/mtytel/vital
- revision: `636ca0ef517a4db087a6a08a6a8a5e704e21f836`
- original files: `src/synthesis/effects/distortion.cpp` and
  `src/synthesis/framework/futils.h`
- copyright: Copyright (C) 2013-2019 Matt Tytel
- license: GNU GPL version 3 or later

The referenced `default_distortion` revision contains scalar adaptations of
Vital's Soft Clip, Hard Clip, and rational `futils::tanh`. In
`default_equalizer`, the Soft Clip and Hard Clip modes were subsequently
modified in `Source/DSP/EQBand.h`: they use the C++ standard-library
`std::tanh`, a bounded cubic morph, and a conventional clamp. Vital's rational
`futils::tanh` implementation is **not** present in this repository. The mode
lineage is nevertheless disclosed, and the GPLv3 text is included at
`LICENSES/GPL-3.0.txt`.

#### CHOW Tape Model and BYOD hysteresis lineage

- repositories: https://github.com/jatinchowdhury18/AnalogTapeModel and
  https://github.com/Chowdhury-DSP/BYOD
- revisions: `604372e4ffd9690c3e283362e4598cb43edbb475` and
  `1cf22b6ac802b9dc33cfc9f8dd6af5b3c3e40bc9`
- original files: `Plugin/Source/Processors/Hysteresis/HysteresisOps.h`,
  `Plugin/Source/Processors/Hysteresis/HysteresisProcessing.*`,
  `src/processors/drive/hysteresis/HysteresisOps.h`, and
  `src/processors/drive/hysteresis/HysteresisProcessing.*`
- copyright: Copyright (C) Jatin Chowdhury and CHOW/BYOD contributors
- license: GNU GPL version 3

The referenced `default_distortion` revision contains a scalar adaptation of
the CHOW/BYOD Jiles-Atherton model. `default_equalizer` does **not** contain
that numerical model: its `Tape Hysteresis` mode is a smaller feedback-memory
waveshaper written for per-band operation, while retaining the inherited
Drive/Hysteresis/Bias vocabulary. This indirect lineage is disclosed to avoid
presenting the mode as wholly unrelated work. See `LICENSES/GPL-3.0.txt`.

## Faust vaeffects.lib matched filters

- repository: https://github.com/grame-cncm/faustlibraries
- file: `vaeffects.lib`
- revision: `ccc6030e60806011ae73c9502d9bca85ff2b79fa`
- relevant author/copyright: Dario Sanfilippo and Faust Libraries contributors
- license for the adapted matched-filter section: MIT
- mathematical reference: Martin Vicanek, “Matched Second Order Digital Filters”

The coefficient code in `Source/DSP/Biquad.h` is a modified C++ adaptation:
finite-domain guards, coefficient output, unsupported-case fallback, and a
smooth upper-frequency-only RBJ/matched blend were added. See
`LICENSES/MIT-Faust-vaeffects.txt`.

## ZLEqualizer

Analyzer FFT normalisation, fractional-octave linear-power smoothing and decay
architecture were adapted; no names, logos or branded assets were copied:

- repository: https://github.com/ZL-Audio/ZLEqualizer
- revision: `26b0ed14cbbac254344e37d872235ce349b79c26`
- copyright: zsliu98 and contributors
- license: GNU AGPL version 3

Modified integration lives in `Source/DSP/SpectrumFIFO.h` and
`Source/UI/ResponseCurveComponent.cpp`: JUCE FFT replaces ZL's SIMD FFT,
triple-buffer publication remains from this project's FreeEQ8-derived path,
and the UI uses the default_* monochrome dual-spectrum presentation.

## JUCE

- repository: https://github.com/juce-framework/JUCE
- submodule revision: `91ad83ae34a81e0833b1a2b0866f54846370ae53`
- licensing: see JUCE upstream terms

The upstream FreeEQ8 JUCE 7 revision was updated because it uses a CoreGraphics
API unavailable in the installed macOS 15 SDK.
