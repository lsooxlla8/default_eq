# TODO

## 0.5.0 — replace the current interface with the new default_eq design

Source of truth:

- [`docs/prototypes/default_eq/index.html`](docs/prototypes/default_eq/index.html)
  for the interactive layout and behaviour reference;
- [`docs/plugin-family-design-system.md`](docs/plugin-family-design-system.md)
  for visual tokens, geometry, component states, scaling, and QA rules.

The redesign is an interface replacement only. Keep the 0.4.0 DSP, parameter
IDs and ordering, automation behaviour, state migration, latency, channel
routing, and audio output unchanged.

### Foundation

- Translate the prototype's paper/ink palette, typography, spacing, borders,
  square geometry, and active/disabled states into shared JUCE look-and-feel
  primitives rather than one-off painting code.
- Implement reusable full-cell selectors, draggable value cells, binary action
  cells, vertical parameter controls, threshold meter-fader, filter icons,
  tooltips, and popup-menu rows from the design-system specification.
- Replace the editor layout with the prototype's `800 x 464` reference frame:
  header, graph, value strip, and contextual workspace. Preserve free resizing,
  persisted window size, HiDPI crispness, and usable arbitrary aspect ratios.

### Header

- Rebuild the header with the `default_eq` wordmark/theme action, Oversampling,
  Phase, Amount, Shift, Auto Gain, and Power cells in the prototype order.
- Preserve every current value range, default, text conversion, gesture,
  attachment, Undo/Redo transaction, and host-automation notification.
- Match full-cell hit targets, picker state, drag behaviour, theme inversion,
  keyboard focus, and disabled states.

### Graph and value strip

- Restyle the analyzer grid, input/output spectra, combined response, individual
  band curves, dynamic range, nodes, selection outlines, and hover card to the
  prototype without changing response calculations or analyzer cadence.
- Preserve all 0.4.0 graph interactions: band creation zones, dragging,
  multi-selection and marquee, 20 px hit radius, momentary solo, bypass,
  deletion/reset gestures, and the current wheel-modifier mapping and direction.
- Replace the lower graph controls with the prototype value strip: Adaptive Q,
  filter type, Frequency, Gain, Q, Slope, and Output. Support direct dragging,
  precise text entry, mixed multi-selection values, and valid per-filter states.

### Contextual workspace

- Rebuild the selected-band workspace with ON/SOLO, L/R-M/S-T/S routing and
  placement, saturation type and Character controls, dynamics mode/sidechain,
  threshold metering, Range, Ratio, and Speed.
- Make the workspace respond correctly to zero, one, and multiple selected
  bands; retain the primary-band concept and group-edit semantics.
- Preserve every current conditional state: filter-specific Q/Slope behaviour,
  saturation-character labels and ranges, dynamics availability, sidechain
  availability, routing names, and meter behaviour.

### Menus and overlays

- Port filter, routing, saturation, header, and field selectors to the new
  project-owned menu visuals while retaining the proven placement behaviour.
- Keep the band context menu vertically aligned so the placement row is under
  the pointer, keep the saturation submenu beside the pointer, flip at screen
  edges, and never make the main menu scroll.
- Ensure menus, hover cards, editors, and tooltips remain inside the usable
  editor/display bounds at every supported size and scale.

### Migration and cleanup

- Replace one visual region at a time behind the existing parameter/state
  wiring: shared primitives, header, graph/value strip, workspace, then menus.
- After each region, run the complete CTest suite, DSP equivalence comparison,
  AU validation, strict VST3 plugin validation, and focused interaction/layout
  regressions. A visual-only step must not move the established DSP residual.
- Add reference screenshots and layout assertions at minimum, default, and
  large editor sizes, representative wide/tall aspect ratios, and 1x/2x scale.
  Cover both paper/ink themes and zero/single/multi-selection states.
- Remove superseded 0.4.0 editor components and obsolete active screenshots only
  after feature, interaction, accessibility, and automation parity is verified.

### 0.5.0 completion gate

- The JUCE editor matches the approved prototype and design-system contract.
- Existing projects reopen with identical state and audio behaviour.
- All automated tests, DSP equivalence budgets, AU validation, strict pluginval,
  cross-platform builds, and screenshot/layout regressions pass.
- README screenshots, interaction documentation, changelog, verification record,
  and release packages describe the new interface rather than the 0.4.0 UI.
