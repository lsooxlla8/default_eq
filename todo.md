# TODO

## 0.5.0 — replace the current interface with the new default_eq design

Source of truth:

- [`docs/prototypes/default_eq/index.html`](docs/prototypes/default_eq/index.html)
  for the approved visual composition, geometry, typography, and styling reference;
- [`docs/plugin-family-design-system.md`](docs/plugin-family-design-system.md)
  for the checkout-local implementation snapshot of the family rules;
- `default_plugins/docs/default-series/visual-design-system.md` in the shared site
  repository is the canonical family design document. Keep the local snapshot in
  sync while the plug-in consumes it as a repository-local implementation reference.

### Immutable migration boundary

The redesign is an interface replacement only. The 0.4.0 production plug-in is
the behaviour baseline. The migration must not change:

- DSP code, coefficients, signal flow, oversampling, latency, CPU behaviour,
  channel routing, or audio output;
- RTA/analyzer acquisition, FFT, smoothing, decay, normalization, calibration,
  response calculations, update cadence, or data flow;
- parameter IDs, ordering, ranges, defaults, normalization, text conversion,
  host automation, preset/project state, or migration;
- Undo/Redo and host gesture boundaries;
- keyboard shortcuts, modifiers, mouse buttons, drag axes, wheel actions,
  double-click actions, context-menu actions, hover behaviour, selection,
  multi-selection, or grouped editing;
- accessibility and keyboard-focus behaviour.

The HTML prototype does not replace production DSP, RTA, parameter, or interaction
logic. It defines where existing functionality is placed and how it is drawn. If
prototype behaviour conflicts with 0.4.0 production behaviour, preserve production
behaviour unless a separate change is explicitly approved.

### Separately approved 0.5.0 extensions

- The wordmark opens a JUCE settings/diagnostics panel occupying exactly the RTA
  rectangle. It exposes Theme (Auto/White/Black), Gain Range
  (Auto/±6/±12/±24/±36), RTA Floor, Average, FFT Size
  (4096/8192/16384), RTA Slope and Show Hover Tooltip. Existing RTA Range,
  Speed and Peak Hold behaviour stays internal and is not exposed.
- Auto is the factory theme and follows local system time: White from 08:00
  through 19:59 and Black from 20:00 through 07:59. Explicit White and Black
  remain fixed.
- AUTO RTA range starts at the smallest ±6/±12/±24/±36 range containing all
  restored band gains, expands one step near an RTA edge, rearms after the
  pointer leaves the edge zone, and does not shrink until the editor is closed
  and reopened or the RTA-sized settings panel is closed.
- The panel's bottom row reports spectral centroid, true output peak/RMS crest
  factor, average spectral tilt, L/R phase correlation and a ten-band tonal
  balance distribution. These diagnostics do not alter or enter the audio path.
- White and Black themes each expose independent Background and Ink colours.
  The factory palette remains `#F6F6F6`/`#050505`; every field accepts the full
  RGB colour space, persists locally, and resets independently by right-click.
- Spectral Statistics gives centroid, crest, tilt and correlation a compact
  graphical scale. The shortcut reference uses an airy 4x2 summary across the
  full panel width; clicking it opens a dedicated 3x2 semantic grid containing
  the complete implemented interaction map at the same 9 px minimum type size.

### Foundation

- Embed JetBrains Mono v2.304 for every interface string: upstream Medium 500 and
  ExtraBold 800 WOFF2 in HTML references, matching static TTF data in JUCE, no
  synthetic weights or ligatures, and no system-font dependency. Retain the OFL
  license and third-party provenance.
- Translate the prototype's paper/ink palette, typography, spacing, borders,
  square geometry, and active/disabled states into shared JUCE look-and-feel
  primitives rather than one-off painting code.
- Implement reusable full-cell selectors, draggable value cells, binary action
  cells, vertical parameter controls, threshold meter-fader, filter icons,
  tooltips, and popup-menu rows from the design-system specification.
- Replace the editor layout with the prototype's `752 x 454` reference frame:
  header, graph, value strip, and contextual workspace. Resize only at the fixed
  `752:454` aspect ratio, preserving persisted size and HiDPI crispness.

### Header

- Rebuild the header with the `default_eq` wordmark/theme action, Oversampling,
  Phase, Amount, Shift, Auto Gain, and Power cells in the prototype order.
- Use MINIMUM, LINEAR ECO, LINEAR MED, and LINEAR MAX as the visible Phase labels.
- Keep secondary labels at the shared 0.72 contrast, use a 9 px minimum ordinary
  label size at 1x, and give Auto Gain the same left inset as peer header cells.
- Preserve every current value range, default, text conversion, gesture,
  attachment, Undo/Redo transaction, and host-automation notification.
- Match full-cell hit targets, picker state, drag behaviour, theme inversion,
  keyboard focus, and disabled states.

### Graph and value strip

- Reuse the existing production RTA and response component. Reposition, frame, and
  restyle its existing grid, input/output spectra, combined response, individual
  band curves, dynamic range, nodes, selection outlines, and hover card without
  replacing or modifying its analyzer pipeline, response math, data transport, or
  cadence.
- Keep the +12 and -12 scale labels optically equidistant from the frame without
  moving their grid lines, the response mapping, or band-node coordinates.
- Preserve all 0.4.0 graph interactions: band creation zones, dragging,
  multi-selection and marquee, 20 px hit radius, momentary solo, bypass,
  deletion/reset gestures, and the current wheel-modifier mapping and direction.
- Replace the lower graph controls with the prototype value strip: Adaptive Q,
  filter type, Frequency, Gain, Q, Slope, and Out. Support direct dragging,
  precise text entry, mixed multi-selection values, and valid per-filter states.
- Use the production filter icons. The closed selector and popup row use the same
  26 x 16 design px icon, 9 px text, 7 px icon-to-text gap, and 10 px effective
  left inset. Display pass filters as RES LP, RES HP, LOW PASS, and HIGH PASS
  according to DSP meaning.

### Contextual workspace

- Rebuild the selected-band workspace with ON/SOLO, L/R-M/S-T/S routing and
  placement, saturation type and Character controls, dynamics mode/sidechain,
  threshold metering, Range, Ratio, and Speed.
- Make the workspace respond correctly to zero, one, and multiple selected
  bands; retain the primary-band concept and group-edit semantics.
- Preserve every current conditional state: filter-specific Q/Slope behaviour,
  saturation-character labels and ranges, dynamics availability, sidechain
  availability, routing names, and meter behaviour.
- Keep ordinary workspace labels and values at least 9 design px at 1x. Use HYST
  and ODD/EVEN for compact Character labels, keep THRESHOLD and SUSTAIN unclipped,
  and always display threshold values with dB except for the MULTI mixed state.

### Menus and overlays

- Port filter, routing, saturation, header, and field selectors to the new
  project-owned menu visuals while retaining the proven placement behaviour.
- Keep closed selector values and popup rows at the same apparent 9 px type scale;
  the closed filter icon must remain as legible as the menu icon.
- Keep the band context menu vertically aligned so the placement row is under
  the pointer, keep the saturation submenu beside the pointer, flip at screen
  edges, and never make the main menu scroll.
- Ensure menus, hover cards, editors, and tooltips remain inside the usable
  editor/display bounds at every supported size and scale.

### Migration and cleanup

- Replace one visual region at a time around the existing parameter, state,
  analyzer, and interaction wiring: shared primitives, header, graph/value strip,
  workspace, then menus. Do not combine a visual migration step with DSP, RTA,
  parameter, state, or gesture refactoring.
- After each region, run the complete CTest suite, DSP equivalence comparison,
  AU validation, strict VST3 plugin validation, and focused interaction/layout
  regressions. A visual-only step must not move the established DSP residual.
- Add reference screenshots and layout assertions at minimum, default, 2x, and
  3x editor sizes while preserving the fixed `752:454` aspect ratio at 1x/2x
  render scale. Cover both paper/ink themes and zero/single/multi-selection
  states.
- Add parity coverage for every existing keyboard shortcut and mouse/trackpad
  action, including modifiers, wheel directions, double-click actions, menu
  contents, hover behaviour, hit targets, selection, and group edits.
- Capture before/after analyzer and DSP baselines using identical input and state.
  The UI migration must not move the established DSP residual, analyzer values,
  response curves, latency, or parameter serialization.
- Remove superseded 0.4.0 editor components and obsolete active screenshots only
  after feature, interaction, accessibility, and automation parity is verified.

### 0.5.0 completion gate

- The JUCE editor matches the approved prototype and design-system contract.
- Existing projects reopen with identical state and audio behaviour.
- The production RTA implementation and every existing shortcut and mouse action
  remain behaviourally identical; only presentation and placement change.
- All automated tests, DSP equivalence budgets, AU validation, strict pluginval,
  cross-platform builds, interaction parity checks, and screenshot/layout
  regressions pass.
- README screenshots, interaction documentation, changelog, verification record,
  and release packages describe the new interface rather than the 0.4.0 UI.
