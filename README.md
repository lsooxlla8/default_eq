# default_eq

![default_eq plug-in interface](docs/screenshots/ui-aligned-rail-tooltip-860.png)

**default_eq** is designed to be your go-to EQ for tonal shaping, dynamic control, and adding harmonic character exactly where you need it.

Built around eight fully independent bands, it combines parametric and dynamic
EQ, per-band saturation, Linear Phase processing, two Auto Gain modes,
L/R, M/S and transient/sustain routing, and a real-time spectrum analyzer in a
compact interface.

Most editing happens directly on the analyzer:

- **Click empty space** to create a filter: Bell in the middle, Low Shelf in
  the upper left, High Shelf in the upper right, High Cut in the lower left,
  or Low Cut in the lower right.
- **Shift-click empty space** to create Tilt, except in the lower far-left and
  far-right areas, which create resonant High Cut and Low Cut respectively.
- **Cmd-drag** a band to adjust Drive; **Shift-drag** it to adjust the dynamic
  Threshold.
- Use the **mouse wheel** for Q (or Slope on non-resonant Cut filters),
  **Shift-wheel** for placement, **Alt-wheel** for Slope, and **Cmd-wheel** for
  Character.
- **Shift-click** individual bands to add or remove them from the selection.
  **Shift-drag** or **right-drag** a marquee over empty graph space to select
  multiple bands.
- **Alt-click** a band to momentarily solo it; **Cmd-click** toggles bypass.
- **Right-click** a band to choose its filter, L/R–M/S–T/S placement,
  saturation mode, or reset the equalizer. **Double-click** a band to delete it.
- **Shift-Cmd-click** a band to reset its placement. **Alt-right-click** resets
  its Slope; **Cmd-right-click** resets its Drive and Character.
- For precise adjustments, **Freq, Gain, Q, and Slope** can also be entered via double-click
  directly in the parameter fields below. 

There is deliberately no internal preset system. Plugin state is handled
through the host, with complete project recall, versioned state, and Undo/Redo
support.

## Thanks and third-party code

- [FreeEQ8](https://github.com/GareBear99/FreeEQ8) by GareBear99, providing the
  GPLv3 core equalizer DSP and logic.
- [ZLEqualizer](https://github.com/ZL-Audio/ZLEqualizer) by ZL-Audio, providing
  AGPLv3 filter design and analyzer architecture.
- [ZLSplitter](https://github.com/ZL-Audio/ZLSplitter) by ZL-Audio, providing
  AGPLv3 transient/sustain separation.
- [Faust Libraries `vaeffects.lib`](https://github.com/grame-cncm/faustlibraries/blob/ccc6030e60806011ae73c9502d9bca85ff2b79fa/vaeffects.lib)
  by Dario Sanfilippo and Faust Libraries contributors, providing MIT-licensed
  matched-filter coefficient code used by de-cramping.

Exact repositories, revisions, licences, modifications, and code boundaries
are documented in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). Included
license texts are in [`LICENSES/`](LICENSES/).

## Build

```sh
git submodule update --init --recursive

cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEFAULT_EQ_BUILD_TESTS=ON \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cmake --build build --config Release -j 8
ctest --test-dir build --output-on-failure
```

On Windows and Linux, omit `CMAKE_OSX_ARCHITECTURES`. AU is built only on
macOS.

## Licence

**default_eq** is open source under `AGPL-3.0-only`.

FreeEQ8-derived portions retain their `GPL-3.0-only` notices and are combined
with the project under GPLv3 section 13.

See [`LICENSE.md`](LICENSE.md), [`LICENSES/`](LICENSES/), and
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

Copyright (C) 2026 icanseesounds and upstream copyright holders.
