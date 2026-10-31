#!/usr/bin/env python3
"""The sprite editor: a local web page for redrawing data/sprites.txt.

    python3 tools/sprite_editor.py [port]      (or: make sprites, or ./sprites.sh)

then open http://localhost:8766. Standard library only, so it runs on the host.
F5 in the game reads the file again.

The page sends one sprite at a time: its name, its block as the page last saw
it, and its new inks and rows. The server re-reads the file and replaces only
that block, keeping its comments, so sprites another session changed meanwhile
are never overwritten. If the block itself changed underneath, it refuses and
the page reloads. A sprite keeps its size: the game draws each at a fixed one.

tools/sprite_editor/ref.json and human.png, written by build/sprite_sheet --ref
(make spriteref), give the previews a human for size and where each machine
holds its goods.
"""
import json
import os
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPRITES = os.path.join(ROOT, "data", "sprites.txt")
PAGE = os.path.join(ROOT, "tools", "sprite_editor")

COLOUR = re.compile(r"^[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$")

lock = threading.Lock()


class Conflict(Exception):
    pass


def write(path, text):
    """Writes the whole file at once, so the game never reads half of it."""
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    os.replace(tmp, path)


def version():
    st = os.stat(SPRITES)
    return f"{st.st_mtime_ns}-{st.st_size}"


def is_comment(row):
    return row == "#" or row.startswith("# ")


def parse(lines):
    """Every sprite block: where it starts and ends, its group, comments, inks,
    blinking letters and rows. A block runs from its sprite line to the next
    sprite or group line, its trailing blank lines left out."""
    sprites, group, cur = [], "", None

    def close(end):
        if cur is None:
            return
        while end > cur["start"] + 1 and not lines[end - 1].strip():
            end -= 1
        cur["end"] = end
        cur["raw"] = "\n".join(lines[cur["start"]:end])
        sprites.append(cur)

    for n, line in enumerate(lines):
        row = line.strip()
        word = row.split(" ", 1)[0] if " " in row else ""
        if word in ("sprite", "group"):
            close(n)
            cur = None
            if word == "group":
                group = row[6:].strip()
            else:
                cur = {"name": row[7:].strip(), "group": group, "start": n, "comments": [], "inks": [],
                       "blink": None, "rows": []}
            continue
        if cur is None or not row:
            continue
        if is_comment(row):
            cur["comments"].append(row[2:])
        elif " " not in row:
            cur["rows"].append(row)
        elif word == "ink":
            parts = row.split(" ", 3)
            if len(parts) >= 3:
                cur["inks"].append({"ch": parts[1], "hex": parts[2].lower(), "name": parts[3] if len(parts) > 3 else ""})
        elif word == "blink":
            parts = row.split()
            if len(parts) == 3:
                cur["blink"] = [parts[1], parts[2]]
    close(len(lines))
    return sprites


def sheet():
    text = open(SPRITES, encoding="utf-8").read()
    return {"version": version(), "sprites": parse(text.split("\n"))}


def block(name, comments, inks, blink, rows):
    out = [f"sprite {name}"] + ["# " + c if c else "#" for c in comments]
    for ink in inks:
        out.append(f"ink {ink['ch']} {ink['hex']} {ink.get('name', '')}".rstrip())
    if blink:
        out.append(f"blink {blink[0]} {blink[1]}")
    return out + rows


def save(op):
    lines = open(SPRITES, encoding="utf-8").read().split("\n")
    found = next((s for s in parse(lines) if s["name"] == op["name"]), None)
    if found is None:
        raise Conflict(f"no sprite {op['name']} in the file any more")
    if found["raw"] != op["raw"]:
        raise Conflict(f"{op['name']} was changed by someone else meanwhile")
    rows, inks = op["rows"], op["inks"]
    if len(rows) != len(found["rows"]) or any(len(r) != len(found["rows"][0]) for r in rows):
        raise Conflict("a sprite keeps its size")
    letters = set()
    for ink in inks:
        ch = ink["ch"]
        if len(ch) != 1 or ch == "." or not "!" <= ch <= "~":
            raise Conflict(f"can't use {ch!r} as an ink letter")
        if not COLOUR.match(ink["hex"]):
            raise Conflict(f"{ink['hex']} isn't a colour (rrggbb or rrggbbaa)")
        if ch in letters:
            raise Conflict(f"two inks for {ch}")
        if "\n" in ink.get("name", ""):
            raise Conflict("an ink's name is one line")
        letters.add(ch)
    for r in rows:
        for ch in r:
            if ch != "." and ch not in letters:
                raise Conflict(f"letter {ch} has no ink")
    new = block(op["name"], found["comments"], inks, found["blink"], rows)
    lines[found["start"]:found["end"]] = new
    write(SPRITES, "\n".join(lines))


class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, kind="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/api/version":
            return self.send(200, {"version": version()})
        if path == "/api/sprites":
            with lock:
                return self.send(200, sheet())
        files = {"/": ("index.html", "text/html; charset=utf-8"), "/human.png": ("human.png", "image/png"),
                 "/ref.json": ("ref.json", "application/json")}
        if path in files:
            name, kind = files[path]
            try:
                with open(os.path.join(PAGE, name), "rb") as f:
                    return self.send(200, f.read(), kind)
            except FileNotFoundError:
                pass
        self.send(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/api/save":
            return self.send(404, {"error": "not found"})
        op = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        try:
            with lock:
                save(op)
                return self.send(200, sheet())
        except Conflict as e:
            return self.send(409, {"error": str(e)})

    def log_message(self, *args):
        pass


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8766
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"Sprite editor on http://localhost:{port}  (Ctrl+C stops it)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
