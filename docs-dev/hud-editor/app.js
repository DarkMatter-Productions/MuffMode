// MuffMode HUD layout previewer (v1). Offline tooling — see README.md.
//
// Walks a CS_STATUSBAR layout string (from the in-game `hud_dump` command) the same way
// src/cgame/screen.cpp's CG_ExecuteLayoutString does, but only the *geometry* pass: anchor
// resolution + if/endif skipping, against a canonical 320x240 virtual screen. Sizes for
// pics/text are approximate (8px/char, fixed pic boxes) — enough to spot misalignment, not
// pixel-exact for kfont. See plan: docs / v1 scope.

"use strict";

const VW = 320, VH = 240, SCALE = 3;
const CHAR_W = 8, CHAR_H = 8;       // classic conchar approximation
const DIGIT_W = 16, DIGIT_H = 24;   // HUD num field digit box

const cv = document.getElementById("cv");
const ctx = cv.getContext("2d");
const statListEl = document.getElementById("statList");
const infoEl = document.getElementById("info");
const metaEl = document.getElementById("meta");
const gridEl = document.getElementById("grid");

let dump = null;          // { gametype, flags, stat_names, layout }
let statNames = {};       // index -> "STAT_x"
let mock = {};            // index -> number
let elements = [];        // walked draw elements
let selected = null;      // element being inspected/dragged

// ---- tokenizer: whitespace split, but "quoted strings" are one token ----------------------
function tokenize(s) {
  const out = [];
  let i = 0, n = s.length;
  while (i < n) {
    while (i < n && /\s/.test(s[i])) i++;
    if (i >= n) break;
    if (s[i] === '"') {
      i++;
      let start = i;
      while (i < n && s[i] !== '"') i++;
      out.push(s.slice(start, i));
      i++; // skip closing quote
    } else {
      let start = i;
      while (i < n && !/\s/.test(s[i])) i++;
      out.push(s.slice(start, i));
    }
  }
  return out;
}

// ---- anchor helpers (canonical 320x240, scale 1, no safe insets) ---------------------------
function resolveX(a) { return a.kind === "xr" ? VW + a.val : a.val; } // xl/xv == val here
function resolveY(a) { return a.kind === "yb" ? VH + a.val : a.val; } // yt/yv == val here
function suggestX(kind, px) { return kind === "xr" ? px - VW : px; }
function suggestY(kind, py) { return kind === "yb" ? py - VH : py; }

const statLabel = (idx) => statNames[idx] || ("stat " + idx);

// ---- the walk ------------------------------------------------------------------------------
function walk(layout) {
  const t = tokenize(layout);
  let p = 0;
  const next = () => (p < t.length ? t[p++] : "");
  const nextInt = () => parseInt(next(), 10) || 0;

  let xa = { kind: "xl", val: 0 }, ya = { kind: "yt", val: 0 };
  let ifDepth = 0, endifDepth = 0, skip = false;
  const els = [];
  const refStats = new Set();

  function cond(idx) { refStats.add(idx); return (mock[idx] || 0) !== 0; }
  function val(idx) { refStats.add(idx); return mock[idx] | 0; }

  // emit a draw element anchored at the current xa/ya
  function emit(kind, w, h, align, label, tokenText) {
    const ax = resolveX(xa), ay = resolveY(ya);
    let bx = ax;
    if (align === "right") bx = ax - w;
    else if (align === "center") bx = ax + VW / 2 - w / 2;
    els.push({ kind, ax, ay, w, h, align, label, tokenText,
               xa: { ...xa }, ya: { ...ya }, box: { x: bx, y: ay, w, h }, dx: 0, dy: 0 });
  }

  // consume the trailing (count, base, ...args) of a loc_* token; returns the base string
  function consumeLoc() {
    const count = nextInt();
    const base = next();
    for (let i = 0; i < count; i++) next();
    return base;
  }

  while (p < t.length) {
    const tok = next();
    switch (tok) {
      // ---- position ----
      case "xl": { const v = nextInt(); if (!skip) xa = { kind: "xl", val: v }; break; }
      case "xv": { const v = nextInt(); if (!skip) xa = { kind: "xv", val: v }; break; }
      case "xr": { const v = nextInt(); if (!skip) xa = { kind: "xr", val: v }; break; }
      case "yt": { const v = nextInt(); if (!skip) ya = { kind: "yt", val: v }; break; }
      case "yv": { const v = nextInt(); if (!skip) ya = { kind: "yv", val: v }; break; }
      case "yb": { const v = nextInt(); if (!skip) ya = { kind: "yb", val: v }; break; }

      // ---- conditionals ----
      case "if": { const idx = nextInt(); ifDepth++; if (!skip && !cond(idx)) { skip = true; endifDepth = ifDepth; } break; }
      case "ifgef": { nextInt(); ifDepth++; break; } // frame compare — treat as visible
      case "endif": { if (skip && ifDepth === endifDepth) skip = false; ifDepth--; break; }

      // ---- pics ----
      case "pic": {
        const idx = nextInt(); refStats.add(idx);
        const mini = /MINISCORE_(FIRST|SECOND)_PIC/.test(statLabel(idx));
        const sz = mini ? 24 : 16;
        if (!skip) emit("pic", sz, sz, "left", "pic " + statLabel(idx), "pic " + idx);
        break;
      }
      case "picn": {
        const name = next();
        let sz = 16;
        if (name.indexOf("/players/") >= 0) sz = 32;
        else if (name === "wheel/p_compass_selected") sz = 12;
        if (!skip) emit("pic", sz, sz, "left", "picn " + name, "picn " + name);
        break;
      }

      // ---- numbers ----
      case "num": { const w = nextInt(), idx = nextInt(); refStats.add(idx);
        if (!skip && (mock[idx] | 0) !== -999) emit("num", w * DIGIT_W, DIGIT_H, "left", "num " + w + " " + statLabel(idx), "num " + w + " " + idx); break; }
      case "hnum": if (!skip) emit("num", 3 * DIGIT_W, DIGIT_H, "left", "hnum (health)", "hnum"); break;
      case "anum": if (!skip) emit("num", 3 * DIGIT_W, DIGIT_H, "left", "anum (ammo)", "anum"); break;
      case "rnum": if (!skip) emit("num", 3 * DIGIT_W, DIGIT_H, "left", "rnum (armor)", "rnum"); break;
      case "lives_num": { const idx = nextInt(); if (!skip) emit("num", DIGIT_W, DIGIT_H, "left", "lives_num " + statLabel(idx), "lives_num " + idx); break; }

      // ---- text from configstring (we don't have the string) ----
      case "stat_string": case "stat_string2": case "loc_stat_string": case "loc_stat_cstring2": {
        const idx = nextInt(); refStats.add(idx);
        const align = tok.indexOf("cstring") >= 0 ? "center" : "left";
        const ph = "[" + statLabel(idx) + "]";
        if (!skip) emit("text", ph.length * CHAR_W, CHAR_H, align, tok + " " + statLabel(idx), tok + " " + idx);
        break;
      }
      case "loc_stat_rstring": {
        const idx = nextInt(); refStats.add(idx);
        const ph = "[" + statLabel(idx) + "]";
        if (!skip) emit("text", ph.length * CHAR_W, CHAR_H, "right", tok + " " + statLabel(idx), tok + " " + idx);
        break;
      }
      case "stat_pname": { const idx = nextInt(); refStats.add(idx);
        if (!skip) emit("text", 12 * CHAR_W, CHAR_H, "center", "stat_pname " + statLabel(idx), "stat_pname " + idx); break; }

      // ---- literal text ----
      case "string": case "string2": { const str = next();
        if (!skip) emit("text", (str.length || 1) * CHAR_W, CHAR_H, "left", JSON.stringify(str), tok); break; }
      case "cstring": case "cstring2": { const str = next();
        if (!skip) emit("text", (str.length || 1) * CHAR_W, CHAR_H, "center", JSON.stringify(str), tok); break; }
      case "loc_string": case "loc_string2": { const b = consumeLoc();
        if (!skip) emit("text", (b.length || 1) * CHAR_W, CHAR_H, "left", b, tok); break; }
      case "loc_rstring": case "loc_rstring2": { const b = consumeLoc();
        if (!skip) emit("text", (b.length || 1) * CHAR_W, CHAR_H, "right", b, tok); break; }
      case "loc_cstring": case "loc_cstring2": { const b = consumeLoc();
        if (!skip) emit("text", (b.length || 1) * CHAR_W, CHAR_H, "center", b, tok); break; }

      // ---- specials ----
      case "health_bars": if (!skip) emit("bars", 160, 24, "center", "health_bars", "health_bars"); break;
      case "story": if (!skip) emit("story", 120, CHAR_H, "center", "story (centre)", "story"); break;
      case "time_limit": { nextInt(); if (!skip) emit("marker", 40, CHAR_H, "right", "time_limit", "time_limit"); break; }
      case "dogtag": { next(); if (!skip) emit("marker", 198, 32, "left", "dogtag", "dogtag"); break; }

      default: break; // unknown / scoreboard-only token: ignore
    }
  }
  return { els, refStats };
}

// ---- mock-stat UI --------------------------------------------------------------------------
function buildStatUI(refStats) {
  // include referenced stats + any named ones, sorted by index
  const idxs = new Set(refStats);
  Object.keys(statNames).forEach(k => { if (k >= 0) idxs.add(+k); });
  const sorted = [...idxs].filter(i => i >= 0).sort((a, b) => a - b);
  statListEl.innerHTML = "";
  for (const idx of sorted) {
    const row = document.createElement("div");
    row.className = "statRow name";
    row.innerHTML = `<span class="idx">${idx}</span><code title="${statLabel(idx)}">${statLabel(idx)}</code>`;
    const inp = document.createElement("input");
    inp.type = "number";
    inp.value = (mock[idx] ?? 1);
    inp.addEventListener("input", () => { mock[idx] = parseInt(inp.value, 10) || 0; render(); });
    row.appendChild(inp);
    statListEl.appendChild(row);
  }
}

function setAllStats(v) {
  for (const row of statListEl.querySelectorAll("input[type=number]")) row.value = v;
  for (const k of Object.keys(mock)) mock[k] = v;
  // also seed any referenced-but-unlisted
  elements.forEach(() => {});
  render(true);
}

// ---- render --------------------------------------------------------------------------------
const KIND_FILL = { pic: "#2d4a73", num: "#2d6b3f", text: "#6b5a2d", bars: "#444", story: "#5a2d6b", marker: "#6b2d2d" };
const KIND_LINE = { pic: "#6fb3ff", num: "#7ad08a", text: "#e0c46f", bars: "#aaa", story: "#c78ff0", marker: "#ff8f8f" };

function render(rewalk) {
  if (rewalk !== false && dump) {
    const r = walk(dump.layout);
    elements = r.els;
  }
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, cv.width, cv.height);
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, cv.width, cv.height);
  ctx.scale(SCALE, SCALE);

  if (gridEl.checked) {
    ctx.strokeStyle = "rgba(255,255,255,0.06)";
    ctx.lineWidth = 1 / SCALE;
    for (let gx = 0; gx <= VW; gx += 16) { ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, VH); ctx.stroke(); }
    for (let gy = 0; gy <= VH; gy += 16) { ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(VW, gy); ctx.stroke(); }
    // centre lines
    ctx.strokeStyle = "rgba(111,179,255,0.25)";
    ctx.beginPath(); ctx.moveTo(VW / 2, 0); ctx.lineTo(VW / 2, VH); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(0, VH / 2); ctx.lineTo(VW, VH / 2); ctx.stroke();
  }

  ctx.font = "8px monospace";
  ctx.textBaseline = "top";
  for (const e of elements) {
    const x = e.box.x + e.dx, y = e.box.y + e.dy;
    ctx.fillStyle = (KIND_FILL[e.kind] || "#444") + (e === selected ? "" : "cc");
    ctx.globalAlpha = e === selected ? 0.95 : 0.7;
    ctx.fillRect(x, y, e.w, e.h);
    ctx.globalAlpha = 1;
    ctx.strokeStyle = e === selected ? "#fff" : (KIND_LINE[e.kind] || "#888");
    ctx.lineWidth = (e === selected ? 1.5 : 0.75) / SCALE;
    ctx.strokeRect(x, y, e.w, e.h);
    // label (clipped)
    ctx.save();
    ctx.beginPath(); ctx.rect(x, y, e.w, e.h); ctx.clip();
    ctx.fillStyle = "#fff";
    ctx.fillText(e.label, x + 1, y + 1);
    ctx.restore();
  }
}

// ---- interaction ---------------------------------------------------------------------------
function toVirtual(ev) {
  const r = cv.getBoundingClientRect();
  return { x: (ev.clientX - r.left) / (r.width / VW), y: (ev.clientY - r.top) / (r.height / VH) };
}
function hit(vx, vy) {
  for (let i = elements.length - 1; i >= 0; i--) {
    const e = elements[i], x = e.box.x + e.dx, y = e.box.y + e.dy;
    if (vx >= x && vx <= x + e.w && vy >= y && vy <= y + e.h) return e;
  }
  return null;
}

let drag = null;
cv.addEventListener("mousedown", (ev) => {
  const v = toVirtual(ev);
  const e = hit(v.x, v.y);
  selected = e;
  if (e) drag = { e, sx: v.x, sy: v.y, odx: e.dx, ody: e.dy };
  showInfo(e);
  render(false);
});
window.addEventListener("mousemove", (ev) => {
  if (!drag) return;
  const v = toVirtual(ev);
  drag.e.dx = drag.odx + (v.x - drag.sx);
  drag.e.dy = drag.ody + (v.y - drag.sy);
  showInfo(drag.e);
  render(false);
});
window.addEventListener("mouseup", () => { drag = null; });

function showInfo(e) {
  if (!e) { infoEl.innerHTML = '<span class="hint">Click an element to inspect it.</span>'; return; }
  const newAx = Math.round(e.ax + e.dx), newAy = Math.round(e.ay + e.dy);
  const sx = suggestX(e.xa.kind, newAx), sy = suggestY(e.ya.kind, newAy);
  const moved = (e.dx || e.dy);
  infoEl.textContent =
    `token : ${e.tokenText}\n` +
    `${e.label}\n` +
    `anchor: ${e.xa.kind} ${e.xa.val} , ${e.ya.kind} ${e.ya.val}\n` +
    `pos   : x=${Math.round(e.ax)} y=${Math.round(e.ay)} (${e.align})\n` +
    (moved ? `\n→ drag to:  ${e.xa.kind} ${sx}   ${e.ya.kind} ${sy}` : `\n(drag to read back corrected anchor values)`);
}

// ---- loading -------------------------------------------------------------------------------
function loadDump(d) {
  dump = d;
  statNames = {};
  for (const [k, v] of Object.entries(d.stat_names || {})) if (v) statNames[k] = v;
  mock = {};
  const r = walk(d.layout);          // first pass to discover referenced stats
  for (const idx of r.refStats) mock[idx] = 1;
  buildStatUI(r.refStats);
  metaEl.textContent = `${d.gametype || "?"}  ·  flags ${d.flags}  ·  ${d.layout.length} chars`;
  render();
}

document.getElementById("file").addEventListener("change", (ev) => {
  const f = ev.target.files[0]; if (!f) return;
  const fr = new FileReader();
  fr.onload = () => { try { loadDump(JSON.parse(fr.result)); } catch (err) { alert("Bad JSON: " + err.message); } };
  fr.readAsText(f);
});
document.getElementById("loadSample").addEventListener("click", () => loadDump(SAMPLE));
document.getElementById("allOn").addEventListener("click", () => setAllStats(1));
document.getElementById("allOff").addEventListener("click", () => setAllStats(0));
gridEl.addEventListener("change", () => render(false));

// ---- tiny built-in sample (smoke test; real data comes from `hud_dump`) --------------------
const SAMPLE = {
  gametype: "sample",
  flags: 0,
  stat_names: { "0": "STAT_HEALTH_ICON", "1": "STAT_HEALTH", "2": "STAT_AMMO_ICON", "3": "STAT_AMMO",
                "4": "STAT_ARMOR_ICON", "5": "STAT_ARMOR", "49": "STAT_SHOW_STATUSBAR", "-1": "" },
  layout:
    "yb -24 " +
    "if 49 xv 0 hnum xv 50 pic 0 endif " +
    "if 49 if 2 xv 100 anum xv 150 pic 2 endif endif " +
    "if 49 if 4 xv 200 rnum xv 250 pic 4 endif endif " +
    'xv 0 yt 14 string2 "SAMPLE — load a real hud_dump.json"'
};

loadDump(SAMPLE);
