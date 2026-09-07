# default_eq 0.5.2 verification

## Scope

- Centered the band parameter row.
- Added RTA Ceiling and shared averaging for all Spectral Statistics.
- Preserved Tonal Balance's relative scale, independent of RTA slope and range.
- Made Shift-created Tilt and resonant Cuts draggable immediately, with creation
  and movement in one Undo step. Marquee selection remains on right-drag.

## Local gates

- AU, VST3 and Standalone built with version 0.5.2 for macOS.
- All 14 CTest targets passed, including new analyzer and Shift-gesture regressions.
- The settings panel was rendered offscreen and visually inspected.
- Release documentation generation and macOS/Linux installer syntax checks passed.
- Installed AU and VST3 bundles report 0.5.2, contain arm64 and x86_64 slices,
  match their local build bundles, and pass strict code-signature verification.
- The installed Audio Unit passed `auval -v aufx Dfeq Icss`.
- Installed executable SHA-256 digests:
  - AU: `b42100b3008dad60ad0a0550d0352fd9786ea03c5e99ba6745124f87c28d78c8`
  - VST3: `bfc06d590150bb53f4c112dde995b9724a92a45d6ffe28d82d3d7772cf4d2cb6`

## Tagged release gates

The `v0.5.2` release workflow must pass build, CTest, pluginval and packaging on
macOS universal, Windows x64 and Linux x86_64, plus auval and architecture/signature
checks on macOS. Published archives are verified against `SHA256SUMS.txt` after
upload. These release checks are separate from the local results above.
