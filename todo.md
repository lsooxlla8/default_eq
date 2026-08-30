# TODO

- Make the plug-in editor freely resizable without breaking the interface.
  Preserve the current proportions and spacing at the default size, scale the
  complete UI uniformly at arbitrary window sizes, keep text and controls
  crisp on HiDPI displays, and persist the user's chosen editor size.
- Slightly enlarge each graph band's pointer hit area by only a few pixels, so
  near-miss clicks select the band without making neighbouring bands ambiguous.
- Add automation fuzz testing that rapidly and randomly changes every exposed
  parameter across sample rates and block sizes, and fails on non-finite output,
  discontinuities, latency mismatches, or non-deterministic state.
- Replace Regular Gain's coarse per-filter coverage table with an immediate,
  parameter-derived estimate based on the exact combined EQ response. Keep it
  independent of programme audio and recompute only when relevant parameters
  change.
- Make Smart Gain compare latency-aligned input and output loudness in LUFS.
- Reduce idle and editor-open CPU without changing the visible RTA cadence:
  drive UI updates from dirty/new-frame events, move spectrum smoothing out of
  paint(), cache static drawing, and avoid repainting unchanged content.
- Add an optimised zero-band/clean-instance path, then profile and remove fixed
  per-block work that is unnecessary when EQ, dynamics, drive and analysis are
  inactive.
- Introduce per-band dirty state so parameter snapshots, coefficient setup and
  routing configuration are refreshed only for bands whose relevant state has
  changed.
- Split the monolithic processor, editor, response-curve and band files by
  responsibility as a behaviour-preserving refactor only. Do not combine it
  with DSP, UI, parameter, state or interaction changes; move one subsystem at
  a time and require sample-equivalent DSP checks, all CTest targets and
  plug-in validation to pass after every step.
- Measure both fixed object size and prepared runtime memory per plug-in
  instance, including the oversampling pool, T/S splitters, linear-phase and
  lookahead workspaces; set regression budgets before optimising allocations.
  The initial macOS Release baseline is a 1,102,392-byte processor object and
  approximately 27.3 MB of prepared RSS per instance at 48 kHz / 512 samples.
- Add cross-platform screenshot/layout regression tests at the minimum,
  default and maximum editor sizes, multiple aspect ratios, and 1x/2x DPI.
- Add keyboard shortcuts for **Delete/Backspace** to delete the selected bands
  and **Cmd/Ctrl-A** to select every present band.
- Make right-click reset Q to **0.75** for resonant Low Cut and High Cut filters,
  while preserving the existing Q reset default for every other filter type.
- Remove the configure-time patch to JUCE popup-menu internals and replace it
  with a project-owned, upstream-compatible submenu-positioning solution.
- Update or explicitly archive stale pre-0.3 documentation, screenshots,
  product names and verification records so current behaviour is unambiguous.
- Add a host-parameter regression test that locks every parameter's stable ID,
  display name, range, default, automation flags and host-visible index.
- Make **Shift-right-click** on a band reset its dynamic Threshold. This should
  replace the current duplicate placement-reset gesture; placement reset stays
  on **Shift-Cmd-click**.
- Position the band context menu vertically so the L/C/R/M/S/T/S placement row
  opens under the pointer, instead of anchoring a menu corner at the click.
  Preserve automatic left/right screen-edge flipping, the hover saturation
  submenu, and a non-scrolling main menu.
