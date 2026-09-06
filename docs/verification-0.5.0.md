# Verification record — 0.5.0

This record covers the local release candidate and the tagged cross-platform
release gates for the 0.5.0 interface migration. It does not replace the earlier
0.4.0 evidence.

## Verified locally on 2026-09-06

- Added the RTA-sized JUCE settings/diagnostics panel and offscreen snapshot
  coverage for its exact bounds and composition.
- Added four full-RGB palette fields (White Background/Ink and Black
  Background/Ink), an in-panel HSV editor, per-field right-click reset and
  persistent local storage. Regression coverage confirms custom colours reach
  both the component look-and-feel and the RTA renderer.
- Expanded Spectral Statistics with four miniature meters/markers and ten tonal
  balance bands, and replaced the left-packed shortcut copy with an airy 4x2
  summary plus a dedicated 3x2 complete interaction map. Both use the embedded
  JetBrains Mono family at a 9 px minimum and follow the cross-layer grid.
- Made Auto the factory theme and added deterministic checks for its boundaries
  at 08:00 and 20:00,
  AUTO RTA range open/expand/no-live-shrink behaviour, fixed manual RTA
  ranges, and 4096/8192/16384 FFT selection.
- Closing the RTA-sized settings panel now recomputes the AUTO RTA range from
  the current band gains, exactly like reopening the editor, and rearms
  subsequent near-edge expansion. Closing unrelated overlays cannot reset it.
- Restored dynamic gain-grid labels and repeatable near-edge Auto expansion from
  ±6 through ±36 dB. RTA labels now bypass the 1x raster cache and render
  directly from the embedded JetBrains Mono typeface on integer
  baselines instead of through a transformed glyph arrangement.
- Kept 16384-point analyzer storage outside the fixed processor object and
  passed the established processor-object and prepared-instance memory budgets.
- Built AU, VST3 and Standalone, passed all 14 local CTest targets, and passed
  pluginval 1.0.4 at strictness 10 against the newly built VST3.
- Installed only the final AU and VST3 candidates in the standard user plug-in
  folders after visual approval; no LV2 or Standalone copy was installed.

## Verified locally on 2026-09-05

- Configured an arm64 Release tree with tests enabled. The build stayed under
  `build-v050` until the explicit final AU/VST3 installation step.
- Built the AU, VST3 and Standalone targets successfully. Their bundle short
  version and bundle version are both `0.5.0`.
- Passed all 14 CTest targets, including host-parameter regression, automation
  fuzzing, memory regression, DSP integration, DSP equivalence and editor layout
  regression.
- Passed pluginval `1.0.4` at strictness level 10, including editor, editor
  automation, processing, state restoration, parameter thread-safety and fuzz
  tests.
- Replaced the remaining native JUCE context popup with the prototype's
  project-owned shell overlay: exact 288 px main box, 6 px padding, 30 px
  action rows, 10-column filter grid, 7-column route grid, 178 px saturation
  submenu, active/hover inversion, shell clamping and edge-aware left/right
  placement. Filter and header selectors use the same project-owned overlay
  path and exact prototype row geometry.
- Corrected the UTF-8 multiplication sign in the Oversampling popup, prevented
  pointer clicks from leaving JUCE keyboard-focus outlines on selectors and
  buttons, and kept a band-originated right-click from being treated as an
  outside click by the editor's parent mouse listener. Repeated internal
  relayouts no longer dismiss an open menu.
- Set the visible band-node body to the requested 28 graph units,
  expanded its selection geometry with it, moved the route caption below the
  body, and kept the number at a 15 graph-unit font, which renders at
  approximately 9 px in the 752 x 454 editor.
- Matched the prototype raster for the four workspace action buttons (39 px),
  vertical fader frames (24 px) and Threshold line (34 px). Threshold now uses
  three discrete integer-pixel rows (paper, ink, paper), and all vertical-fader
  fills use integer pixel edges with no antialiased shadow.
  Embedded JetBrains Mono glyphs retain their native proportions; CSS-style
  tracking is applied through the font's advance metrics rather than by
  stretching or repositioning individual glyphs. Context-menu labels are
  uppercase throughout.
- Decoupled the dynamic Range guide from the live gain-reduction curve. The
  dashed guide is calculated from an unmodulated reference response and remains
  stationary while the selected band reacts. Its draggable square uses that
  same response calculation, including Amount and the other static bands, and
  dragging inverts the displayed target response so the handle remains aligned.
- Installed only AU and VST3 to the standard user plug-in folders. Both installed
  executables match the validated build artifacts byte-for-byte and pass strict
  code-signature verification. The replaced 0.4.0 bundles were retained in the
  user's Trash as recoverable backups.
- Final installed executable SHA-256 values are
  `6244edc03e7a714a20f9234ebc82f9ddda1fe452ba48ec4bfda892f8faf35316`
  for AU and
  `dbfda0d09cec58b2e81735d050d721574846dc48c9e43e950049146d874cddb6`
  for VST3. They match the signed release-candidate build artifacts
  byte-for-byte. The immediately preceding installed candidate is recoverable
  from `~/.Trash/default_eq.component.v0.5.0-pre-release-final` and
  `~/.Trash/default_eq.vst3.v0.5.0-pre-release-final`.
- Passed `auval -v aufx Dfeq Icss` against the installed AU; the validator reports
  component version `0.5.0` and `AU VALIDATION SUCCEEDED`.
- Kept the established DSP-equivalence budget unchanged while migrating the
  editor. The production analyzer, response calculation and graph interaction
  paths remain in use.
- Exercised minimum, default, 2x and 3x proportional editor layouts, with
  offscreen structural renders at 1x and 2x. The editor is constrained to the
  prototype's fixed `752:454` aspect ratio; the default graph remains exactly
  `4,68,744,254` in that design frame.
- Exercised dark and light paper/ink renders for zero, single and multi-selection
  states. Mixed numeric values show `MULTI`, mixed binary buttons show `MIXED`,
  shared values remain concrete, and mixed Threshold has no `dB` suffix.
- Exercised grouped UI changes: frequency drag preserves ratios, Gain drag
  applies a common delta, and absolute selector/binary edits update every
  selected band.
- Captured the rebuilt JUCE editor at the prototype's exact `752 x 454` state
  with six reference bands and the same two-band selection. The reference
  screenshot is generated by the editor regression itself; production response
  and analyzer math remain deliberately unchanged under the immutable migration
  boundary.
- Linted the AU, VST3 and Standalone property lists successfully.

## Tagged release gates

- Representative DAW interaction QA was completed against the installed 0.5.0
  candidate before publication.
- The `v0.5.0` GitHub workflow must pass its macOS universal, Windows x64 and
  Linux x86_64 builds, validation, packaging and checksum jobs before the release
  is treated as published evidence.
