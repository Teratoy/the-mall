"use strict";
// ---- The DIAGRAM tab: every moment, what makes it said, what it does, what comes next ----
// It reads /api/diagram (the code's account from tools/talk_map.py, data/talk_flow.txt and
// build/coverage.json) and draws on the page's own book, bubble and meanings from index.html.

/// How thick a branch's arrow is, and whether it is dashed, by how often it is taken.
const CHANCE_STROKE = { always: [3, ""], often: [2.2, ""], sometimes: [1.5, ""], seldom: [1.2, "4 3"], "": [1.2, "2 3"] };
/// How many cards a focus column shows before it folds the rest into "+N more".
const FOCUS_COLUMN = 12;
/// What each who reads as on a card.
const WHO_MARK = { someone: "→", nobody: "·", shout: "!", wall: "▮", "": "?" };
const WHO_WORDS = { someone: "to the one it talks to", nobody: "to nobody", shout: "shouted, heard far off",
  wall: "through a wall or door", "": "not written" };

let dia = null, diaVersion = "";
const dview = { on: false, mode: "overview", focus: null, pick: null, hover: null, back: [],
  unreached: false, unrev: false, nolines: false, flagged: false, tone: "", search: "" };
try { Object.assign(dview, JSON.parse(localStorage.getItem("diagramView") || "{}"), { hover: null, back: [] }); } catch (e) {}

function dsave() {
  try {
    const { on, mode, focus, pick, unreached, unrev, nolines, flagged, tone, search } = dview;
    localStorage.setItem("diagramView", JSON.stringify({ on, mode, focus, pick, unreached, unrev, nolines, flagged, tone, search }));
  } catch (e) {}
}
function dm(name) { return dia && dia.byName[name]; }
function outOf(name) { const m = dm(name); return m && m.flow ? m.flow.branches : []; }
function intoOf(name) { return dia.into[name] || []; }
function codeShows(from, to) { return dia.follows.some(f => f.from === from && f.to === to); }
function flagsOf(m) {
  const c = dia.checks, out = [];
  if (c.missing.includes(m.name)) out.push("no section in talk_flow.txt: not described yet");
  if (c.stranded.includes(m.name)) out.push("no when, and nothing leads here");
  for (const f of c.unwritten) if (f.from === m.name) out.push(`the code shows it leads to ${f.to} (${f.at}); no branch for it`);
  if (m.flow) for (const b of m.flow.branches) if (!dm(b.to)) out.push(`a branch to ${b.to}, which isn't a moment`);
  if (!m.inFile) out.push("no section in data/lines.txt");
  if (!m.uses.some(u => u.kind === "say")) out.push("never said by name in the code");
  return out;
}
function shown(m) {
  const q = dview.search.toLowerCase();
  return (!dview.unreached || m.said === 0) && (!dview.unrev || m.reviewed < m.lines) && (!dview.nolines || m.lines === 0)
    && (!dview.flagged || flagsOf(m).length) && (!dview.tone || m.tones[dview.tone])
    && (!q || m.name.includes(q) || m.group.toLowerCase().includes(q) ||
        (m.flow && [...m.flow.when, ...m.flow.does].some(r => r.text.toLowerCase().includes(q))));
}

// ---- Loading ----
async function dload() {
  const r = await fetch("api/diagram");
  dia = await r.json();
  diaVersion = dia.version;
  dia.byName = {};
  dia.into = {};
  for (const m of dia.moments) dia.byName[m.name] = m;
  for (const m of dia.moments) for (const b of m.flow ? m.flow.branches : [])
    (dia.into[b.to] = dia.into[b.to] || []).push({ ...b, from: m.name });
  // The groups, in the enum's order: each a run of moments sharing talk.h's comment.
  dia.groups = [];
  for (const m of dia.moments) {
    const g = dia.groups[dia.groups.length - 1];
    if (g && g.doc === m.group) g.moments.push(m);
    else dia.groups.push({ doc: m.group, title: m.group.split(/[:.;]/)[0] || "Other", moments: [m] });
  }
  if (!dview.pick || !dm(dview.pick)) dview.pick = dia.moments[0].name;
  if (!dview.focus || !dm(dview.focus)) dview.focus = dview.pick;
}
async function dpoll() {
  if (dview.on) {
    try {
      const v = (await (await fetch("api/version")).json()).version;
      if (v !== diaVersion) { await dload(); drender(); }
    } catch (e) {}
  }
  setTimeout(dpoll, 2000);
}

// ---- Switching tabs ----
async function showDiagram(on) {
  dview.on = on;
  $("app").style.display = on ? "none" : "";
  $("diagram").style.display = on ? "" : "none";
  dsave();
  if (on) { if (!dia) await dload(); drender(); }
}
function openLines(name) {
  showDiagram(false);
  showMoment(name);
}

// ---- Drawing ----
function card(m, extra) {
  const tones = Object.entries(m.tones).map(([t, n]) =>
    `<i class="tdot" style="background:var(--${t})" title="${n} ${t}"></i>`).join("");
  const rev = m.lines ? Math.round(100 * m.reviewed / m.lines) : 0;
  const said = m.said === null || m.said === undefined ? `<i class="reach" title="no coverage yet: run make coverage">?</i>`
    : m.said ? `<i class="reach yes" title="said ${m.said} times in the soak and scenarios">●</i>`
    : `<i class="reach no" title="never said in the soak or scenarios">○</i>`;
  const who = m.flow ? m.flow.who : "";
  const flagged = flagsOf(m).length ? `<i class="flagd" title="${esc(flagsOf(m).join("\n"))}">!</i>` : "";
  const cls = ["dcard", m.name === dview.pick ? "on" : "", extra || ""].join(" ");
  return `<div class="${cls}" data-m="${m.name}">
    <div class="dtop"><span class="who" title="${WHO_WORDS[who]}${m.flow && m.flow.about ? ", about someone" : ""}">${WHO_MARK[who]}</span>
      <span class="nm">${m.name}</span>${flagged}${said}</div>
    <div class="dbot"><span class="n" title="${m.reviewed} of ${m.lines} lines reviewed">${m.lines}</span>
      <span class="rbar"><i style="width:${rev}%"></i></span>${tones}
      ${m.chance !== null ? `<span class="ch" title="say-chance">${m.chance}</span>` : ""}
      <button class="fbtn" data-focus="${m.name}" title="Open its focus view">◎</button></div></div>`;
}
function drender() {
  if (!dia) return;
  dsave();
  dtoolbar();
  if (dview.mode === "focus") dfocus(); else doverview();
  dpanel();
}
function dtoolbar() {
  const c = dia.checks;
  const counts = [["missing", c.missing.length], ["stranded", c.stranded.length], ["unwritten", c.unwritten.length],
                  ["unknown", c.unknown.length], ["bad rows", c.bad.length]].filter(([, n]) => n);
  const unreached = dia.moments.filter(m => m.said === 0).length;
  $("dstats").innerHTML = `${dia.moments.length} moments · ${dia.moments.reduce((n, m) => n + (m.flow ? m.flow.branches.length : 0), 0)} branches · `
    + (dia.coverage ? `<b>${unreached}</b> never reached (coverage ${esc(dia.coverage.replace("T", " "))})` : "no coverage yet: run make coverage")
    + (counts.length ? ` · <span class="flagw">${counts.map(([k, n]) => n + " " + k).join(", ")}</span>` : " · flow checks clean");
  $("dmode").textContent = dview.mode === "focus" ? "◂ Overview (o)" : "Focus ◎ (f)";
  $("dback").style.display = dview.mode === "focus" && dview.back.length ? "" : "none";
}
function doverview() {
  let html = "";
  for (const g of dia.groups) {
    const ms = g.moments.filter(shown);
    if (!ms.length) continue;
    html += `<section class="dgroup"><h3 title="${esc(g.doc)}">${esc(g.title)}</h3><div class="dcards">${ms.map(m => card(m)).join("")}</div></section>`;
  }
  $("dcanvas").innerHTML = `<div class="dover">${html || '<p class="empty">No moment matches the filters.</p>'}<svg class="darrows"></svg></div>`;
  arrowsFor(dview.hover || dview.pick);
}
/// In the overview, the arrows of one moment only: where it leads and what leads to it.
function arrowsFor(name) {
  const root = $("dcanvas").querySelector(".dover"); if (!root) return;
  const svg = root.querySelector(".darrows");
  svg.setAttribute("width", root.scrollWidth); svg.setAttribute("height", root.scrollHeight);
  const edges = [...outOf(name).map(b => ({ from: name, to: b.to, chance: b.chance, dir: "out" })),
                 ...intoOf(name).map(b => ({ from: b.from, to: name, chance: b.chance, dir: "in" }))];
  for (const el of root.querySelectorAll(".dcard.lit")) el.classList.remove("lit", "lin", "lout");
  for (const e of edges) {
    const other = root.querySelector(`.dcard[data-m="${e.dir === "out" ? e.to : e.from}"]`);
    if (other) other.classList.add("lit", e.dir === "out" ? "lout" : "lin");
  }
  svg.innerHTML = defs() + edges.map(e => edge(root, e.from, e.to, e.chance, e.dir)).join("");
}
function defs() {
  return `<defs>${["out", "in"].map(d => `<marker id="ah-${d}" viewBox="0 0 8 8" refX="7" refY="4" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
    <path d="M0,0 L8,4 L0,8 z" class="ah ${d}"/></marker>`).join("")}</defs>`;
}
function box(root, name) {
  const el = root.querySelector(`.dcard[data-m="${name}"]`); if (!el) return null;
  const r = el.getBoundingClientRect(), o = root.getBoundingClientRect();
  return { x: r.left - o.left + root.scrollLeft, y: r.top - o.top + root.scrollTop, w: r.width, h: r.height };
}
function edge(root, from, to, chance, dir, label) {
  const a = box(root, from), b = box(root, to);
  if (!a || !b) return "";
  const [w, dash] = CHANCE_STROKE[chance] || CHANCE_STROKE[""];
  let x1, y1, x2, y2, d;
  if (a === b || from === to) return "";
  if (Math.abs((a.x + a.w / 2) - (b.x + b.w / 2)) > (a.w + b.w) / 2) {
    // Side by side: leave from the facing edge, a gentle S.
    const right = b.x > a.x;
    x1 = right ? a.x + a.w : a.x; y1 = a.y + a.h / 2;
    x2 = right ? b.x : b.x + b.w; y2 = b.y + b.h / 2;
    const k = Math.max(30, Math.abs(x2 - x1) / 2) * (right ? 1 : -1);
    d = `M${x1},${y1} C${x1 + k},${y1} ${x2 - k},${y2} ${x2},${y2}`;
  } else {
    // One above the other: leave from the facing edge.
    const down = b.y > a.y;
    x1 = a.x + a.w / 2; y1 = down ? a.y + a.h : a.y;
    x2 = b.x + b.w / 2; y2 = down ? b.y : b.y + b.h;
    const k = Math.max(20, Math.abs(y2 - y1) / 2) * (down ? 1 : -1);
    d = `M${x1},${y1} C${x1 + 40},${y1 + k} ${x2 + 40},${y2 - k} ${x2},${y2}`;
  }
  // The label sits near the end away from the middle card, where arrows don't bunch up.
  const t = to === dview.focus ? 0.25 : 0.75;
  const text = label ? `<text x="${x1 + (x2 - x1) * t}" y="${y1 + (y2 - y1) * t - 4}" class="elabel">${esc(label)}</text>` : "";
  return `<path d="${d}" class="edge ${dir}" stroke-width="${w}" ${dash ? `stroke-dasharray="${dash}"` : ""} marker-end="url(#ah-${dir})"><title>${from} → ${to}${chance ? ", " + chance : ""}</title></path>${text}`;
}
/// The focus view: the moment in the middle, what leads to it on the left and what it leads to
/// on the right, two steps out each way. A moment shows once, nearest the middle.
function dfocus() {
  const c = dview.focus;
  const col = new Map([[c, 0]]);
  const step = (names, d, next) => {
    const out = [];
    for (const n of names) for (const t of next(n)) if (!col.has(t)) { col.set(t, d); out.push(t); }
    return out;
  };
  const out1 = step([c], 1, n => outOf(n).map(b => b.to).filter(dm));
  const in1 = step([c], -1, n => intoOf(n).map(b => b.from));
  const out2 = step(out1, 2, n => outOf(n).map(b => b.to).filter(dm));
  const in2 = step(in1, -2, n => intoOf(n).map(b => b.from));
  const column = (names, d, title) => {
    const extra = names.length > FOCUS_COLUMN ? `<div class="more">+${names.length - FOCUS_COLUMN} more</div>` : "";
    if (names.length > FOCUS_COLUMN) for (const n of names.slice(FOCUS_COLUMN)) col.delete(n);
    return `<div class="fcol" data-d="${d}"><h4>${title}</h4>${names.slice(0, FOCUS_COLUMN).map(n => card(dm(n), d === 0 ? "centre" : "")).join("")
      || '<div class="none">nothing</div>'}${extra}</div>`;
  };
  // The outer columns only when they hold anything, so a narrow window keeps the middle in view.
  const cols = [in2.length ? column(in2, -2, "two before") : "", column(in1, -1, "leads here"), column([c], 0, ""),
                column(out1, 1, "leads to"), out2.length ? column(out2, 2, "then") : ""].filter(Boolean);
  $("dcanvas").innerHTML = `<div class="dover dfocus" style="grid-template-columns: repeat(${cols.length}, minmax(150px, 1fr))">
    ${cols.join("")}<svg class="darrows"></svg></div>`;
  const root = $("dcanvas").querySelector(".dover");
  const svg = root.querySelector(".darrows");
  svg.setAttribute("width", root.scrollWidth); svg.setAttribute("height", root.scrollHeight);
  let paths = defs();
  for (const [from, df] of col) for (const b of outOf(from)) {
    const dt = col.get(b.to);
    if (dt === undefined || dt === df) continue;
    // Only between neighbouring columns, or to and from the middle, so the picture stays readable.
    if (Math.abs(dt - df) !== 1 && df !== 0 && dt !== 0) continue;
    const touches = from === c || b.to === c;
    paths += edge(root, from, b.to, b.chance, dt > df ? "out" : "in", touches ? b.chance : "");
  }
  svg.innerHTML = paths;
}
function dpanel() {
  const m = dm(dview.pick);
  if (!m) { $("dpanel").innerHTML = ""; return; }
  const f = m.flow;
  const lm = typeof book !== "undefined" && book ? book.moments.find(x => x.name === m.name) : null;
  const rows = (list, cls) => list.length ? `<ul class="${cls}">${list.map(r => `<li>${esc(r.text)}</li>`).join("")}</ul>` : `<p class="none">not written</p>`;
  const branch = (b, other) => {
    const code = other === "to" ? codeShows(m.name, b.to) : codeShows(b.from, m.name);
    const name = other === "to" ? b.to : b.from;
    return `<li><a data-go="${name}">${name}</a> <span class="chw c-${b.chance || "none"}">${b.chance || "—"}</span>
      ${code ? '<span class="code" title="the code shows this follow">code</span>' : ""}<div class="bw">${esc(b.when)}</div></li>`;
  };
  const tones = Object.entries(m.tones).map(([t, n]) =>
    `<li><b style="color:var(--${t})">${t}</b> ${n} line${n === 1 ? "" : "s"}<div class="bw">${esc((typeof meaning !== "undefined" && meaning[t]) || "")}</div></li>`).join("");
  const uses = m.uses.map((u, i) => `<li class="use ${u.kind}"><a data-copy="${esc(u.file)}:${u.line}" title="copy file:line">${esc(u.file.replace(/^src\//, ""))}:${u.line}</a>
      <span class="k">${u.kind === "say" ? (u.run ? "said, as one of the " + u.run + " run" : "said") : "acted on"}</span>
      ${u.fn ? `<div class="fn">${esc(u.fn)}</div>` : ""}
      ${u.fnDoc ? `<div class="doc">${esc(u.fnDoc)}</div>` : ""}
      ${u.near && u.near !== u.fnDoc ? `<div class="near">${esc(u.near)}</div>` : ""}
      <code>${esc(u.code)}</code></li>`).join("");
  const sample = lm && lm.lines.length ? lm.lines[0] : null;
  const flags = flagsOf(m);
  $("dpanel").innerHTML = `
    <h2>${m.name} <span class="g">${m.enum}</span></h2>
    <div class="row"><button class="primary" data-lines="${m.name}">Open lines (l)</button>
      ${dview.mode === "focus" && dview.focus === m.name ? "" : `<button data-focus="${m.name}">Focus ◎</button>`}</div>
    <p class="gdoc">${esc(m.group)}</p>
    <div class="facts">
      <span>said</span><b>${f ? WHO_WORDS[f.who] + (f.about ? ", about someone" : "") : "not written"}</b>
      <span>lines</span><b>${m.lines} (${m.reviewed} reviewed)</b>
      <span>say-chance</span><b>${m.chance === null ? "always said" : m.chance}</b>
      <span>in tests</span><b>${m.said === null || m.said === undefined ? "unknown: run make coverage" : m.said ? "said " + m.said + " times" : '<span class="flagw">never said</span>'}</b>
    </div>
    ${flags.length ? `<ul class="flags">${flags.map(x => `<li>${esc(x)}</li>`).join("")}</ul>` : ""}
    ${sample ? `<div class="sample"></div>` : ""}
    <h3>When</h3>${rows(f ? f.when : [], "when")}
    <h3>Does</h3>${rows(f ? f.does : [], "does")}
    ${tones ? `<h3>Tones</h3><ul class="tones">${tones}</ul>` : ""}
    <h3>Leads to</h3>${f && f.branches.length ? `<ul class="br">${f.branches.map(b => branch(b, "to")).join("")}</ul>` : '<p class="none">nothing</p>'}
    <h3>Comes after</h3>${intoOf(m.name).length ? `<ul class="br">${intoOf(m.name).map(b => branch(b, "from")).join("")}</ul>` : '<p class="none">nothing</p>'}
    <h3>In the code</h3>${uses ? `<ul class="uses">${uses}</ul>` : '<p class="none">never named in the code</p>'}`;
  if (sample && ink) $("dpanel").querySelector(".sample").appendChild(drawBubble(sample.text, sample.tone));
}

// ---- Acting ----
function pick(name) {
  dview.pick = name;
  if (dview.mode === "focus") dpanel(); else { for (const el of $("dcanvas").querySelectorAll(".dcard.on")) el.classList.remove("on");
    const el = $("dcanvas").querySelector(`.dcard[data-m="${name}"]`); if (el) el.classList.add("on");
    arrowsFor(name); dpanel(); }
  $("dpanel").scrollTop = 0;
  dsave();
}
function focusOn(name) {
  if (dview.mode === "focus" && dview.focus && dview.focus !== name) dview.back.push(dview.focus);
  dview.mode = "focus"; dview.focus = name; dview.pick = name;
  drender();
  $("dcanvas").scrollTop = 0;
}
function overview() {
  dview.mode = "overview"; dview.back = [];
  drender();
  const el = $("dcanvas").querySelector(`.dcard[data-m="${dview.pick}"]`);
  if (el) el.scrollIntoView({ block: "center" });
}
function goBack() { if (dview.back.length) { const n = dview.back.pop(); dview.mode = "focus"; dview.focus = n; dview.pick = n; drender(); } }

/// Keys while the tab shows; true when it took the key, so the lines tab's keys don't run.
window.diagramKeys = e => {
  if (!dview.on) return false;
  if (e.target.tagName === "INPUT" || e.target.tagName === "SELECT") {
    if (e.target.id === "dsearch" && (e.key === "Escape" || e.key === "Enter")) {
      if (e.key === "Escape") { e.target.value = ""; dview.search = ""; drender(); }
      e.target.blur();
    }
    return true;
  }
  if (e.ctrlKey || e.metaKey || e.altKey) return true;
  const k = e.key, go = f => { e.preventDefault(); f(); };
  if (k === "f" || k === "Enter") go(() => focusOn(dview.pick));
  else if (k === "o" || k === "Escape") go(overview);
  else if (k === "b" || k === "Backspace") go(goBack);
  else if (k === "l") go(() => openLines(dview.pick));
  else if (k === "/") go(() => $("dsearch").focus());
  else if (k === "t") go(() => showDiagram(false));
  return true;
};

// ---- The tab's own page parts, put in beside the lines tab's ----
(function build() {
  const style = document.createElement("style");
  style.textContent = `
#diagram { position: fixed; inset: 0; display: grid; grid-template-rows: auto 1fr; background: var(--bg); }
#dbar { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; padding: 8px 12px; border-bottom: 1px solid var(--line); background: var(--panel); }
#dbar .stats { flex: 1 1 260px; }
#dbar #dsearch { width: 200px; }
.flagw { color: var(--accent); }
#dbody { display: grid; grid-template-columns: 1fr 380px; min-height: 0; }
#dcanvas { overflow: auto; min-height: 0; }
#dpanel { overflow-y: auto; border-left: 1px solid var(--line); background: var(--panel); padding: 12px 14px 60px; }
.dover { position: relative; padding: 10px 14px 80px; min-height: 100%; }
.darrows { position: absolute; left: 0; top: 0; pointer-events: none; overflow: visible; }
.darrows .edge { fill: none; stroke: var(--plead); opacity: .85; pointer-events: stroke; }
.darrows .edge.in { stroke: var(--accent); }
.darrows .ah.out { fill: var(--plead); } .darrows .ah.in { fill: var(--accent); }
.darrows .elabel { fill: var(--dim); font-size: 10px; text-anchor: middle; paint-order: stroke; stroke: var(--bg); stroke-width: 3px; }
.dgroup { margin: 6px 0 12px; }
.dgroup h3 { margin: 0 0 4px; color: var(--accent); font-size: 11px; font-weight: normal; text-transform: uppercase; letter-spacing: .05em; }
.dcards { display: flex; flex-wrap: wrap; gap: 6px; }
.dcard { width: 168px; padding: 4px 6px; border: 1px solid var(--line); border-radius: 4px; background: var(--panel); cursor: pointer; }
.dcard:hover { border-color: var(--dim); }
.dcard.on { border-color: var(--bright); background: var(--sel); }
.dcard.lout { border-color: var(--plead); } .dcard.lin { border-color: var(--accent); }
.dcard.centre { width: 200px; border-color: var(--bright); }
.dtop { display: flex; gap: 5px; align-items: baseline; }
.dtop .nm { color: var(--bright); flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.dtop .who { color: var(--dim); width: 10px; text-align: center; }
.reach { font-style: normal; font-size: 11px; color: var(--dim); } .reach.yes { color: var(--good); } .reach.no { color: var(--bad); }
.flagd { font-style: normal; color: var(--accent); font-weight: bold; }
.dbot { display: flex; gap: 4px; align-items: center; margin-top: 2px; font-size: 11px; color: var(--dim); }
.rbar { width: 34px; height: 4px; background: var(--raise); border-radius: 2px; overflow: hidden; }
.rbar i { display: block; height: 100%; background: var(--good); }
.tdot { display: inline-block; width: 7px; height: 7px; border-radius: 50%; }
.dbot .ch { color: var(--ink); }
.fbtn { margin-left: auto; padding: 0 5px; font-size: 11px; line-height: 15px; }
.dfocus { display: grid; gap: 0 44px; align-items: start; }
.dfocus .dcard { width: 150px; } .dfocus .dcard.centre { width: 170px; }
.fcol { display: grid; gap: 8px; justify-items: center; }
.fcol h4 { margin: 0; color: var(--dim); font-weight: normal; font-size: 11px; text-transform: uppercase; min-height: 14px; }
.fcol[data-d="0"] { align-self: center; }
.fcol .none, .fcol .more { color: var(--dim); font-size: 12px; }
#dpanel h2 { margin: 0; color: var(--bright); font-weight: normal; font-size: 17px; }
#dpanel h2 .g { color: var(--dim); font-size: 12px; }
#dpanel h3 { margin: 14px 0 4px; color: var(--accent); font-weight: normal; font-size: 11px; text-transform: uppercase; letter-spacing: .05em; }
#dpanel .row { display: flex; gap: 6px; margin: 8px 0; }
#dpanel .gdoc { color: var(--dim); margin: 6px 0; }
#dpanel .facts { display: grid; grid-template-columns: 90px 1fr; gap: 2px 8px; }
#dpanel .facts span { color: var(--dim); } #dpanel .facts b { font-weight: normal; color: var(--bright); }
#dpanel ul { margin: 0; padding-left: 16px; } #dpanel li { margin: 2px 0; }
#dpanel .flags { color: var(--accent); margin-top: 8px; }
#dpanel .none { color: var(--dim); margin: 0; }
#dpanel .sample { margin-top: 10px; }
#dpanel a { color: var(--bright); cursor: pointer; text-decoration: underline dotted; }
#dpanel .bw { color: var(--dim); font-size: 12px; }
#dpanel .chw { font-size: 11px; padding: 0 4px; border-radius: 3px; background: var(--raise); }
#dpanel .code { font-size: 10px; color: var(--good); border: 1px solid var(--good); border-radius: 3px; padding: 0 3px; }
#dpanel ul.uses { list-style: none; padding: 0; }
#dpanel .use { padding: 5px 0; border-top: 1px solid var(--line); }
#dpanel .use .k { color: var(--dim); font-size: 11px; } #dpanel .use.check .k { color: var(--accent); }
#dpanel .use .fn { color: var(--ink); } #dpanel .use .doc { color: var(--dim); font-size: 12px; }
#dpanel .use .near { color: var(--dim); font-size: 12px; font-style: italic; }
#dpanel .use code { display: block; color: var(--ink); font-size: 11px; white-space: pre-wrap; word-break: break-all; opacity: .8; }
.tabs { display: flex; gap: 4px; } .tabs button.on { border-color: var(--accent); color: var(--accent); }
@media (max-width: 900px) { #dbody { grid-template-columns: 1fr; grid-template-rows: 1fr 45vh; } #dpanel { border-left: 0; border-top: 1px solid var(--line); } }
`;
  document.head.appendChild(style);
  const el = document.createElement("div");
  el.id = "diagram"; el.style.display = "none";
  el.innerHTML = `<div id="dbar">
      <div class="tabs"><button id="dtabLines">Lines</button><button class="on">Diagram</button></div>
      <button id="dmode"></button><button id="dback">◂ Back (b)</button>
      <input id="dsearch" placeholder="Search moments, whens  ( / )" autocomplete="off">
      <label class="tog"><input type="checkbox" id="dUnreached"> unreached</label>
      <label class="tog"><input type="checkbox" id="dUnrev"> unreviewed</label>
      <label class="tog"><input type="checkbox" id="dNolines"> no lines</label>
      <label class="tog"><input type="checkbox" id="dFlagged"> flagged</label>
      <select id="dTone"><option value="">any tone</option></select>
      <span class="stats" id="dstats"></span></div>
    <div id="dbody"><div id="dcanvas"></div><aside id="dpanel"></aside></div>`;
  document.body.appendChild(el);
  // The lines tab's way over.
  const tabs = document.createElement("div");
  tabs.className = "tabs";
  tabs.innerHTML = `<button class="on">Lines</button><button id="ltabDiagram">Diagram (t)</button>`;
  $("side").querySelector(".top").prepend(tabs);
  $("ltabDiagram").onclick = () => showDiagram(true);
  $("dtabLines").onclick = () => showDiagram(false);
  $("dmode").onclick = () => dview.mode === "focus" ? overview() : focusOn(dview.pick);
  $("dback").onclick = goBack;
  for (const t of ["warm", "rude", "plead", "fear", "threat"]) $("dTone").insertAdjacentHTML("beforeend", `<option>${t}</option>`);
  const filters = { dUnreached: "unreached", dUnrev: "unrev", dNolines: "nolines", dFlagged: "flagged" };
  for (const [id, k] of Object.entries(filters)) { $(id).checked = !!dview[k]; $(id).onchange = e => { dview[k] = e.target.checked; drender(); }; }
  $("dTone").value = dview.tone; $("dTone").onchange = e => { dview.tone = e.target.value; drender(); };
  $("dsearch").value = dview.search; $("dsearch").oninput = e => { dview.search = e.target.value.trim(); drender(); };
  $("dcanvas").addEventListener("click", e => {
    const f = e.target.closest("[data-focus]"); if (f) return focusOn(f.dataset.focus);
    const c = e.target.closest(".dcard"); if (!c) return;
    if (dview.mode === "focus" && c.dataset.m !== dview.focus) focusOn(c.dataset.m); else pick(c.dataset.m);
  });
  $("dcanvas").addEventListener("dblclick", e => { const c = e.target.closest(".dcard"); if (c) focusOn(c.dataset.m); });
  $("dcanvas").addEventListener("mouseover", e => {
    if (dview.mode !== "overview") return;
    const c = e.target.closest(".dcard"); const n = c ? c.dataset.m : null;
    if (n !== dview.hover) { dview.hover = n; arrowsFor(n || dview.pick); }
  });
  $("dcanvas").addEventListener("mouseleave", () => { if (dview.mode === "overview") { dview.hover = null; arrowsFor(dview.pick); } });
  $("dpanel").addEventListener("click", e => {
    const g = e.target.closest("[data-go]"); if (g) return dview.mode === "focus" ? focusOn(g.dataset.go) : (pick(g.dataset.go), overviewTo(g.dataset.go));
    const f = e.target.closest("[data-focus]"); if (f) return focusOn(f.dataset.focus);
    const l = e.target.closest("[data-lines]"); if (l) return openLines(l.dataset.lines);
    const c = e.target.closest("[data-copy]");
    if (c) { navigator.clipboard && navigator.clipboard.writeText(c.dataset.copy); toast("Copied " + c.dataset.copy, true); }
  });
  window.addEventListener("resize", () => { if (dview.on) drender(); });
  setTimeout(dpoll, 2000);
  if (dview.on) setTimeout(() => showDiagram(true), 0);
})();
function overviewTo(name) {
  const el = $("dcanvas").querySelector(`.dcard[data-m="${name}"]`);
  if (el) el.scrollIntoView({ block: "center", behavior: "smooth" });
}
