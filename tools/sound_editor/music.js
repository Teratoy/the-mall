// The MUSIC tab of the sound editor: the songs of data/music.txt, a song at a
// time. The song page holds its tempo, wow, gain, channels, flow and vary; an
// instrument page its layers, as a sound's; a section page its notes, a row
// down per row and a column per channel. Everything is heard through the
// game's own synth (build/sound_sheet --music-one), and SAVE writes the whole
// song back, keeping the lines of every part not changed. See
// AI/music_plan.md and the header of data/music.txt.
"use strict";

// Sliders of the song and its instruments, beside the layer ones.
Object.assign(RANGES, {
  tempo: {min: 30, max: 200, step: 1}, rowsPerBeat: {min: 1, max: 8, step: 1}, wow: {min: 0, max: 60, step: 0.5},
  songGain: {min: 0, max: 2, step: 0.01}, insGain: {min: 0, max: 2, step: 0.01}, detune: {min: -50, max: 50, step: 0.5},
});
Object.assign(TIPS, {
  tempo: "beats a minute", rowsPerBeat: "rows a beat", wow: "how far the old tape drifts out of tune, in cents",
  songGain: "how loud the whole mix is before the crush", insGain: "how loud this instrument is",
  detune: "cents out of tune, the whole instrument",
});
// The song's own defaults, as music::Song has them.
const SONG_DEFAULTS = {tempo: 90, rows: 4, wow: 0, gain: 1};
// How long the song plays from PLAY, in seconds.
const SONG_SECONDS = 60;
// A cell: a note (flats only), a hit, nothing new, or a stop.
const CELL = /^([A-G][-b]-?\d|x|==|\.+)$/;

let tab = "sounds", songs = [], mversion = "", mopen = null;

function mstatus(text, kind) {
  $("mstatus").textContent = text || "";
  $("mstatus").className = kind || "";
}

// The part being looked at: the song itself (part -1), an instrument or a section.
const openSong = () => songs.find(s => s.name === mopen.song);
const openPart = () => mopen.part < 0 ? null : openSong().edit.parts[mopen.part];
// Muting is for listening only: it changes nothing saved.
const unmuted = o => JSON.stringify(o, (k, v) => k === "muted" ? undefined : v);
const songDirty = s => unmuted(s.edit) !== unmuted(s.base.saved);
const channelsOf = s => (s.edit.parts.find(p => p.kind === "channels")?.text || "").split(/\s+/).filter(Boolean);
const rowsPerBeat = s => s.edit.options.rows ?? SONG_DEFAULTS.rows;

async function loadMusic() {
  const sheet = await (await fetch("/api/music")).json();
  mversion = sheet.version;
  const old = Object.fromEntries(songs.map(s => [s.name, s]));
  songs = sheet.songs;
  for (const s of songs) {
    const saved = {options: s.options, parts: s.parts};
    const was = old[s.name];
    // Edits not yet saved stay, still built on the file they began from, and
    // answer to the block last seen.
    if (was && songDirty(was)) s.edit = was.edit, s.base = was.base, s.seen = was.seen;
    else s.edit = clone(saved), s.base = {saved: clone(saved), block: s.block}, s.seen = s.block;
  }
  drawMusicList();
  if (!songs.length) return mstatus("No songs in data/music.txt.", "bad");
  if (!mopen || !openSong()) mopen = {song: songs[0].name, part: -1};
  if (tab === "music") showPart();
}

// --- Writing a song back as text.

function songLine(s) {
  let line = "song " + s.name;
  const o = s.edit.options;
  for (const k of ["tempo", "rows", "wow", "gain", "bits", "rate"]) if (o[k] !== undefined) line += ` ${k} ${fmt(o[k])}`;
  return line;
}

function partText(p, skipMuted) {
  if (p.kind === "instrument") {
    let line = "instrument " + p.name;
    for (const k of ["gain", "detune"]) if (p.options[k] !== undefined) line += ` ${k} ${fmt(p.options[k])}`;
    if (p.sustain) line += " sustain";
    return [line, ...p.layers.filter(l => !(skipMuted && l.muted)).map(layerLine)].join("\n");
  }
  if (p.kind === "section") {
    const rows = p.rows.map(r => ("    " + r.cells.map(c => c.padEnd(3)).join(" ")).trimEnd() +
      (r.comment ? "    # " + r.comment : ""));
    return ["section " + p.name, ...rows].join("\n");
  }
  return p.kind + " " + p.text;
}

// The song's block: its file lines, with each changed part written anew and
// new sections after the last of the old ones.
function songBlock(s, skipMuted) {
  const lines = s.base.block.split("\n"), saved = s.base.saved, out = [];
  const same = (a, b) => unmuted(a) === unmuted(b) && !(skipMuted && JSON.stringify(a).includes('"muted":true'));
  const fresh = s.edit.parts.slice(saved.parts.length);
  const lastSection = saved.parts.map(p => p.kind).lastIndexOf("section");
  out.push(same(s.edit.options, saved.options) ? lines[0] : songLine(s));
  for (let j = 1; j < lines.length;) {
    const i = saved.parts.findIndex(p => p.from === j);
    if (i < 0) { out.push(lines[j++]); continue; }
    const p = s.edit.parts[i];
    if (same(p, saved.parts[i])) out.push(...lines.slice(j, saved.parts[i].to));
    else out.push(...partText(p, skipMuted).split("\n"));
    j = saved.parts[i].to;
    if (i === lastSection) for (const f of fresh) out.push("", ...partText(f, skipMuted).split("\n"));
  }
  if (lastSection < 0) for (const f of fresh) out.push("", ...partText(f, skipMuted).split("\n"));
  return out.join("\n");
}

// The song to hear, muted layers left out.
const songText = s => `crush ${s.crush.bits} ${s.crush.rate}\n${songBlock(s, true)}\n`;

// --- Listening.

let stopAt = null;

async function hear(text, song, what, seconds, label) {
  ctx = ctx || new AudioContext();
  mstatus("Rendering…");
  let r;
  try {
    r = await fetch("/api/music/render", {method: "POST", body: JSON.stringify({text, song, what, seconds})});
  } catch (e) {
    return mstatus("The editor isn't running: start it with ./sounds.sh", "bad");
  }
  if (!r.ok) return mstatus((await r.json()).error, "bad");
  const buffer = await ctx.decodeAudioData(await r.arrayBuffer());
  stopHearing();
  const src = ctx.createBufferSource();
  src.buffer = buffer;
  src.connect(ctx.destination);
  src.start();
  playing = src;
  drawWave(buffer, 1, label, +r.headers.get("X-Peak"), +r.headers.get("X-Clipped"), "m");
  mstatus("");
}

function stopHearing() {
  if (playing) try { playing.stop(); } catch (e) {}
  playing = null;
}

function playSong(from) {
  const s = openSong();
  hear(songText(s), s.name, from ? "from:" + from : "-", SONG_SECONDS,
    from ? `the song from ${from}, as played` : "the song as played");
}

function playSection(p) {
  const s = openSong();
  hear(songText(s), s.name, p.name, 0, "section " + p.name + " alone");
}

// One note of an instrument, alone, held three beats at 60 a minute.
function playNote(p, note) {
  const s = openSong();
  if (!CELL.test(note) || note.startsWith(".") || note === "==") return mstatus(`${note} is no note`, "bad");
  const text = `crush ${s.crush.bits} ${s.crush.rate}\nsong __note tempo 60 rows 1 gain ${s.edit.options.gain ?? 1}\n` +
    `${partText(p, true)}\nchannels ${p.name}\nsection note\n    ${note}\n    .\n    .\n    ==\n`;
  hear(text, "__note", "note", 0, `${p.name} playing ${note}`);
}

// --- The list.

function drawMusicList() {
  const list = $("mlist");
  list.innerHTML = "";
  for (const s of songs) {
    const g = document.createElement("div");
    g.className = "grp", g.textContent = "song " + s.name;
    list.appendChild(g);
    const entry = (part, kind, name, act) => {
      const row = document.createElement("div");
      row.className = "snd" + (mopen && mopen.song === s.name && mopen.part === part ? " on" : "");
      const p = document.createElement("button");
      p.className = "p", p.textContent = "▶", p.title = "Play";
      p.onclick = e => { e.stopPropagation(); mopen = {song: s.name, part}; showPart(); act(); };
      const k = document.createElement("span");
      k.className = "k", k.textContent = kind;
      const n = document.createElement("span");
      n.className = "n", n.textContent = name;
      const d = document.createElement("span");
      d.className = "d", d.textContent = part < 0 && songDirty(s) ? "●" : "", d.title = "changed, not saved";
      row.append(p, k, n, d);
      row.onclick = () => { mopen = {song: s.name, part}; showPart(); };
      list.appendChild(row);
    };
    entry(-1, "", "the song", () => playSong());
    s.edit.parts.forEach((p, i) => {
      if (p.kind === "instrument") entry(i, "♪", p.name, () => playNote(p, "C-4"));
      if (p.kind === "section") entry(i, "§", p.name, () => playSection(p));
    });
  }
}

function changedMusic() {
  const s = openSong();
  $("msave").disabled = $("mrevert").disabled = !songDirty(s);
  // Find the song's row by walking the list: its group header comes first.
  let inSong = false;
  for (const el of $("mlist").children) {
    if (el.classList.contains("grp")) inSong = el.textContent === "song " + s.name;
    else if (inSong && el.querySelector(".n").textContent === "the song")
      el.querySelector(".d").textContent = songDirty(s) ? "●" : "";
  }
}

// --- The open part.

function tool(text, title, act, key) {
  const b = document.createElement("button");
  b.innerHTML = text + (key ? `<kbd>${key}</kbd>` : "");
  b.title = title, b.onclick = act;
  $("mtools").appendChild(b);
  return b;
}
function sep() {
  const s = document.createElement("span");
  s.className = "sep";
  $("mtools").appendChild(s);
}

function textRow(label, get, set, title) {
  const row = document.createElement("div");
  row.className = "text", row.title = title || "";
  const lab = document.createElement("label");
  lab.textContent = label;
  const input = document.createElement("input");
  input.value = get();
  input.onchange = () => { set(input.value.trim()); changedMusic(); };
  row.append(lab, input);
  return row;
}

function card(title, ...children) {
  const c = document.createElement("div");
  c.className = "card";
  const bar = document.createElement("div");
  bar.className = "bar";
  bar.innerHTML = `<span class="t">${title}</span>`;
  const rows = document.createElement("div");
  rows.className = "rows";
  rows.append(...children);
  c.append(bar, rows);
  return c;
}

function showPart() {
  const s = openSong(), p = openPart(), box = $("mpart");
  drawMusicList();
  $("mtools").innerHTML = "";
  box.innerHTML = "";
  $("mname").textContent = p ? p.name : s.name;
  $("mkind").textContent = p ? `${p.kind} of song ${s.name}` : "song";
  $("mdoc").textContent = "";
  if (!p) showSong(s, box);
  else if (p.kind === "instrument") showInstrument(s, p, box);
  else showSection(s, p, box);
  sep();
  tool("STOP", "Stop listening", stopHearing, "esc");
  sep();
  tool("SAVE", "Write the song back to data/music.txt", saveSong, "ctrl+s").id = "msave";
  tool("REVERT", "Back to the song as saved", revertSong).id = "mrevert";
  changedMusic();
}

function showSong(s, box) {
  const o = s.edit.options;
  $("mdoc").textContent = "Played from its first section; then each section is followed as its flow says, " +
    "and each time a section starts, a channel may drop out as vary says.";
  tool("PLAY", `The song as the game plays it, ${SONG_SECONDS} s`, () => playSong(), "space");
  const slide = (key, file, label) => slider(key, label, () => o[file] ?? SONG_DEFAULTS[file],
    v => o[file] = key === "tempo" || key === "rowsPerBeat" ? Math.round(v) : v,
    () => (o[file] ?? SONG_DEFAULTS[file]) === SONG_DEFAULTS[file], () => { changedMusic(); if (key === "rowsPerBeat") showPart(); });
  box.appendChild(card("the song", slide("tempo", "tempo", "tempo"), slide("rowsPerBeat", "rows", "rows"),
    slide("wow", "wow", "wow"), slide("songGain", "gain", "gain")));
  const texts = s.edit.parts.filter(p => ["channels", "flow", "vary"].includes(p.kind)).map(p =>
    textRow(p.kind, () => p.text, v => p.text = v, {
      channels: "an instrument per channel, NAME or NAME:CENTS out of tune",
      flow: "FROM TO WEIGHT [TO WEIGHT ...]: what may follow a section",
      vary: "drop CHANCE [keep NAME ...]: how often a channel drops out, but those kept"}[p.kind]));
  const c = card("channels, flow and vary", ...texts);
  c.querySelector(".rows").style.display = "block";
  box.appendChild(c);
}

function showInstrument(s, p, box) {
  const note = document.createElement("input");
  note.className = "note", note.value = "C-4", note.title = "The note PLAY NOTE plays";
  tool("PLAY NOTE", "This instrument alone, the note beside", () => playNote(p, note.value.trim()), "space");
  $("mtools").appendChild(note);
  sep();
  tool("+ LAYER", "Add a layer", () => {
    p.layers.push({wave: "square", from: 440, keys: {}});
    showPart(); playNote(p, note.value.trim());
  });
  const redo = () => { changedMusic(); playNote(p, note.value.trim()); };
  const sustain = document.createElement("label");
  sustain.className = "row" + (p.sustain ? " set" : "");
  sustain.title = "Held for as long as the note is written, not struck and let ring";
  const tick = document.createElement("input");
  tick.type = "checkbox", tick.checked = p.sustain;
  tick.onchange = () => { p.sustain = tick.checked; redo(); };
  sustain.append("sustain", tick);
  box.appendChild(card("the instrument",
    slider("insGain", "gain", () => p.options.gain ?? 1, v => p.options.gain = v, () => (p.options.gain ?? 1) === 1, redo),
    slider("detune", "detune", () => p.options.detune ?? 0, v => p.options.detune = v, () => (p.options.detune ?? 0) === 0, redo),
    sustain));
  $("mdoc").textContent = "Layers are written at A4 (440 Hz); each note moves them to its pitch.";
  const layers = document.createElement("div");
  box.appendChild(layers);
  const draw = () => layerCards(p.layers, layers, draw, redo, () => playNote(p, note.value.trim()));
  draw();
}

function showSection(s, p, box) {
  const bar = rowsPerBeat(s) * 4;
  tool("PLAY", "This section alone", () => playSection(p), "space");
  tool("PLAY FROM HERE", `The song from this section, as played, ${SONG_SECONDS} s`, () => playSong(p.name));
  sep();
  tool("+ BAR", `Add ${bar} rows at the end`, () => {
    for (let i = 0; i < bar; i++) p.rows.push({cells: channelsOf(s).map(() => "."), comment: ""});
    showPart(); $("mbody").scrollTop = $("mbody").scrollHeight;
  });
  tool("− BAR", `Take the last ${bar} rows away`, () => {
    if (p.rows.length <= bar) return mstatus("A section keeps at least a bar.", "bad");
    p.rows.splice(p.rows.length - bar, bar);
    showPart();
  });
  tool("COPY SECTION", "A copy of this section, under a new name, after the last", () => {
    const name = (prompt("The new section's name:", p.name + "2") || "").trim();
    if (!name) return;
    if (s.edit.parts.some(q => q.kind === "section" && q.name === name)) return mstatus(`${name} is taken`, "bad");
    s.edit.parts.push({kind: "section", name, rows: clone(p.rows)});
    mopen = {song: s.name, part: s.edit.parts.length - 1};
    showPart();
    mstatus(`Made ${name}. Add it to a flow on the song's page to hear it in the song.`, "good");
  });
  $("mdoc").textContent = "A cell is a note (C-4, Db4: flats only), x (a hit), . (nothing new) or == (stop). " +
    "↑ ↓ ← → and Enter move; the comment column keeps the chord names.";
  box.appendChild(grid(s, p));
}

function grid(s, p) {
  const names = channelsOf(s), per = rowsPerBeat(s);
  const table = document.createElement("table");
  table.className = "grid";
  const head = table.insertRow();
  head.appendChild(document.createElement("th"));
  for (const n of [...names, "comment"]) {
    const th = document.createElement("th");
    th.textContent = n;
    head.appendChild(th);
  }
  const look = (input, v) => {
    input.classList.toggle("set", !/^\.+$/.test(v));
    input.classList.toggle("bad", !CELL.test(v));
  };
  p.rows.forEach((r, i) => {
    const tr = table.insertRow();
    if (i % per === 0) tr.classList.add("beat");
    if (i % (per * 4) === 0 && i) tr.classList.add("bar");
    const n = tr.insertCell();
    n.className = "n", n.textContent = i % per === 0 ? `${Math.floor(i / per / 4) + 1}.${Math.floor(i / per) % 4 + 1}` : "";
    for (let c = 0; c <= names.length; c++) {
      const input = document.createElement("input");
      input.dataset.r = i, input.dataset.c = c;
      if (c === names.length) {
        input.className = "c", input.value = r.comment;
        input.onchange = () => { r.comment = input.value.trim(); changedMusic(); };
      } else {
        input.value = r.cells[c] ?? ".";
        look(input, input.value);
        input.oninput = () => look(input, input.value.trim() || ".");
        input.onchange = () => {
          const v = input.value.trim() || ".";
          while (r.cells.length < names.length) r.cells.push(".");
          r.cells[c] = v, input.value = v;
          look(input, v);
          changedMusic();
        };
      }
      input.onfocus = () => input.select();
      input.onkeydown = e => cellKey(e, table, i, c, names.length);
      tr.insertCell().appendChild(input);
    }
  });
  return table;
}

// Moving about the grid like a tracker.
function cellKey(e, table, r, c, last) {
  const to = (dr, dc) => {
    const next = table.querySelector(`input[data-r="${r + dr}"][data-c="${c + dc}"]`);
    if (next) { e.preventDefault(); e.target.dispatchEvent(new Event("change")); next.focus(); }
  };
  const t = e.target, atStart = t.selectionStart === 0 && t.selectionEnd === 0 || t.selectionEnd - t.selectionStart === t.value.length;
  const atEnd = t.selectionStart === t.value.length || atStart;
  if (e.key === "ArrowUp") to(-1, 0);
  else if (e.key === "ArrowDown" || e.key === "Enter") to(1, 0);
  else if (e.key === "ArrowLeft" && atStart && c > 0) to(0, -1);
  else if (e.key === "ArrowRight" && atEnd && c < last) to(0, 1);
}

async function saveSong() {
  const s = openSong();
  if (!songDirty(s)) return;
  const r = await fetch("/api/music/save", {method: "POST", body: JSON.stringify({name: s.name, old: s.seen, block: songBlock(s)})});
  const body = await r.json();
  if (!r.ok) {
    // Someone else changed it: SAVE again keeps these edits over theirs.
    const fresh = await (await fetch("/api/music")).json();
    const theirs = fresh.songs.find(t => t.name === s.name);
    if (theirs) s.seen = theirs.block;
    return mstatus(body.error + (theirs ? "\nSAVE again to keep yours over it, or REVERT to take the file's." : ""), "bad");
  }
  // Saved: this song starts again from the file, other songs keep their edits.
  mversion = "";
  songs = songs.filter(t => t !== s);
  await loadMusic();
  mstatus(`Saved song ${s.name}. F5 in the game, on the main menu, hears it.`, "good");
}

function revertSong() {
  const s = openSong();
  s.edit = clone(s.base.saved);
  s.seen = s.block;
  if (mopen.part >= s.edit.parts.length) mopen.part = -1;
  showPart();
}

function musicKey(e) {
  const typing = e.target.tagName === "INPUT" && e.target.type !== "range" && e.target.type !== "checkbox" ||
    e.target.tagName === "SELECT";
  if ((e.ctrlKey || e.metaKey) && e.key === "s") { e.preventDefault(); saveSong(); return; }
  if (e.key === "Escape") { stopHearing(); return; }
  if (typing || !mopen) return;
  if (e.key === " ") {
    e.preventDefault();
    const p = openPart();
    if (!p) playSong();
    else if (p.kind === "section") playSection(p);
    else playNote(p, $("mtools").querySelector("input.note")?.value.trim() || "C-4");
  }
}

function switchTab(to) {
  tab = to;
  $("tabSounds").classList.toggle("on", to === "sounds");
  $("tabMusic").classList.toggle("on", to === "music");
  $("list").classList.toggle("hidden", to !== "sounds");
  $("main").classList.toggle("hidden", to !== "sounds");
  $("mlist").classList.toggle("hidden", to !== "music");
  $("mmain").classList.toggle("hidden", to !== "music");
  $("sidenote").textContent = to === "sounds" ? "▶ plays. ↑ ↓ move through them." :
    "▶ plays: the song, a note, a section. Esc stops.";
  stopHearing();
  if (to === "music" && mopen) showPart();
}

$("tabSounds").onclick = () => switchTab("sounds");
$("tabMusic").onclick = () => switchTab("music");
loadMusic();
