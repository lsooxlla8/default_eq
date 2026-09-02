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

Reused author-owned family infrastructure and interaction conventions:

- repository: https://github.com/lsooxlla8/default_distortion
- revision: `d145fab57e943869939d6a987cc69f90d676a4ae`
- files referenced: `docs/plugin-family-design-system.md`,
  `Source/GlobalBypass.h`, `Source/DistortionEngine.cpp`
- copyright: icanseesounds
- license: GNU AGPL version 3 only

The bypass was copied into `Source/DSP/GlobalBypass.h`, detached from distortion
parameters, and integrated with equalizer latency reporting. The family theme
preferences, drive control semantics, and deterministic lookup-table approach
were also carried over, while the current EQ-specific tables were generated
against `default_eq` itself.

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

Analyzer FFT normalisation, fractional-octave linear-power smoothing, decay
architecture, Ivantsov coefficient equations, discrete cascade design, Flat
Tilt coefficient design and the selected-band dynamic-range interaction were
adapted; no names, logos or branded assets were copied:

- repository: https://github.com/ZL-Audio/ZLEqualizer
- analyzer reference revision: `26b0ed14cbbac254344e37d872235ce349b79c26`
- filter-design revision: `02c517e35f0ef8460c15815f303051dffdb0895a`
- copyright: zsliu98 and contributors
- license: GNU AGPL version 3

Modified integration lives in `Source/DSP/SpectrumFIFO.h`,
`Source/DSP/ZLFilter.h`, `Source/DSP/EQBand.h` and
`Source/UI/ResponseCurveComponent.cpp`: JUCE FFT replaces ZL's SIMD FFT,
triple-buffer publication remains from this project's FreeEQ8-derived path,
the range handle is a square, and the UI uses the default_* monochrome
dual-spectrum presentation.

## ZLSplitter

Transient/sustain separation is adapted from the upstream TSSplitter:

- repository: https://github.com/ZL-Audio/ZLSplitter
- revision: `2f50824ab925eeff7950986eac640dab43c3ce67`
- files referenced: `source/dsp/splitter/ts_splitter/ts_splitter.hpp`,
  `source/dsp/splitter/ts_splitter/median_filter.hpp`, and
  `source/dsp/filter/fir_filter/fir_base.hpp`
- copyright: zsliu98 and contributors
- license: GNU AGPL version 3

The adapted implementation is in `Source/DSP/TransientSplitter.h`. It retains
the 75% overlap, Hann-window overlap-add, 5-bin/5-frame median mask, two-hop
spectral delay and upstream parameter transforms. JUCE FFT replaces KFR, and
the code is integrated as one shared stereo splitter feeding per-band T/S
routes; no ZL branding or assets are included.

## JUCE

- repository: https://github.com/juce-framework/JUCE
- submodule revision: `91ad83ae34a81e0833b1a2b0866f54846370ae53`
- licensing: see JUCE upstream terms

The upstream FreeEQ8 JUCE 7 revision was updated because it uses a CoreGraphics
API unavailable in the installed macOS 15 SDK.

Popup and hover-submenu positioning is implemented in project-owned UI code;
the JUCE submodule is used without local source patches.
