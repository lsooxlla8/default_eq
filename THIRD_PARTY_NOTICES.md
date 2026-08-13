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

Reused author-owned family design, click-free latency-aligned bypass, and
adapted stateless Morph Soft Clip, Hard Clip, Recursive Fold, and Sine Fold
per-band drive modes:

- repository: https://github.com/lsooxlla8/default_distortion
- revision: `d145fab57e943869939d6a987cc69f90d676a4ae`
- files referenced: `docs/plugin-family-design-system.md`,
  `Source/GlobalBypass.h`, `Source/DistortionEngine.cpp`
- copyright: icanseesounds
- license: GNU AGPL version 3 only

The bypass was moved to `Source/DSP/GlobalBypass.h`, detached from distortion
parameters, and integrated with equalizer latency reporting.

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

Functional audit reference only; no source or branded assets were copied:

- repository: https://github.com/ZL-Audio/ZLEqualizer
- revision: `26b0ed14cbbac254344e37d872235ce349b79c26`
- copyright: zsliu98 and contributors
- license: GNU AGPL version 3

## JUCE

- repository: https://github.com/juce-framework/JUCE
- submodule revision: `91ad83ae34a81e0833b1a2b0866f54846370ae53`
- licensing: see JUCE upstream terms

The upstream FreeEQ8 JUCE 7 revision was updated because it uses a CoreGraphics
API unavailable in the installed macOS 15 SDK.
