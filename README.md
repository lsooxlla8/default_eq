# default_equalizer

`default_equalizer` is an open-source eight-band parametric dynamic equalizer
for AU, VST3, and standalone hosts. It is a substantially modified derivative
of [FreeEQ8](https://github.com/GareBear99/FreeEQ8); exact provenance is retained
in Git history and [third-party notices](THIRD_PARTY_NOTICES.md).

The interface follows the `default_*` paper/ink family but is designed around
direct graph editing: frequency and gain are moved on the RTA, Q uses the mouse
wheel, and detailed controls appear only in compact per-band workspaces. There
is deliberately no preset concept; host project recall, versioned state, A/B,
and Undo/Redo remain complete.

See [implementation status](docs/implementation-status.md) for verified scope,
and the [0.1.0 verification record](docs/verification-0.1.0.md) for measured
latency, DSP results, host validation, and visual QA.

## Build on macOS

```sh
git submodule update --init --recursive
cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEFAULT_EQUALIZER_BUILD_TESTS=ON \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j 8
ctest --test-dir build --output-on-failure
```

## License

The combined project is GNU AGPL version 3 only. FreeEQ8-derived portions retain
their GPLv3 notices; GPLv3 section 13 permits combination with AGPLv3. See
[LICENSE.md](LICENSE.md), [AGPL-3.0](LICENSES/AGPL-3.0.txt), and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Copyright (C) 2026 icanseesounds and upstream copyright holders.
