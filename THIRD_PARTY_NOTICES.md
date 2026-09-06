# Third-party notices

## FreeEQ8

Original code base from which this repository was directly forked:

- repository: https://github.com/GareBear99/FreeEQ8
- revision: `11376c1c496569c975e4d195c3fe5fd44b53d415`
- copyright: Gary Doman / GareBear99 and contributors
- license: GNU GPL version 3

The current filter design and much of the product logic have since been
replaced or substantially reworked. Modified FreeEQ8-derived portions remain
in the processor lifecycle, APVTS parameter/state scaffolding, band engine,
spectrum transport, Linear Phase infrastructure, and related integration code.
Original history is retained.

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

## JetBrains Mono

The interface uses an embedded copy of JetBrains Mono so that glyph shapes and font
metrics do not depend on fonts installed by the host operating system:

- repository: https://github.com/JetBrains/JetBrainsMono
- release: `v2.304`
- revision: `cd5227bd1f61dff3bbd6c814ceaf7ffd95e947d9`
- files: `Resources/Fonts/JetBrainsMono/JetBrainsMono-Medium.woff2`,
  `Resources/Fonts/JetBrainsMono/JetBrainsMono-ExtraBold.woff2`, and their
  matching TTF files
- copyright: Copyright 2020 The JetBrains Mono Project Authors
- license: SIL Open Font License 1.1

The WOFF2 files are used by the HTML interface prototypes. The matching static
TTFs are retained for embedding into the JUCE interface. The complete license is
included in `LICENSES/OFL-JetBrainsMono.txt`.

## JUCE

- repository: https://github.com/juce-framework/JUCE
- submodule revision: `91ad83ae34a81e0833b1a2b0866f54846370ae53`
- licensing: see JUCE upstream terms

The upstream FreeEQ8 JUCE 7 revision was updated because it uses a CoreGraphics
API unavailable in the installed macOS 15 SDK.

Popup and hover-submenu positioning is implemented in project-owned UI code;
the JUCE submodule is used without local source patches.
