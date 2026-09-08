# default_eq 0.5.3 verification

## Scope

- Restored the band context menu and saturation-mode menu as screen-level windows.
- Kept the main menu's L route cell immediately to the right of the click.
- Made saturation-menu direction follow the live hover position instead of band or
  parent-menu placement.
- Removed popup shadows and excess saturation-menu padding.
- Closed the saturation menu after leaving its trigger unless the pointer enters
  the saturation menu itself.

## Local gates

- AU, VST3 and Standalone built with version 0.5.3 for macOS.
- All 14 CTest targets passed, including context-menu placement and hover-lifetime
  regressions.
- Installed AU and VST3 bundles contain arm64 and x86_64 slices, match their local
  build bundles, and pass strict code-signature verification.
- The installed Audio Unit passed `auval -v aufx Dfeq Icss`.
- The refreshed interface screenshot is a visually checked `1504 x 908` Retina PNG.
- Installed executable SHA-256 digests:
  - AU: `b8859b760d4a5300a4f8d1dd7fb22a7f0df40941954649467d34cc70858b4e3f`
  - VST3: `dfb905b81efccfd4c9c156c401ddd16fbdacb6a4560db3edb34e5199263f8cca`

## Tagged release gates

The `v0.5.3` release workflow must pass build, CTest, pluginval and packaging on
macOS universal, Windows x64 and Linux x86_64, plus auval and architecture/signature
checks on macOS. Published archives are verified against `SHA256SUMS.txt` after
upload. These release checks are separate from the local results above.
