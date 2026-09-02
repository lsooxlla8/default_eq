# Phase 1: Safety net and baseline

Recorded on 2026-08-30 from `ef83492b78f79c070990b55b9251e9d25e404378`
with a macOS 26.5.2 arm64 Release build (Apple clang 21.0.0, CMake 4.3.3).

## Host parameter contract

`host_parameter_regression` locks all 173 host-visible parameters. For every
index it checks the stable ID, display name, label, range endpoints, interval,
skew, default, host index, choice labels, category, and automatable, meta,
orientation, discrete, and boolean flags. A deliberate parameter-contract
change must update this test explicitly and be treated as a compatibility
decision.

## Automation fuzz baseline

`automation_fuzz` uses fixed seeds and runs every exposed parameter through a
shuffled automation sweep at 44.1, 48, 96, and 192 kHz with block sizes 17, 64,
257, and 512. Each scenario performs 396 changes and is repeated to verify
deterministic serialized state. Every block checks finite output, an independent
reported-latency calculation, and whether processing itself changes latency.
State save/restore must be byte-stable and retain the reported latency.

Initial maximum adjacent-sample steps were 0.0171, 0.0203, 0.0448, and 0.0952.
The regression budget is 0.5. Initial peak magnitudes were 0.0245, 0.0296,
0.0299, and 0.1365; the budget is 2.0. These are safety limits rather than DSP
equivalence tolerances; sample-equivalence checks remain required during the
Phase 2 refactor.

## Memory baseline and budgets

`memory_regression` records both fixed object size and prepared runtime RSS at
48 kHz with a 512-sample block. It keeps five prepared instances alive and uses
the slope from instances two through five, excluding first-instance code pages
and one-time JUCE setup. Preparation and processing touch the processor object,
oversampling pool, T/S splitters, linear-phase engine, lookahead and detector
workspaces, and analyzer storage.

| Metric | Initial macOS Release baseline | Regression budget |
|---|---:|---:|
| `sizeof(DefaultEqualizerAudioProcessor)` | 1,102,392 bytes | 1,130,000 bytes |
| Prepared RSS per retained instance | 26,972,160 bytes (approximately 27.0 MB) | 41,943,040 bytes (40 MiB) |

RSS varies with the platform allocator and loaded runtime, so its budget has
more headroom than the fixed-size budget. The test prints raw byte counts in a
machine-readable `key=value` form for future profiling comparisons.

## Running the gates

```sh
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEFAULT_EQ_BUILD_TESTS=ON
cmake --build build-release --parallel 3
ctest --test-dir build-release --output-on-failure
```

Focused reruns:

```sh
ctest --test-dir build-release -R host_parameter_regression --output-on-failure
ctest --test-dir build-release -R automation_fuzz --output-on-failure
ctest --test-dir build-release -R memory_regression --output-on-failure
```
