(() => {
  "use strict";

  const NS = "http://www.w3.org/2000/svg";
  const W = 1200;
  const H = 420;
  const MIN_FREQ = 20;
  const MAX_FREQ = 20000;
  const DISPLAY_DB = 12;

  const FILTER_TYPES = [
    "RES LOW CUT",
    "RES HIGH CUT",
    "NOTCH",
    "TILT",
    "BAND PASS",
    "BELL",
    "LOW SHELF",
    "HIGH SHELF",
    "LOW CUT",
    "HIGH CUT",
  ];
  const SATURATION_TYPES = [
    "SOFT CLIP",
    "DIODE",
    "TRIODE",
    "TRANSISTOR",
    "TAPE",
    "ODD / EVEN",
    "PHASE DISTORTION",
    "SINE EROSION",
  ];
  const CHARACTER_NAMES = [
    "CURVE",
    "TOPOLOGY",
    "BIAS",
    "GATE",
    "HYSTERESIS",
    "ODD / EVEN",
    "TONE",
    "FREQUENCY",
  ];
  const PLACEMENT_MODES = ["L/R", "M/S", "T/S"];
  const DISCRETE_SLOPES = [6, 12, 24, 36, 48, 72, 96];
  // SVG transcription of paintFilterIcon(), aligned by the unaffected-response baseline.
  const FILTER_ICON_PATHS = [
    "M1 9H9C12 9 13 2 16 3C20 4 23 14 27 17",
    "M1 17C5 14 8 4 12 3C15 2 16 9 19 9H27",
    "M1 9H11L14 17L17 9H27",
    "M1 14L27 4",
    "M1 15C7 15 8 3 14 3C20 3 21 15 27 15",
    "M1 9H8A6 4 0 1 0 20 9A6 4 0 1 0 8 9M20 9H27",
    "M1 4H9C14 4 14 9 19 9H27",
    "M1 9H9C14 9 14 4 19 4H27",
    "M1 9H12C19 9 21 8 27 17",
    "M1 17C7 8 9 9 16 9H27",
  ];

  const $ = (id) => document.getElementById(id);
  const plugin = $("plugin");
  const svg = $("eqGraph");
  const graphPanel = $("graphPanel");

  const globalState = {
    outputGain: 0,
    amount: 1,
    shift: 0,
    adaptiveQ: false,
    oversampling: 0,
    linearPhase: false,
    linearQuality: 2,
    autoGainMode: 1,
    pluginEnabled: true,
    transientSplitStrength: 100,
    transientSplitBalance: 0,
    transientSplitHold: 50,
    transientSplitSmooth: 50,
  };

  const makeBand = (overrides = {}) => ({
    present: false,
    on: false,
    type: 5,
    slope: 12,
    placementMode: 0,
    placement: 0,
    freq: 1000,
    q: 1,
    gain: 0,
    drive: 0,
    driveCharacter: 0,
    driveSecondary: 0,
    satMode: 0,
    dynMode: 0,
    scSource: 0,
    dynLookahead: 0,
    dynThresh: 0,
    dynRange: 6,
    dynRatio: 4,
    dynSpeed: 75,
    ...overrides,
  });

  const bands = [
    makeBand({ present: true, on: true, type: 1, freq: 38, q: 0.75, placement: -100 }),
    makeBand({ present: true, on: true, type: 6, freq: 118, gain: 2.4, q: 0.82 }),
    makeBand({ present: true, on: true, type: 5, freq: 520, gain: -3.2, q: 1.45, dynThresh: -24, dynRange: 4.5, drive: 2.2 }),
    makeBand({ present: true, on: true, type: 5, freq: 2100, gain: 1.8, q: 1.1, placementMode: 2, placement: -100, satMode: 4, drive: 5.5, driveCharacter: 0.55, driveSecondary: 0.5, dynThresh: -18, dynRange: 3.5 }),
    makeBand({ present: true, on: true, type: 7, freq: 7800, gain: 1.2, q: 0.9, placementMode: 1, placement: 100 }),
    makeBand({ present: true, on: true, type: 0, freq: 16500, q: 0.75 }),
    makeBand(),
    makeBand(),
  ];

  let selection = new Set([2, 3]);
  let primaryBand = 3;
  let soloBand = -1;
  let drag = null;
  let marquee = null;
  let hoveredBand = -1;
  let groupEdit = null;
  let activePickerField = null;

  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
  }

  function clean(value, digits = 1) {
    const next = Math.abs(value) < 0.5 * 10 ** -digits ? 0 : value;
    return next.toFixed(digits);
  }

  function svgEl(name, attrs = {}, text = "") {
    const node = document.createElementNS(NS, name);
    Object.entries(attrs).forEach(([key, value]) => node.setAttribute(key, String(value)));
    if (text) node.textContent = text;
    return node;
  }

  function filterIcon(type) {
    const icon = svgEl("svg", { viewBox: "0 0 28 18", "aria-hidden": "true" });
    icon.append(svgEl("path", { d: FILTER_ICON_PATHS[clamp(Number(type), 0, 9)] }));
    return icon;
  }

  function fillSelect(select, values) {
    select.replaceChildren();
    values.forEach((label, index) => {
      const option = document.createElement("option");
      option.value = String(index);
      option.textContent = label;
      select.append(option);
    });
  }

  function hideSelectMenu() {
    const menu = $("selectMenu");
    menu.hidden = true;
    menu.replaceChildren();
    activePickerField?.classList.remove("is-picker-open");
    activePickerField = null;
  }

  function openSelectMenu(field, select) {
    hideContextMenu();
    hideSelectMenu();

    const menu = $("selectMenu");
    [...select.options].forEach((option, index) => {
      const button = document.createElement("button");
      button.type = "button";
      button.classList.toggle("is-active", select.selectedIndex === index);

      if (select.id === "filterType") {
        const icon = document.createElement("span");
        icon.className = "menu-filter-icon";
        icon.append(filterIcon(index));
        button.append(icon);
      }

      const label = document.createElement("span");
      label.textContent = option.textContent;
      button.append(label);
      button.addEventListener("click", (event) => {
        event.stopPropagation();
        select.value = option.value;
        select.dispatchEvent(new Event("change", { bubbles: true }));
        hideSelectMenu();
        select.focus({ preventScroll: true });
      });
      menu.append(button);
    });

    activePickerField = field;
    field.classList.add("is-picker-open");
    menu.hidden = false;

    const shellRect = plugin.getBoundingClientRect();
    const fieldRect = field.getBoundingClientRect();
    menu.style.width = `${fieldRect.width}px`;
    const left = clamp(fieldRect.left - shellRect.left, 4, shellRect.width - menu.offsetWidth - 4);
    let top = fieldRect.bottom - shellRect.top;
    if (top + menu.offsetHeight > shellRect.height - 4) {
      top = fieldRect.top - shellRect.top - menu.offsetHeight;
    }
    menu.style.left = `${left}px`;
    menu.style.top = `${Math.max(4, top)}px`;
  }

  function bindWholeFieldPicker(field, select) {
    field.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || select.disabled) return;
      event.preventDefault();
      event.stopPropagation();
      select.focus({ preventScroll: true });
      openSelectMenu(field, select);
    });
    field.addEventListener("click", (event) => event.preventDefault());
    select.addEventListener("keydown", (event) => {
      if (!["Enter", " ", "ArrowDown"].includes(event.key) || select.disabled) return;
      event.preventDefault();
      openSelectMenu(field, select);
    });
  }

  function freqToX(freq) {
    return ((Math.log10(freq) - Math.log10(MIN_FREQ)) / (Math.log10(MAX_FREQ) - Math.log10(MIN_FREQ))) * W;
  }

  function xToFreq(x) {
    return 10 ** (Math.log10(MIN_FREQ) + (clamp(x, 0, W) / W) * (Math.log10(MAX_FREQ) - Math.log10(MIN_FREQ)));
  }

  function dbToY(db) {
    return (H * 0.5) * (1 - clamp(db, -DISPLAY_DB, DISPLAY_DB) / DISPLAY_DB);
  }

  function yToDb(y) {
    return clamp(DISPLAY_DB * (1 - (2 * y) / H), -36, 36);
  }

  function displayedFreq(band) {
    return clamp(band.freq * 2 ** (globalState.shift / 12), MIN_FREQ, MAX_FREQ);
  }

  function graphPoint(event) {
    const rect = svg.getBoundingClientRect();
    return {
      x: ((event.clientX - rect.left) / rect.width) * W,
      y: ((event.clientY - rect.top) / rect.height) * H,
    };
  }

  function selectedBands() {
    return [...selection].filter((index) => bands[index].present);
  }

  function primary() {
    if (primaryBand >= 0 && bands[primaryBand]?.present) return bands[primaryBand];
    const first = selectedBands()[0];
    primaryBand = first ?? -1;
    return primaryBand >= 0 ? bands[primaryBand] : null;
  }

  function valuesFor(key) {
    return selectedBands().map((index) => bands[index][key]);
  }

  function isMixed(key) {
    const values = valuesFor(key);
    return values.length > 1 && values.some((value) => value !== values[0]);
  }

  function forSelection(callback) {
    selectedBands().forEach((index) => callback(bands[index], index));
  }

  function routeLabel(band) {
    if (Math.abs(band.placement) < 1) return PLACEMENT_MODES[band.placementMode].replace("/", "");
    if (band.placementMode === 0) return band.placement < 0 ? "L" : "R";
    if (band.placementMode === 1) return band.placement < 0 ? "M" : "S";
    return band.placement < 0 ? "T" : "S";
  }

  function placementText(band) {
    if (Math.abs(band.placement) < 1) return band.placementMode === 2 ? "SUM" : "CENTER";
    if (band.placementMode === 0) return band.placement < 0 ? "LEFT" : "RIGHT";
    if (band.placementMode === 1) return band.placement < 0 ? "MID" : "SIDE";
    return band.placement < 0 ? "TRNSNT" : "SUSTAIN";
  }

  function hoverPlacementText(band) {
    const mode = PLACEMENT_MODES[band.placementMode];
    if (Math.abs(band.placement) < 0.05) return `${mode} CENTER`;
    const side = band.placementMode === 0
      ? (band.placement < 0 ? "L" : "R")
      : band.placementMode === 1
        ? (band.placement < 0 ? "M" : "S")
        : (band.placement < 0 ? "T" : "S");
    return `${mode} ${side} ${Math.round(Math.abs(band.placement))}%`;
  }

  function qReset(type) {
    return type === 0 || type === 1 ? 0.75 : 1;
  }

  function isClassicCut(type) {
    return type === 8 || type === 9;
  }

  function usesQVertical(type) {
    return [0, 1, 2, 4, 8, 9].includes(type);
  }

  function gainBearing(type) {
    return [3, 5, 6, 7].includes(type);
  }

  function formatCharacter(band) {
    if (band.satMode === 7) {
      const unit = clamp(band.driveCharacter, 0, 1);
      const hz = unit <= 0.5 ? 1000 * (2 * unit) ** 2 : 1000 * 10 ** (2 * unit - 1);
      return hz >= 1000 ? `${clean(hz / 1000, 2)} kHz` : `${Math.round(hz)} Hz`;
    }
    const bipolar = [2, 3, 5].includes(band.satMode);
    const value = Math.round((bipolar ? band.driveCharacter : Math.max(0, band.driveCharacter)) * 100);
    return `${value === 0 ? "00" : value}%`;
  }

  function bandResponse(band, frequency, gainOffset = 0) {
    if (!band.present || !band.on) return 0;
    const center = displayedFreq(band);
    const octave = Math.log2(frequency / center);
    const q = Math.max(0.1, band.q * (globalState.adaptiveQ ? 1 + Math.abs(band.gain) * 0.12 : 1));
    const width = Math.max(0.08, 1.25 / q);
    const gaussian = Math.exp(-0.5 * (octave / width) ** 2);
    const amount = globalState.amount;
    const gain = (band.gain + gainOffset) * amount;

    switch (band.type) {
      case 0: {
        const roll = -Math.max(0, octave) * Math.min(48, band.slope) * Math.max(0, amount);
        const resonance = 5.2 * q * Math.exp(-0.5 * (octave / 0.13) ** 2);
        return clamp(roll + resonance, -36, 12);
      }
      case 1: {
        const roll = Math.min(0, octave) * Math.min(48, band.slope) * Math.max(0, amount);
        const resonance = 5.2 * q * Math.exp(-0.5 * (octave / 0.13) ** 2);
        return clamp(roll + resonance, -36, 12);
      }
      case 2:
        return -Math.min(30, 9 + q * 2.5) * gaussian * Math.max(0, amount);
      case 3:
        return gain * Math.tanh(-octave * 0.8);
      case 4:
        return -Math.min(30, Math.abs(octave) * 14 * q) * Math.max(0, amount);
      case 5:
        return gain * gaussian;
      case 6:
        return gain * (1 - 1 / (1 + Math.exp(-octave * 5)));
      case 7:
        return gain * (1 / (1 + Math.exp(-octave * 5)));
      case 8:
        return clamp(-Math.max(0, octave) * band.slope * Math.max(0, amount), -36, 0);
      case 9:
        return clamp(Math.min(0, octave) * band.slope * Math.max(0, amount), -36, 0);
      default:
        return 0;
    }
  }

  function responsePath(extraForBand = -1, extraGain = 0) {
    const points = [];
    for (let i = 0; i <= 320; i += 1) {
      const frequency = xToFreq((i / 320) * W);
      let total = 0;
      bands.forEach((band, index) => {
        total += bandResponse(band, frequency, index === extraForBand ? extraGain : 0);
      });
      const x = (i / 320) * W;
      const y = dbToY(total);
      points.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)},${y.toFixed(2)}`);
    }
    return points.join(" ");
  }

  function individualPath(band, gainOffset = 0) {
    const points = [];
    for (let i = 0; i <= 180; i += 1) {
      const x = (i / 180) * W;
      const y = dbToY(bandResponse(band, xToFreq(x), gainOffset));
      points.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)},${y.toFixed(2)}`);
    }
    return points.join(" ");
  }

  function drawGrid() {
    const layer = $("gridLayer");
    layer.replaceChildren();
    [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000].forEach((frequency) => {
      const x = freqToX(frequency);
      layer.append(svgEl("line", { x1: x, y1: 0, x2: x, y2: H, class: "grid-line" }));
      if ([100, 1000, 10000].includes(frequency)) {
        layer.append(svgEl("text", { x, y: H - 12, class: "grid-label", "text-anchor": "middle" }, frequency >= 1000 ? `${frequency / 1000}k` : String(frequency)));
      }
    });
    [-12, -6, 0, 6, 12].forEach((db) => {
      const y = dbToY(db);
      layer.append(svgEl("line", { x1: 0, y1: y, x2: W, y2: y, class: `grid-line${db === 0 ? " zero" : ""}` }));
      layer.append(svgEl("text", { x: 8, y: clamp(y - 7, 13, H - 8), class: "grid-label" }, db > 0 ? `+${db}` : String(db)));
    });
  }

  function spectrumPath(variant) {
    const points = [];
    for (let i = 0; i <= 180; i += 1) {
      const x = (i / 180) * W;
      const t = i / 180;
      const base = variant === "input" ? 0.66 : 0.61;
      const shape =
        Math.sin(t * 41 + (variant === "input" ? 0.7 : 0)) * 0.055 +
        Math.sin(t * 93) * 0.024 +
        Math.exp(-(((t - 0.24) / 0.17) ** 2)) * 0.12 +
        Math.exp(-(((t - 0.61) / 0.1) ** 2)) * 0.09;
      const y = H * clamp(base - shape + t * 0.13, 0.2, 0.92);
      points.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)},${y.toFixed(2)}`);
    }
    return points.join(" ");
  }

  function drawSpectrum() {
    const layer = $("spectrumLayer");
    layer.replaceChildren();
    ["input", "output"].forEach((variant) => {
      const path = spectrumPath(variant);
      layer.append(svgEl("path", { d: `${path} L${W},${H} L0,${H} Z`, class: `spectrum-fill ${variant}` }));
      layer.append(svgEl("path", { d: path, class: `spectrum-line ${variant}` }));
    });
  }

  function drawCurves() {
    const bandLayer = $("bandCurveLayer");
    const responseLayer = $("responseLayer");
    bandLayer.replaceChildren();
    responseLayer.replaceChildren();

    bands.forEach((band, index) => {
      if (!band.present || !band.on) return;
      const d = individualPath(band);
      bandLayer.append(svgEl("path", { d: `${d} L${W},${dbToY(0)} L0,${dbToY(0)} Z`, class: `band-fill${selection.has(index) ? " is-selected" : ""}` }));
    });

    const current = responsePath();
    const band = primary();
    if (band && band.dynThresh < -0.05 && band.on) {
      const direction = band.dynMode === 1 ? 1 : -1;
      const target = responsePath(primaryBand, gainBearing(band.type) ? band.dynRange * direction : 0);
      responseLayer.append(svgEl("path", { d: `${current} ${target.replace(/^M/, "L")} Z`, class: "dynamic-area" }));
      responseLayer.append(svgEl("path", { d: target, class: "dynamic-curve" }));
    }
    responseLayer.append(svgEl("path", { d: current, class: "response-curve" }));
  }

  function nodePosition(band) {
    return { x: freqToX(displayedFreq(band)), y: dbToY(band.gain) };
  }

  function drawNodes() {
    const layer = $("nodeLayer");
    layer.replaceChildren();

    bands.forEach((band, index) => {
      if (!band.present) return;
      const { x, y } = nodePosition(band);
      const group = svgEl("g", {
        class: `band-node${band.on ? "" : " is-off"}`,
        "data-band": index,
        transform: `translate(${x},${y})`,
      });
      group.append(svgEl("circle", { cx: 0, cy: 0, r: 32, class: "node-hitbox" }));
      if (selection.has(index)) {
        group.append(svgEl("rect", { x: -17, y: -17, width: 34, height: 34, class: "node-selection" }));
        if (index !== primaryBand) group.append(svgEl("rect", { x: -15, y: -15, width: 30, height: 30, class: "node-selection secondary" }));
      }
      group.append(svgEl("rect", { x: -10, y: -10, width: 20, height: 20, class: "node-body" }));
      group.append(svgEl("text", { x: 0, y: 0, class: "node-number" }, String(index + 1)));
      group.append(svgEl("text", { x: 0, y: 24, class: "node-route" }, routeLabel(band)));
      layer.append(group);

      group.addEventListener("pointerdown", (event) => beginNodePointer(event, index));
      group.addEventListener("dblclick", (event) => {
        event.stopPropagation();
        deleteBands([index]);
      });
      group.addEventListener("mouseenter", (event) => showHover(event, index));
      group.addEventListener("mousemove", (event) => showHover(event, index));
      group.addEventListener("mouseleave", hideHover);
      group.addEventListener("wheel", (event) => wheelBand(event, index), { passive: false });
      group.addEventListener("contextmenu", (event) => openContextMenu(event, index));
    });

    const band = primary();
    if (band && band.on && band.dynThresh < -0.05) {
      const { x } = nodePosition(band);
      const direction = band.dynMode === 1 ? 1 : -1;
      const y = dbToY(band.gain + direction * band.dynRange);
      const handle = svgEl("rect", { x: x - 6, y: y - 6, width: 12, height: 12, class: "range-handle" });
      handle.addEventListener("pointerdown", (event) => {
        event.stopPropagation();
        handle.setPointerCapture(event.pointerId);
        drag = { type: "range", pointerId: event.pointerId, band: primaryBand };
      });
      layer.append(handle);
    }
  }

  function renderGraph() {
    drawCurves();
    drawNodes();
    drawMarquee();
  }

  function showHover(event, index) {
    hoveredBand = index;
    const band = bands[index];
    const rect = graphPanel.getBoundingClientRect();
    const card = $("hoverCard");
    card.innerHTML = [
      `DRIVE ${clean(band.drive)}dB&nbsp;&nbsp;&nbsp;CHAR ${formatCharacter(band)}`,
      `THR ${clean(band.dynThresh)}dB&nbsp;&nbsp;&nbsp;${hoverPlacementText(band)}`,
    ].join("<br>");
    card.hidden = false;
    const cardWidth = card.offsetWidth;
    const above = event.clientY - rect.top > 58;
    card.style.left = `${clamp(event.clientX - rect.left - cardWidth * 0.5, 6, rect.width - cardWidth - 6)}px`;
    card.style.top = `${clamp(event.clientY - rect.top + (above ? -57 : 20), 8, rect.height - 45)}px`;
  }

  function hideHover() {
    hoveredBand = -1;
    $("hoverCard").hidden = true;
  }

  function beginNodePointer(event, index) {
    if (event.button !== 0) return;
    event.stopPropagation();
    hideContextMenu();

    if (event.metaKey || event.ctrlKey) {
      bands[index].on = !bands[index].on;
      if (!selection.has(index)) selection = new Set([index]);
      primaryBand = index;
      refresh();
      return;
    }

    if (event.shiftKey) {
      if (selection.has(index)) selection.delete(index);
      else selection.add(index);
      if (selection.has(index)) primaryBand = index;
      else if (primaryBand === index) primaryBand = selectedBands()[0] ?? -1;
      refresh();
      return;
    }

    if (!selection.has(index)) selection = new Set([index]);
    primaryBand = index;
    const point = graphPoint(event);
    const anchor = bands[index];
    drag = {
      type: "nodes",
      pointerId: event.pointerId,
      start: point,
      anchorFreq: displayedFreq(anchor),
      anchorGain: anchor.gain,
      starts: selectedBands().map((bandIndex) => ({
        index: bandIndex,
        freq: bands[bandIndex].freq,
        gain: bands[bandIndex].gain,
        q: bands[bandIndex].q,
      })),
    };
    svg.setPointerCapture(event.pointerId);
    refresh(false);
  }

  function beginGraphPointer(event) {
    if (event.button !== 0) return;
    hideContextMenu();
    const point = graphPoint(event);
    if (event.shiftKey) {
      marquee = { start: point, current: point, pointerId: event.pointerId };
      svg.setPointerCapture(event.pointerId);
      drawMarquee();
      return;
    }
    const index = createBand(point.x, point.y, false);
    if (index >= 0) beginNodePointer(event, index);
  }

  function movePointer(event) {
    if (marquee && marquee.pointerId === event.pointerId) {
      marquee.current = graphPoint(event);
      const left = Math.min(marquee.start.x, marquee.current.x);
      const right = Math.max(marquee.start.x, marquee.current.x);
      const top = Math.min(marquee.start.y, marquee.current.y);
      const bottom = Math.max(marquee.start.y, marquee.current.y);
      selection.clear();
      bands.forEach((band, index) => {
        if (!band.present) return;
        const point = nodePosition(band);
        if (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom) selection.add(index);
      });
      if (!selection.has(primaryBand)) primaryBand = selectedBands()[0] ?? -1;
      renderGraph();
      updateControls();
      return;
    }

    if (!drag || drag.pointerId !== event.pointerId) return;
    const point = graphPoint(event);
    if (drag.type === "range") {
      const band = bands[drag.band];
      band.dynRange = clamp(Math.abs(yToDb(point.y) - band.gain), 0, 24);
      refresh(false);
      return;
    }
    if (drag.type !== "nodes") return;

    const frequency = xToFreq(point.x);
    const ratio = frequency / Math.max(1, drag.anchorFreq);
    const gainDelta = yToDb(point.y) - drag.anchorGain;
    drag.starts.forEach((start) => {
      const band = bands[start.index];
      band.freq = clamp(start.freq * ratio, MIN_FREQ, MAX_FREQ);
      if (usesQVertical(band.type)) {
        band.q = clamp(start.q * 2 ** (-(point.y - drag.start.y) / 80), 0.1, 24);
      } else {
        band.gain = clamp(start.gain + gainDelta, -36, 36);
      }
    });
    refresh(false);
  }

  function endPointer(event) {
    if (marquee?.pointerId === event.pointerId) {
      const distance = Math.abs(marquee.current.x - marquee.start.x) + Math.abs(marquee.current.y - marquee.start.y);
      if (distance <= 3) createBand(marquee.start.x, marquee.start.y, true);
      marquee = null;
    }
    if (drag?.pointerId === event.pointerId) drag = null;
    renderGraph();
    updateControls();
  }

  function drawMarquee() {
    const layer = $("marqueeLayer");
    layer.replaceChildren();
    if (!marquee) return;
    layer.append(svgEl("rect", {
      x: Math.min(marquee.start.x, marquee.current.x),
      y: Math.min(marquee.start.y, marquee.current.y),
      width: Math.abs(marquee.current.x - marquee.start.x),
      height: Math.abs(marquee.current.y - marquee.start.y),
      class: "marquee",
    }));
  }

  function createBand(x, y, shiftType) {
    const index = bands.findIndex((band) => !band.present);
    if (index < 0) return -1;
    const shownFrequency = xToFreq(x);
    const baseFrequency = shownFrequency / 2 ** (globalState.shift / 12);
    const gain = yToDb(y);
    let type = 5;
    if (shiftType) type = gain < 0 && shownFrequency <= 100 ? 1 : gain < 0 && shownFrequency >= 5000 ? 0 : 3;
    else if (shownFrequency <= 100) type = gain >= 0 ? 6 : 9;
    else if (shownFrequency >= 5000) type = gain >= 0 ? 7 : 8;
    bands[index] = makeBand({ present: true, on: true, type, freq: clamp(baseFrequency, MIN_FREQ, MAX_FREQ), gain, q: qReset(type) });
    selection = new Set([index]);
    primaryBand = index;
    refresh();
    return index;
  }

  function deleteBands(indices) {
    indices.forEach((index) => { bands[index] = makeBand(); });
    indices.forEach((index) => selection.delete(index));
    primaryBand = selectedBands()[0] ?? -1;
    refresh();
  }

  function wheelBand(event, index) {
    event.preventDefault();
    if (!selection.has(index)) selection = new Set([index]);
    primaryBand = index;
    const direction = Math.sign(-event.deltaY);
    forSelection((band) => {
      if (event.shiftKey) band.placement = clamp(band.placement + direction * 12, -100, 100);
      else if (event.altKey) band.slope = clamp(band.slope + direction * 12, 3, 96);
      else if (event.metaKey || event.ctrlKey) {
        const minimum = [2, 3, 5].includes(band.satMode) ? -1 : 0;
        band.driveCharacter = clamp(band.driveCharacter + direction * 0.05, minimum, 1);
      } else if (isClassicCut(band.type)) band.slope = clamp(band.slope + direction * 12, 3, 96);
      else band.q = clamp(band.q * 2 ** (direction * 0.12), 0.1, 24);
    });
    refresh();
  }

  function openContextMenu(event, index) {
    event.preventDefault();
    event.stopPropagation();
    hideSelectMenu();
    if (!selection.has(index)) selection = new Set([index]);
    primaryBand = index;
    const menu = $("contextMenu");
    menu.replaceChildren();
    menu.classList.remove("opens-left");

    const addButton = (label, callback) => {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = label;
      button.addEventListener("click", () => { callback(); hideContextMenu(); });
      menu.append(button);
      return button;
    };

    const addSeparator = () => {
      const separator = document.createElement("hr");
      menu.append(separator);
    };

    addButton(`Enable/Disable Band ${index + 1}`, () => {
      bands[index].on = !bands[index].on;
      refresh();
    });
    addSeparator();

    const filterGrid = document.createElement("div");
    filterGrid.className = "menu-filter-grid";
    FILTER_TYPES.forEach((name, type) => {
      const button = document.createElement("button");
      button.type = "button";
      button.title = name;
      button.classList.toggle("is-active", type === bands[index].type);
      const icon = document.createElement("span");
      icon.className = "menu-filter-icon";
      icon.append(filterIcon(type));
      button.append(icon);
      button.addEventListener("click", () => {
        forSelection((band) => {
          band.type = type;
          if (type === 0 || type === 1) band.q = 0.75;
          if (type === 3) band.placementMode = 1;
        });
        hideContextMenu();
        refresh();
      });
      filterGrid.append(button);
    });
    menu.append(filterGrid);
    addSeparator();

    const routeGrid = document.createElement("div");
    routeGrid.className = "menu-route-grid";
    const selectedRoute = Math.abs(bands[index].placement) <= 1
      ? 1
      : bands[index].placementMode === 2
        ? (bands[index].placement < 0 ? 5 : 6)
        : bands[index].placementMode === 1
          ? (bands[index].placement < 0 ? 3 : 4)
          : (bands[index].placement < 0 ? 0 : 2);
    [
      ["L", 0, -100], ["C", 0, 0], ["R", 0, 100],
      ["M", 1, -100], ["S", 1, 100], ["T", 2, -100], ["S", 2, 100],
    ].forEach(([label, mode, placement], route) => {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = label;
      button.title = `${PLACEMENT_MODES[mode]} ${placement}`;
      button.classList.toggle("is-active", route === selectedRoute);
      button.addEventListener("click", () => {
        forSelection((band) => { band.placementMode = mode; band.placement = placement; });
        hideContextMenu();
        refresh();
      });
      routeGrid.append(button);
    });
    menu.append(routeGrid);
    addSeparator();

    const saturationWrap = document.createElement("div");
    saturationWrap.className = "menu-saturation-wrap";
    const saturationRow = document.createElement("button");
    saturationRow.type = "button";
    saturationRow.className = "menu-saturation-row";
    saturationRow.textContent = "Saturation";
    const saturationSubmenu = document.createElement("div");
    saturationSubmenu.className = "saturation-submenu";
    SATURATION_TYPES.forEach((name, mode) => {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = name;
      button.classList.toggle("is-active", mode === bands[index].satMode);
      button.addEventListener("click", () => {
        forSelection((band) => setSaturationMode(band, mode));
        hideContextMenu();
        refresh();
      });
      saturationSubmenu.append(button);
    });
    saturationWrap.append(saturationRow, saturationSubmenu);
    menu.append(saturationWrap);
    addSeparator();

    addButton("Reset equalizer", () => {
      bands.forEach((_, bandIndex) => { bands[bandIndex] = makeBand(); });
      selection.clear();
      primaryBand = -1;
      refresh();
    });
    if (selectedBands().length > 1) {
      addSeparator();
      addButton(`Bypass selected (${selectedBands().length})`, () => {
        forSelection((band) => { band.on = false; });
        refresh();
      });
    }

    const shellRect = plugin.getBoundingClientRect();
    menu.hidden = false;
    const left = clamp(event.clientX - shellRect.left, 4, shellRect.width - menu.offsetWidth - 4);
    const routeCentre = routeGrid.offsetTop + routeGrid.offsetHeight * 0.5;
    const top = clamp(event.clientY - shellRect.top - routeCentre, 4, shellRect.height - menu.offsetHeight - 4);
    menu.style.left = `${left}px`;
    menu.style.top = `${top}px`;
    const pointerX = event.clientX - shellRect.left;
    let opensLeft = Math.abs(pointerX - left) <= Math.abs(pointerX - (left + menu.offsetWidth));
    const submenuReach = 185;
    if (opensLeft && left < submenuReach) opensLeft = false;
    if (!opensLeft && left + menu.offsetWidth + submenuReach > shellRect.width) opensLeft = true;
    menu.classList.toggle("opens-left", opensLeft);
    refresh(false);
  }

  function hideContextMenu() {
    $("contextMenu").hidden = true;
  }

  function setSaturationMode(band, mode) {
    band.satMode = mode;
    band.driveCharacter = [4, 6, 7].includes(mode) ? 0.5 : 0;
    band.driveSecondary = mode === 4 ? 0.5 : 0;
  }

  function miniRange(container, config) {
    const label = document.createElement("label");
    label.className = `mini-control${config.orientation === "horizontal" ? " is-horizontal" : ""}`;
    const name = document.createElement("span");
    name.textContent = config.label;
    const input = document.createElement("input");
    input.type = "range";
    input.min = String(config.min);
    input.max = String(config.max);
    input.step = String(config.step);
    input.value = String(config.value);
    const output = document.createElement("output");
    output.textContent = config.mixed ? "MULTI" : config.format(config.value);
    label.append(name, input, output);
    container.append(label);
    const updateFill = () => {
      const proportion = (Number(input.value) - config.min) / Math.max(1e-9, config.max - config.min);
      label.style.setProperty("--value-ratio", String(clamp(proportion, 0, 1)));
    };
    updateFill();
    bindGroupRange(input, config.key, config.min, config.max, config.multiplicative, () => {
      output.textContent = config.format(Number(input.value));
      updateFill();
    });
    if (config.orientation !== "horizontal") bindVerticalDrag(label, input, config);
  }

  function bindVerticalDrag(surface, input, config) {
    let activePointer = null;
    let startY = 0;
    let startValue = 0;
    let dragSpan = 160;
    surface.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || input.disabled) return;
      event.preventDefault();
      activePointer = event.pointerId;
      startY = event.clientY;
      startValue = Number(input.value);
      dragSpan = Math.max(1, typeof config.dragSpan === "function" ? config.dragSpan() : (config.dragSpan ?? 160));
      const band = primary();
      if (band && config.key) {
        groupEdit = {
          input,
          key: config.key,
          primaryStart: Number(band[config.key]),
          starts: selectedBands().map((index) => [index, Number(bands[index][config.key])]),
        };
      }
      surface.setPointerCapture(event.pointerId);
    });
    surface.addEventListener("pointermove", (event) => {
      if (event.pointerId !== activePointer) return;
      const range = config.max - config.min;
      const fine = event.shiftKey ? 0.2 : 1;
      const raw = startValue - ((event.clientY - startY) / dragSpan) * range * fine;
      const steps = Math.round((clamp(raw, config.min, config.max) - config.min) / config.step);
      input.value = String(clamp(config.min + steps * config.step, config.min, config.max));
      input.dispatchEvent(new Event("input", { bubbles: true }));
    });
    const finish = (event) => {
      if (event.pointerId !== activePointer) return;
      activePointer = null;
      input.dispatchEvent(new Event("change", { bubbles: true }));
    };
    surface.addEventListener("pointerup", finish);
    surface.addEventListener("pointercancel", finish);
    surface.addEventListener("click", (event) => event.preventDefault());
  }

  function thresholdMeter(container, config) {
    const label = document.createElement("label");
    label.className = "threshold-meter";
    const name = document.createElement("span");
    name.textContent = "THRES";
    const visual = document.createElement("div");
    visual.className = "threshold-meter-visual";
    const lanes = document.createElement("div");
    lanes.className = "threshold-lanes";
    const left = document.createElement("i");
    const right = document.createElement("i");
    left.className = right.className = "threshold-lane";
    left.style.setProperty("--meter-level", "72%");
    right.style.setProperty("--meter-level", "58%");
    const line = document.createElement("b");
    line.className = "threshold-line";
    lanes.append(left, right, line);
    visual.append(lanes);
    const input = document.createElement("input");
    input.type = "range";
    input.min = String(config.min);
    input.max = String(config.max);
    input.step = String(config.step);
    input.value = String(config.value);
    const output = document.createElement("output");
    const update = () => {
      const value = Number(input.value);
      const proportion = (value - config.min) / (config.max - config.min);
      line.style.setProperty("--threshold-position", `${Math.round(clamp(proportion, 0, 1) * 100)}%`);
      output.textContent = config.mixed ? "MULTI" : config.format(value);
    };
    label.append(name, visual, input, output);
    container.append(label);
    update();
    bindGroupRange(input, config.key, config.min, config.max, false, update);
    bindVerticalDrag(label, input, {
      ...config,
      dragSpan: () => visual.getBoundingClientRect().height || 52,
    });
  }

  function bindGroupRange(input, key, min, max, multiplicative = false, updateOutput = () => {}) {
    input.addEventListener("pointerdown", () => {
      const band = primary();
      if (!band) return;
      groupEdit = {
        input,
        key,
        primaryStart: Number(band[key]),
        starts: selectedBands().map((index) => [index, Number(bands[index][key])]),
      };
    });
    input.addEventListener("input", () => {
      const value = clamp(Number(input.value), min, max);
      const active = groupEdit?.input === input ? groupEdit : null;
      if (!active) {
        forSelection((band) => { band[key] = value; });
      } else {
        const delta = value - active.primaryStart;
        const ratio = Math.abs(active.primaryStart) > 1e-9 ? value / active.primaryStart : 1;
        active.starts.forEach(([index, start]) => {
          bands[index][key] = clamp(multiplicative ? start * ratio : start + delta, min, max);
        });
      }
      updateOutput();
      refresh(false);
    });
    input.addEventListener("change", () => { groupEdit = null; refresh(); });
    input.addEventListener("pointerup", () => { groupEdit = null; refresh(); });
  }

  function updateControls() {
    const band = primary();
    const enabled = Boolean(band);

    $("workspace").querySelectorAll("input, select, button").forEach((control) => { control.disabled = !enabled; });
    ["freqField", "gainField", "qField", "slopeField", "bandOn", "bandSolo", "placementMode", "placement", "filterType", "saturationMode", "dynMode", "sidechain"].forEach((id) => { $(id).disabled = !enabled; });
    if (!band) return;

    const setNumber = (id, key, shownValue = band[key], formatter = (value) => String(Number(value.toFixed(3)))) => {
      const field = $(id);
      if (document.activeElement !== field) field.value = isMixed(key) ? "" : formatter(shownValue);
      field.placeholder = isMixed(key) ? "MULTI" : "";
    };
    setNumber("freqField", "freq", displayedFreq(band), (value) => String(Math.round(value)));
    setNumber("gainField", "gain", band.gain, (value) => Number(value).toFixed(1));
    setNumber("qField", "q", band.q, (value) => String(Number(Number(value).toFixed(2))));
    setNumber("slopeField", "slope");
    $("outputField").value = String(Number(clean(globalState.outputGain, 1)));

    $("bandOn").textContent = isMixed("on") ? "MIXED" : band.on ? "ON" : "OFF";
    $("bandOn").classList.toggle("is-active", !isMixed("on") && band.on);
    $("bandSolo").classList.toggle("is-active", soloBand === primaryBand);

    $("placementMode").value = String(band.placementMode);
    $("placement").value = String(band.placement);
    $("placementName").textContent = isMixed("placement") || isMixed("placementMode") ? "MULTI" : placementText(band);
    $("placementValue").textContent = isMixed("placement") ? "—" : `${Math.round(band.placement)}%`;
    $("filterType").value = String(band.type);
    $("filterGlyph").dataset.type = String(band.type);
    $("filterIconPreview").replaceChildren(filterIcon(band.type));
    $("saturationMode").value = String(band.satMode);

    $("dynMode").textContent = isMixed("dynMode") ? "MIXED" : band.dynMode ? "UP" : "DOWN";
    $("dynMode").classList.toggle("is-active", !isMixed("dynMode") && band.dynMode === 1);
    $("sidechain").textContent = isMixed("scSource") ? "MIXED" : band.scSource ? "EX SC" : "IN SC";
    $("sidechain").classList.toggle("is-active", !isMixed("scSource") && band.scSource === 1);

    const driveContainer = $("driveControls");
    driveContainer.replaceChildren();
    miniRange(driveContainer, { key: "drive", label: "DRIVE", min: 0, max: 36, step: 0.01, value: band.drive, mixed: isMixed("drive"), format: (v) => `${clean(v)} dB` });
    miniRange(driveContainer, { key: "driveCharacter", label: CHARACTER_NAMES[band.satMode], min: [2, 3, 5].includes(band.satMode) ? -1 : 0, max: 1, step: 0.001, value: band.driveCharacter, mixed: isMixed("driveCharacter"), format: () => formatCharacter(band) });

    const thresholdContainer = $("thresholdControl");
    thresholdContainer.replaceChildren();
    thresholdMeter(thresholdContainer, { key: "dynThresh", min: -60, max: 0, step: 0.1, value: band.dynThresh, mixed: isMixed("dynThresh"), format: (v) => clean(v) });
    const dynContainer = $("dynamicControls");
    dynContainer.replaceChildren();
    miniRange(dynContainer, { key: "dynRange", label: "RANGE", min: 0, max: 24, step: 0.1, value: band.dynRange, mixed: isMixed("dynRange"), format: (v) => `${clean(v)} dB` });
    miniRange(dynContainer, { key: "dynRatio", label: "RATIO", min: 1, max: 20, step: 0.1, value: band.dynRatio, mixed: isMixed("dynRatio"), format: (v) => clean(v, 2) });
    miniRange(dynContainer, { key: "dynSpeed", label: "SPEED", min: 0, max: 100, step: 0.01, value: band.dynSpeed, mixed: isMixed("dynSpeed"), format: (v) => `${Math.round(v)}%` });
    miniRange(dynContainer, { key: "dynLookahead", label: "LOOKAHEAD", min: 0, max: 5, step: 0.01, value: band.dynLookahead, mixed: isMixed("dynLookahead"), orientation: "horizontal", format: (v) => v < 0.005 ? "OFF" : `${clean(v, 2)} ms` });
  }

  function refresh(updateUi = true) {
    renderGraph();
    if (updateUi) updateControls();
  }

  function bindVerticalFieldDrag(field, config) {
    const surface = field.closest(".strip-field");
    let activePointer = null;
    let startY = 0;
    let session = null;
    let dragging = false;
    let lastClickAt = 0;
    surface.classList.add("strip-value-drag");
    surface.addEventListener("click", (event) => event.preventDefault());

    surface.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || field.disabled) return;
      session = config.begin();
      if (!session) return;
      event.preventDefault();
      activePointer = event.pointerId;
      startY = event.clientY;
      dragging = false;
      surface.setPointerCapture(event.pointerId);
    });

    surface.addEventListener("pointermove", (event) => {
      if (event.pointerId !== activePointer || !session) return;
      const deltaY = event.clientY - startY;
      if (!dragging && Math.abs(deltaY) < 2) return;
      if (!dragging) {
        dragging = true;
        groupEdit = null;
      }
      event.preventDefault();
      const fine = event.shiftKey ? 0.2 : 1;
      const dragAmount = (-deltaY / (config.dragSpan ?? 120)) * fine;
      const raw = config.exponential
        ? session.value * (config.max / config.min) ** dragAmount
        : session.value + (config.max - config.min) * dragAmount;
      const bounded = clamp(raw, config.min, config.max);
      const steps = Math.round((bounded - config.min) / config.step);
      const value = clamp(config.min + steps * config.step, config.min, config.max);
      const shown = session.apply(value);
      field.value = config.format(shown ?? value);
      renderGraph();
    });

    const finish = (event, cancelled = false) => {
      if (event.pointerId !== activePointer) return;
      activePointer = null;
      session = null;
      if (dragging) {
        lastClickAt = 0;
        field.blur();
        refresh();
      }
      else if (!cancelled) {
        const now = performance.now();
        if (now - lastClickAt <= 450) {
          lastClickAt = 0;
          field.focus({ preventScroll: true });
          field.select();
        } else {
          lastClickAt = now;
          const active = document.activeElement;
          if (active !== field && active?.matches?.(".strip-value-drag input")) active.blur();
        }
      }
      dragging = false;
    };
    surface.addEventListener("pointerup", (event) => finish(event));
    surface.addEventListener("pointercancel", (event) => finish(event, true));
  }

  function bindNumberField(id, key, min, max, displayed = false, formatter = (value) => clean(value, 2)) {
    const field = $(id);
    field.addEventListener("focus", () => {
      const band = primary();
      if (!band) return;
      const base = displayed ? displayedFreq(band) : band[key];
      groupEdit = {
        input: field,
        key,
        primaryStart: base,
        starts: selectedBands().map((index) => [index, bands[index][key]]),
      };
    });
    field.addEventListener("change", () => {
      if (!primary() || field.value === "") return;
      let value = clamp(Number(field.value), min, max);
      if (displayed) value /= 2 ** (globalState.shift / 12);
      const active = groupEdit?.input === field ? groupEdit : null;
      if (active) {
        const baseTarget = displayed ? value * 2 ** (globalState.shift / 12) : value;
        const multiplicative = key === "freq" || key === "q";
        const delta = baseTarget - active.primaryStart;
        const ratio = Math.abs(active.primaryStart) > 1e-9 ? baseTarget / active.primaryStart : 1;
        active.starts.forEach(([index, start]) => {
          bands[index][key] = clamp(multiplicative ? start * ratio : start + delta, min, max);
        });
      } else forSelection((band) => { band[key] = value; });
      groupEdit = null;
      refresh();
    });

    bindVerticalFieldDrag(field, {
      min,
      max,
      step: Number(field.step) || 0.01,
      exponential: key === "freq" || key === "q",
      format: formatter,
      begin: () => {
        const band = primary();
        if (!band) return null;
        const shiftFactor = displayed ? 2 ** (globalState.shift / 12) : 1;
        const startInternal = Number(band[key]);
        const starts = selectedBands().map((index) => [index, Number(bands[index][key])]);
        return {
          value: startInternal * shiftFactor,
          apply: (shownValue) => {
            const target = clamp(shownValue / shiftFactor, min, max);
            const multiplicative = key === "freq" || key === "q";
            const delta = target - startInternal;
            const ratio = Math.abs(startInternal) > 1e-9 ? target / startInternal : 1;
            starts.forEach(([index, start]) => {
              bands[index][key] = clamp(multiplicative ? start * ratio : start + delta, min, max);
            });
            return bands[primaryBand][key] * shiftFactor;
          },
        };
      },
    });
  }

  function bindUi() {
    fillSelect($("filterType"), FILTER_TYPES);
    fillSelect($("saturationMode"), SATURATION_TYPES);
    fillSelect($("placementMode"), PLACEMENT_MODES);
    bindWholeFieldPicker($("oversampling").closest(".header-select"), $("oversampling"));
    bindWholeFieldPicker($("phaseMode").closest(".header-select"), $("phaseMode"));
    bindWholeFieldPicker(document.querySelector(".strip-filter-field"), $("filterType"));
    bindWholeFieldPicker(document.querySelector(".saturation-field"), $("saturationMode"));
    bindWholeFieldPicker(document.querySelector(".route-mode-field"), $("placementMode"));

    $("themeToggle").addEventListener("click", () => plugin.classList.toggle("is-inverted"));
    $("phaseMode").addEventListener("change", (event) => {
      const value = Number(event.target.value);
      globalState.linearPhase = value > 0;
      globalState.linearQuality = Math.max(0, value - 1);
    });
    $("oversampling").addEventListener("change", (event) => { globalState.oversampling = Number(event.target.value); });
    $("amount").addEventListener("input", (event) => {
      globalState.amount = Number(event.target.value);
      $("amountValue").textContent = `${Math.round(globalState.amount * 100)}%`;
      renderGraph();
    });
    $("shift").addEventListener("input", (event) => {
      globalState.shift = Number(event.target.value);
      $("shiftValue").textContent = `${globalState.shift > 0 ? "+" : ""}${clean(globalState.shift)}`;
      refresh();
    });
    bindVerticalDrag($("amount").closest(".header-drag"), $("amount"), {
      min: -2,
      max: 2,
      step: 0.01,
    });
    bindVerticalDrag($("shift").closest(".header-drag"), $("shift"), {
      min: -48,
      max: 48,
      step: 0.1,
    });
    $("autoGain").addEventListener("click", () => {
      globalState.autoGainMode = (globalState.autoGainMode + 1) % 3;
      const labels = ["OFF", "REGULAR", "SMART"];
      $("autoGainValue").textContent = labels[globalState.autoGainMode];
      $("autoGain").classList.toggle("is-active", globalState.autoGainMode > 0);
    });
    $("power").addEventListener("click", () => {
      globalState.pluginEnabled = !globalState.pluginEnabled;
      $("powerValue").textContent = globalState.pluginEnabled ? "ON" : "OFF";
      $("power").classList.toggle("is-active", globalState.pluginEnabled);
      svg.style.opacity = globalState.pluginEnabled ? "1" : "0.36";
    });
    $("adaptiveQ").addEventListener("click", () => {
      globalState.adaptiveQ = !globalState.adaptiveQ;
      $("adaptiveQ").classList.toggle("is-active", globalState.adaptiveQ);
      renderGraph();
    });
    $("outputField").addEventListener("change", (event) => {
      globalState.outputGain = clamp(Number(event.target.value), -24, 24);
      event.target.value = String(Number(clean(globalState.outputGain, 1)));
      refresh();
    });

    bindNumberField("freqField", "freq", 20, 20000, true, (value) => String(Math.round(value)));
    bindNumberField("gainField", "gain", -36, 36, false, (value) => Number(value).toFixed(1));
    bindNumberField("qField", "q", 0.1, 24, false, (value) => String(Number(Number(value).toFixed(2))));
    bindNumberField("slopeField", "slope", 3, 96, false, (value) => clean(value));
    bindVerticalFieldDrag($("outputField"), {
      min: -24,
      max: 24,
      step: 0.1,
      format: (value) => String(Number(clean(value, 1))),
      begin: () => ({
        value: globalState.outputGain,
        apply: (value) => {
          globalState.outputGain = value;
          return value;
        },
      }),
    });

    $("bandOn").addEventListener("click", () => {
      const band = primary();
      if (!band) return;
      const value = isMixed("on") ? true : !band.on;
      forSelection((item) => { item.on = value; });
      refresh();
    });
    $("bandSolo").addEventListener("click", () => { soloBand = soloBand === primaryBand ? -1 : primaryBand; updateControls(); });
    $("placementMode").addEventListener("change", (event) => {
      const mode = Number(event.target.value);
      forSelection((band) => { band.placementMode = mode; });
      refresh();
    });
    bindGroupRange($("placement"), "placement", -100, 100, false, () => {
      const band = primary();
      if (!band) return;
      $("placementName").textContent = placementText(band);
      $("placementValue").textContent = `${Math.round(band.placement)}%`;
    });
    bindVerticalDrag(document.querySelector(".placement-field"), $("placement"), {
      key: "placement",
      min: -100,
      max: 100,
      step: 0.1,
    });
    $("filterType").addEventListener("change", (event) => {
      const type = Number(event.target.value);
      forSelection((band) => { band.type = type; if (type === 0 || type === 1) band.q = 0.75; });
      refresh();
    });
    $("saturationMode").addEventListener("change", (event) => { forSelection((band) => setSaturationMode(band, Number(event.target.value))); refresh(); });
    $("dynMode").addEventListener("click", () => { const next = primary().dynMode ? 0 : 1; forSelection((band) => { band.dynMode = next; }); refresh(); });
    $("sidechain").addEventListener("click", () => { const next = primary().scSource ? 0 : 1; forSelection((band) => { band.scSource = next; }); refresh(); });

    svg.addEventListener("pointerdown", beginGraphPointer);
    svg.addEventListener("pointermove", movePointer);
    svg.addEventListener("pointerup", endPointer);
    svg.addEventListener("pointercancel", endPointer);
    svg.addEventListener("contextmenu", (event) => {
      if (event.target === svg || event.target.closest("#gridLayer, #spectrumLayer, #bandCurveLayer, #responseLayer")) {
        event.preventDefault();
        selection.clear();
        primaryBand = -1;
        refresh();
      }
    });

    document.addEventListener("pointerdown", (event) => {
      if (!$("contextMenu").contains(event.target)) hideContextMenu();
      if (!$("selectMenu").contains(event.target)) hideSelectMenu();
    });
    document.addEventListener("keydown", (event) => {
      const tag = document.activeElement?.tagName;
      const editing = tag === "INPUT" || tag === "SELECT";
      if (!editing && (event.key === "Delete" || event.key === "Backspace")) {
        event.preventDefault();
        deleteBands(selectedBands());
      }
      if (event.key === "Escape") {
        hideContextMenu();
        hideSelectMenu();
      }
    });
  }

  drawGrid();
  drawSpectrum();
  bindUi();
  refresh();
})();
