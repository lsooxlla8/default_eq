# default_equalizer

`default_equalizer` is an open-source 24-band parametric dynamic equalizer for
AU, VST3, and standalone hosts. This repository is a modified derivative of
[FreeEQ8](https://github.com/GareBear99/FreeEQ8), with project history and
attribution preserved.

The current development version provides a working vertical slice: interactive
EQ nodes, seven filter shapes, per-band dynamic processing and drive, minimum-
phase and linear-phase paths, analyzer, Match EQ, A/B state, undo/redo, adaptive
Q, Mid/Side routing, oversampling for nonlinear processing, and click-free
global bypass. See [implementation status](docs/implementation-status.md) for
the exact verified boundary; unfinished features are not represented as done.

## Build on macOS

```sh
git submodule update --init --recursive
cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEFAULT_EQUALIZER_BUILD_TESTS=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release -j 8
ctest --test-dir build --output-on-failure
```

## Licensing

The combined project is licensed under GNU AGPL version 3 only. FreeEQ8-derived
files retain their GPLv3 notices; GPLv3 section 13 permits their combination
with this AGPLv3 work. See [LICENSE.md](LICENSE.md) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Copyright (C) 2026 icanseesounds and the upstream copyright holders.
