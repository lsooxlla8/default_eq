# default_eq

**default_eq** is designed to be your go-to EQ for shaping, balancing, and
dynamically controlling audio.

Built around eight fully independent bands, it combines parametric and dynamic
EQ, per-band saturation, Linear Phase processing, two Auto Gain modes,
L/R, M/S and transient/sustain routing, and a real-time spectrum analyzer in a
compact interface.

Most editing happens directly on the analyzer:

- **Drag** a band to adjust frequency and gain. Hold **Cmd** while dragging to
  control Drive, or **Shift** to adjust the dynamics Threshold.
- Use the **mouse wheel** for Q, **Shift + wheel** for placement,
  **Alt + wheel** for Slope, and **Cmd + wheel** for Character.
- **Shift-click** bands or **Shift-drag** or **Right-click-drag** a marquee over empty graph space to select and
  edit multiple bands relatively.
- Use global **Shift** to move every band by the same musical interval.
- For precise adjustments, **Freq, Gain, Q, and Slope** can also be entered
  directly in the parameter fields below.
- **Alt-click** a band to momentarily solo it; **Cmd-click** toggles bypass.
- **Shift + Cmd + click** a band to reset it's placement.
- **Cmd + Right-click** a band to reset it's drive and character.
- **Shift + Right-click** a band to reset it's threshold.

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
- [Vital](https://github.com/mtytel/vital/tree/636ca0ef517a4db087a6a08a6a8a5e704e21f836),
  providing the disclosed GPLv3 upstream lineage of Soft Clip inherited
  through `default_distortion`.

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
