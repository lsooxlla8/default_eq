# Third-party notices

## FreeEQ8

Primary architectural and code base:

- repository: https://github.com/GareBear99/FreeEQ8
- revision: `11376c1c496569c975e4d195c3fe5fd44b53d415`
- copyright: Gary Doman / GareBear99 and contributors
- license: GNU GPL version 3

This derivative changes product identity, build targets, JUCE version, license
and update infrastructure, state serialization, bypass/latency behavior, filter
types, interaction gestures, and UI styling. Original history is retained.

## default_distortion

Reused author-owned family design and click-free latency-aligned bypass:

- repository: https://github.com/lsooxlla8/default_distortion
- revision: `d145fab57e943869939d6a987cc69f90d676a4ae`
- files referenced: `docs/plugin-family-design-system.md`,
  `Source/GlobalBypass.h`
- copyright: icanseesounds
- license: GNU AGPL version 3 only

The bypass was moved to `Source/DSP/GlobalBypass.h`, detached from distortion
parameters, and integrated with equalizer latency reporting.

## ZLEqualizer

Functional audit reference only; no source or branded assets were copied:

- repository: https://github.com/ZL-Audio/ZLEqualizer
- revision: `26b0ed14cbbac254344e37d872235ce349b79c26`
- copyright: zsliu98 and contributors
- license: GNU AGPL version 3

## JUCE

- repository: https://github.com/juce-framework/JUCE
- version: 8.0.15, submodule revision `91ad83ae34...`
- licensing: see JUCE upstream terms

The upstream FreeEQ8 JUCE 7 revision was updated because it uses a CoreGraphics
API unavailable in the installed macOS 15 SDK.
