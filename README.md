# default_eq8

![default\_equalizer plug-in interface](docs/screenshots/ui-aligned-rail-tooltip-860.png)

**default_eq8** is designed to be your go-to EQ for shaping, balancing, and
dynamically controlling audio.

Built around eight fully independent bands, it combines parametric and dynamic EQ, per-band saturation, Linear Phase processing, variable filter slopes, two Auto Gain modes, Match EQ, and a real-time spectrum analyzer in a compact, modern interface.

Most editing happens directly on the analyzer:

- **Drag** a band to adjust frequency and gain. Hold **Cmd** while dragging to
  control Drive, or **Shift** to adjust the dynamics Threshold.
- Use the **mouse wheel** for Q, **Shift + wheel** for Pan, and
  **Ctrl/Cmd + wheel** for filter Slope.
- For precise adjustments, **Freq, Gain, Q, and Slope** can also be entered
  directly in the parameter fields below.
- **Alt-click** a band to momentarily solo it.
- **Ctrl/Cmd + click** for bypass.


There is deliberately no internal preset system. Plugin state is handled
through the host, with complete project recall, versioned state, and Undo/Redo
support.

## Thanks and third-party code

* [FreeEQ8](https://github.com/GareBear99/FreeEQ8) by GareBear99, providing the
  GPLv3 core equalizer DSP and logic.
* [ZLEqualizer](https://github.com/ZL-Audio/ZLEqualizer) by ZL-Audio, providing
  the AGPLv3 analyzer architecture.
* [default_distortion](https://github.com/lsooxlla8/default_distortion),
  providing the `default_*` UI language, bypass and Auto Gain interaction, and
  drive algorithm semantics.
* [Faust Libraries `vaeffects.lib`](https://github.com/grame-cncm/faustlibraries/blob/ccc6030e60806011ae73c9502d9bca85ff2b79fa/vaeffects.lib)
  by Dario Sanfilippo and Faust Libraries contributors, providing MIT-licensed
  matched-filter coefficient code used by de-cramping.
* [Vital](https://github.com/mtytel/vital/tree/636ca0ef517a4db087a6a08a6a8a5e704e21f836),
  [CHOW Tape Model](https://github.com/jatinchowdhury18/AnalogTapeModel/tree/604372e4ffd9690c3e283362e4598cb43edbb475),
  and [BYOD](https://github.com/Chowdhury-DSP/BYOD/tree/1cf22b6ac802b9dc33cfc9f8dd6af5b3c3e40bc9),
  providing the GPLv3 upstream lineage of drive modes inherited through
  `default_distortion`.

## Build

```sh
git submodule update --init --recursive

cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEFAULT_EQUALIZER_BUILD_TESTS=ON \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cmake --build build --config Release -j 8
ctest --test-dir build --output-on-failure
```

On Windows and Linux, omit `CMAKE_OSX_ARCHITECTURES`. AU is built only on
macOS.

## Licence

**default_eq8** is open source under `AGPL-3.0-only`.

FreeEQ8-derived portions retain their `GPL-3.0-only` notices and are combined
with the project under GPLv3 section 13.

See [`LICENSE.md`](LICENSE.md), [`LICENSES/`](LICENSES/), and
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

Copyright (C) 2026 icanseesounds and upstream copyright holders.
