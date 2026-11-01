#!/usr/bin/env python3
"""The sound editor: a local web page for tweaking the synth sounds in data/sounds.txt.

    python3 tools/sound_editor.py [port]      (or: make sounds, or ./sounds.sh)

then open http://localhost:8767. Standard library only, so it runs on the host.
F5 in the game reads the file again. See AI/sound_editor_plan.md.

Each sound is heard as the game makes it: the page sends the sound's text and
the server renders it with the game's own synth, build/sound_sheet --one
(tools/sound_sheet.cpp, no raylib, so it runs on the host), and sends back a
.wav. sounds.sh builds it in the toolbox first.

The page saves one sound at a time: its name, its block as the page last saw
it, and the new block. The server re-reads the file and replaces only that
block (the sound line and its layer lines), keeping the comments, so sounds
another session changed meanwhile are never overwritten. If the block itself
changed underneath, it refuses; the page keeps its edits, and SAVE again puts
them over the file's, REVERT takes the file's. A block whose lines the synth
can't make out is refused too. No sounds are added or renamed: the game
plays them by name.
"""
import json
import os
import re
import struct
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOUNDS = os.path.join(ROOT, "data", "sounds.txt")
PAGE = os.path.join(ROOT, "tools", "sound_editor")
RENDER = os.path.join(ROOT, "build", "sound_sheet")

WAVES = ("square", "triangle", "saw", "sine", "noise")
# A layer's keys and their defaults, as synth::Layer has them.
LAYER_KEYS = {"gain": 0.5, "delay": 0.0, "attack": 0.002, "hold": 0.0, "decay": 0.1, "lowpass": 0.0,
              "highpass": 0.0, "duty": 0.5, "repeat": 1, "gap": 0.1, "fade": 1.0, "wobble": 0.0}
# The crush before any crush line, as synth::Def has it.
CRUSH = (5, 8000.0)
SECTION = re.compile(r"^#\s*---\s*(.*?)\.?\s*$")

lock = threading.Lock()


class Refused(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def write(path, text):
    """Writes the whole file at once, so the game never reads half of it."""
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    os.replace(tmp, path)


def version():
    st = os.stat(SOUNDS)
    return f"{st.st_mtime_ns}-{st.st_size}"


def read_lines():
    with open(SOUNDS, encoding="utf-8") as f:
        return f.read().split("\n")


def words_of(line):
    return line.split("#", 1)[0].split()


def number(text):
    v = float(text)
    return int(v) if v == int(v) and "." not in text else v


def parse(lines):
    """Every sound: its name, section, the comment lines just above it, the
    crush it inherits, its own options and layers, and where its block runs
    (the sound line and the layer lines under it)."""
    sounds, section, crush, notes, cur = [], "", CRUSH, [], None
    for i, line in enumerate(lines):
        stripped = line.strip()
        words = words_of(line)
        if not words:
            if cur:
                cur = None
            if stripped.startswith("#"):
                m = SECTION.match(stripped)
                if m:
                    section, notes = m.group(1).split(". ")[0], []
                else:
                    notes.append(stripped.lstrip("#").strip())
            else:
                notes = []
            continue
        head = words[0]
        if head == "crush":
            crush = (int(words[1]), float(words[2]))
            cur, notes = None, []
            continue
        if head == "sound":
            opts = {}
            for k, v in zip(words[2::2], words[3::2]):
                opts[k] = number(v)
            cur = {"name": words[1], "section": section, "notes": [n for n in notes if n],
                   "crush": {"bits": crush[0], "rate": crush[1]}, "options": opts, "layers": [],
                   "start": i, "end": i + 1}
            sounds.append(cur)
            notes = []
            continue
        if cur and head in WAVES:
            layer = {"wave": head, "from": number(words[1]), "keys": {}}
            rest = words[2:]
            if rest:
                try:
                    layer["to"] = number(rest[0])
                    rest = rest[1:]
                except ValueError:
                    pass
            for k, v in zip(rest[0::2], rest[1::2]):
                layer["keys"][k] = number(v)
            cur["layers"].append(layer)
            cur["end"] = i + 1
    for s in sounds:
        s["block"] = "\n".join(lines[s["start"]:s["end"]])
    return sounds


def sheet():
    lines = read_lines()
    sounds = parse(lines)
    for s in sounds:
        del s["start"], s["end"]
    return {"version": version(), "sounds": sounds, "layerKeys": LAYER_KEYS, "waves": WAVES,
            "renderer": os.path.exists(RENDER)}


def render(block, crush, lowpass):
    """The block as a .wav, made by the game's synth, and what it complained of."""
    if not os.path.exists(RENDER):
        raise Refused(503, "build/sound_sheet is missing: run ./sounds.sh, which builds it in the toolbox")
    text = f"crush {int(crush['bits'])} {crush['rate']}\n{block}\n"
    run = subprocess.run([RENDER, "--one", str(float(lowpass))], input=text.encode(), capture_output=True,
                         timeout=10)
    if run.returncode != 0:
        raise Refused(400, run.stderr.decode().strip() or "the synth made nothing")
    return run.stdout, run.stderr.decode().strip()


def loudness(wav):
    """The peak of the samples as the synth made them, 0 to 1, and how many
    sit at full scale, cut off: the browser plays the sound resampled, which
    hides both."""
    data = wav[44:]
    samples = struct.unpack(f"<{len(data) // 2}h", data[:len(data) // 2 * 2])
    peak = max((abs(v) for v in samples), default=0)
    return {"X-Peak": f"{peak / 32767:.3f}", "X-Clipped": str(sum(abs(v) >= 32767 for v in samples))}


def save(op):
    lines = read_lines()
    found = [s for s in parse(lines) if s["name"] == op["name"]]
    if not found:
        raise Refused(409, f"{op['name']} is no longer in data/sounds.txt")
    s = found[0]
    if s["block"] != op["old"]:
        raise Refused(409, f"{op['name']} changed in data/sounds.txt meanwhile")
    new = op["block"].rstrip("\n").split("\n")
    if not new or words_of(new[0])[:2] != ["sound", op["name"]]:
        raise Refused(400, "the block must start with its own sound line")
    _, complaints = render("\n".join(new), s["crush"], 0)
    if complaints:
        raise Refused(400, complaints)
    lines[s["start"]:s["end"]] = new
    write(SOUNDS, "\n".join(lines))


class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, kind="application/json", headers=None):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", kind)
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/api/version":
            return self.send(200, {"version": version()})
        if path == "/api/sounds":
            with lock:
                return self.send(200, sheet())
        if path == "/":
            with open(os.path.join(PAGE, "index.html"), "rb") as f:
                return self.send(200, f.read(), "text/html; charset=utf-8")
        self.send(404, {"error": "not found"})

    def do_POST(self):
        op = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        try:
            if self.path == "/api/render":
                wav, complaints = render(op["block"], op["crush"], op.get("lowpass", 0))
                if complaints:
                    raise Refused(400, complaints)
                return self.send(200, wav, "audio/wav", loudness(wav))
            if self.path == "/api/save":
                with lock:
                    save(op)
                    return self.send(200, sheet())
            return self.send(404, {"error": "not found"})
        except Refused as e:
            return self.send(e.code, {"error": str(e)})

    def log_message(self, *args):
        pass


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8767
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"Sound editor on http://localhost:{port}  (Ctrl+C stops it)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
