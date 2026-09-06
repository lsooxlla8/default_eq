# default_eq 0.5.1 verification

## Scope

- Corrected the FreeEQ8 attribution boundary after auditing the current source
  tree and retained GPL-derived code.
- Removed the unused Faust matched-filter implementation, its dead de-cramping
  plumbing, tests, notice and MIT licence copy.
- Reduced every binary package to plug-in/Standalone binaries, a release README
  without the Build section, one consolidated `LEGAL.txt`, and a platform-native
  double-click install script.
- Replaced the README image with a `1504 x 908` Retina render of the existing
  `752 x 454` interface state.

## Local gates

- Release configuration builds AU, VST3 and Standalone as universal macOS
  binaries.
- All 14 CTest targets pass.
- The editor regression generates the README image at `1504 x 908` and the
  resulting PNG has been visually inspected.
- The documentation generator removes `## Build`, rewrites local links for the
  tagged source tree, and emits the required consolidated legal text.
- macOS and Linux installer scripts pass their shell syntax checks. A complete
  macOS archive was assembled locally and contains none of the superseded root
  documentation entries.
- The installed AU and VST3 bundles report version `0.5.1`, contain both
  `x86_64` and `arm64` slices, and pass strict code-signature verification.
- The installed AU passes `auval -v aufx Dfeq Icss`; the installed VST3 passes
  pluginval at strictness level 10.
- Installed executable SHA-256 digests:
  - AU: `724bed3123c141b32447ada6347258bc75318162bef8b13241d6d80940b815d4`
  - VST3: `633e8add21052ff66f42b8200aff788325f059fa6ae9848114d546ad662ad70a`

## Tagged release gates

The `v0.5.1` release is accepted only after the GitHub workflow passes build,
CTest, pluginval and packaging on macOS universal, Windows x64 and Linux x86_64,
plus `auval` and universal-binary/signature checks on macOS. Published archive
digests must match `SHA256SUMS.txt`.
