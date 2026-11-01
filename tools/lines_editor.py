#!/usr/bin/env python3
"""The lines editor: a local web page for rewriting data/lines.txt.

    python3 tools/lines_editor.py [port]      (or: make lines)

then open http://localhost:8765. Standard library only, so it runs on the host.

The page sends small operations (this row, which read so, now reads so). Each
one re-reads the file and changes only that row, keeping its layout, so lines
another session added meanwhile are never overwritten. Moment, condition and
tone names come from src/human/talk.cpp, so new ones show up by themselves.

Beside the lines file it keeps:
  data/lines_review.json   which lines are reviewed, keyed by moment,
                           conditions and words: a line changed or added
                           elsewhere counts as not reviewed.
  data/lines_drafts.json   rewrites Claude drafts, to accept or reject:
      {"drafts": [{"id": "...", "moment": "meet", "old": "HELLO.",
                   "conds": "sociable warm", "text": "...", "note": "..."}]}
      "old" is the words of the line it replaces (null adds a line);
      "conds" left out keeps the old line's; "delete": true proposes
      deleting it.

For the DIAGRAM tab, /api/diagram gives every moment with what the code says
of it (tools/talk_map.py), its lines, reviewed and tones, its say-chance, and
how often the tests said it (build/coverage.json, from make coverage), with
its section of data/talk_flow.txt (tools/talk_flow.py) and that file's checks.
"""
import datetime
import json
import os
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import talk_flow
import talk_map

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINES = os.path.join(ROOT, "data", "lines.txt")
REVIEW = os.path.join(ROOT, "data", "lines_review.json")
DRAFTS = os.path.join(ROOT, "data", "lines_drafts.json")
TALK = os.path.join(ROOT, "src", "human", "talk.cpp")
PAGE = os.path.join(ROOT, "tools", "lines_editor")
MEANINGS = os.path.join(PAGE, "conditions.txt")
COVERAGE = os.path.join(ROOT, "build", "coverage.json")
FLOW = talk_flow.FLOW

# Conditions and tone stand in a column this wide before the bar, as in the file.
COLUMN = 24
SLOTS = {"{NAME}", "{ME}", "{WHO}", "{THING}", "{N}"}

lock = threading.Lock()


def names(array):
    """The strings of one constexpr const char* array in talk.cpp."""
    src = open(TALK, encoding="utf-8").read()
    body = re.search(r"const char\* " + array + r"\[\] = \{(.*?)\};", src, re.S).group(1)
    return re.findall(r'"([^"]*)"', body)


def read_json(path, empty):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return empty


def write(path, text):
    """Writes the whole file at once, so the game never reads half of it."""
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    os.replace(tmp, path)


def write_json(path, data):
    write(path, json.dumps(data, indent=1, ensure_ascii=False, sort_keys=True) + "\n")


def split_row(raw):
    """A line's row as (conditions and tone, words, trailing comment)."""
    comment = ""
    if "#" in raw:
        at = raw.index("#")
        raw, comment = raw[:at], raw[at:]
    if "|" in raw:
        words, text = raw.split("|", 1)
    else:
        words, text = "", raw
    return words.split(), text.strip(), comment


def make_row(conds, text):
    head = " ".join(conds)
    if len(head) < COLUMN:
        return head.ljust(COLUMN) + "| " + text
    return head + " | " + text


def key(moment, conds, text):
    """What a review mark is kept under: words and conditions, in any order."""
    return moment + "|" + " ".join(sorted(conds)) + "|" + text.upper()


def parse(rows, tones):
    """The file as moments, each with its lines, its group (the last "# ---- X"
    heading, or a bare rule's first words) and the comment right above it."""
    moments, group, comments, current = [], "Start", [], None
    for n, raw in enumerate(rows):
        row = raw.strip()
        if not row:
            comments = []
            continue
        if row.startswith("#"):
            m = re.match(r"#\s*-{3,}\s*(.*?)\s*-*\s*$", row)
            if m:
                group, comments, current = m.group(1) or None, [], None
                continue
            text = row.lstrip("#").strip()
            if group is None:
                group = first_words([text])
            comments.append(text)
            continue
        if row.startswith("["):
            head = row[1:row.index("]")].split()
            current = {
                "name": head[0] if head else "",
                "chance": float(head[1]) if len(head) > 1 else None,
                "row": n,
                "doc": comments,
                "group": group or "",
                "lines": [],
            }
            moments.append(current)
            comments = []
            continue
        if current is None:
            continue
        words, text, comment = split_row(raw)
        current["lines"].append({
            "row": n, "raw": raw, "text": text, "comment": comment,
            "tone": next((w for w in words if w in tones), ""),
            "conds": [w for w in words if w not in tones],
            "key": key(current["name"], words, text),
        })
        comments = []
    return moments


def first_words(doc):
    return re.split(r"[:.]", doc[0])[0] if doc else ""


def header(rows):
    out = []
    for raw in rows:
        if not raw.startswith("#"):
            if out:
                break
            continue
        out.append(raw[1:].rstrip()[1:] if raw.startswith("# ") else raw[1:].rstrip())
    return out


def groups_of(rows):
    """Each group heading's opening comment, for the page's help."""
    out, name, doc, started = {}, None, [], False
    for raw in rows:
        row = raw.strip()
        m = re.match(r"#\s*-{3,}\s*(.*?)\s*-*\s*$", row)
        if m:
            name, doc, started = m.group(1), [], True
            continue
        if not started:
            continue
        if row.startswith("["):
            if name is not None or doc:
                key_ = name or first_words(doc)
                out.setdefault(key_, doc)
            name, doc, started = None, [], False
            continue
        if row.startswith("#"):
            doc.append(row.lstrip("#").strip())
        elif not row and doc:
            doc.append("")
    return out


def meanings():
    """What each condition and tone means, in groups, from conditions.txt."""
    groups = []
    for raw in open(MEANINGS, encoding="utf-8"):
        row = raw.strip()
        if row.startswith("##"):
            groups.append({"title": row[2:].strip(), "items": []})
        elif row and not row.startswith("#") and "|" in row and groups:
            name, text = row.split("|", 1)
            groups[-1]["items"].append([name.strip(), text.strip()])
    return groups


def stamp(path):
    try:
        return os.stat(path).st_mtime_ns
    except FileNotFoundError:
        return 0


def version():
    return f"{stamp(LINES)}-{stamp(REVIEW)}-{stamp(DRAFTS)}-{stamp(TALK)}-{stamp(MEANINGS)}-{stamp(COVERAGE)}-{stamp(FLOW)}"


def book():
    rows = open(LINES, encoding="utf-8").read().split("\n")
    tones = names("TONES")
    moments = parse(rows, tones)
    code = names("MOMENTS")
    present = {m["name"] for m in moments}
    drafts = read_json(DRAFTS, {"drafts": []})["drafts"]
    for d in drafts:
        d["found"] = draft_target(moments, d) is not None or not d.get("old")
    return {
        "version": version(),
        "header": header(rows),
        "groups": groups_of(rows),
        "moments": moments,
        "missing": [m for m in code if m not in present],
        "unknown": [m["name"] for m in moments if m["name"] not in code],
        "conds": names("CONDS"),
        "tones": tones,
        "meanings": meanings(),
        "review": read_json(REVIEW, {}),
        "drafts": drafts,
    }


def diagram():
    """Every moment as the DIAGRAM tab draws it: the code's account of it
    beside its lines, and the moments the code says follow it."""
    rows = open(LINES, encoding="utf-8").read().split("\n")
    tones = names("TONES")
    sections = {m["name"]: m for m in parse(rows, tones)}
    review = read_json(REVIEW, {})
    coverage = read_json(COVERAGE, None)
    code = talk_map.talk_map()
    flow = talk_flow.read()
    flows = {f["name"]: f for f in flow["sections"]}
    for m in code["moments"]:
        m["flow"] = flows.get(m["name"])
        section = sections.get(m["name"])
        lines = section["lines"] if section else []
        m["inFile"] = section is not None
        m["chance"] = section["chance"] if section else None
        m["lines"] = len(lines)
        m["reviewed"] = sum(1 for line in lines if line["key"] in review)
        m["tones"] = {t: n for t in tones if (n := sum(1 for line in lines if line["tone"] == t))}
        m["said"] = coverage["said"].get(m["name"]) if coverage else None
    return {
        "version": version(),
        "moments": code["moments"],
        "follows": code["follows"],
        "coverage": coverage["made"] if coverage else None,
        "checks": talk_flow.checks(flow, code),
        "who": talk_flow.WHO,
        "chances": talk_flow.CHANCES,
    }


def draft_target(moments, d):
    """The line a draft would replace, found by its moment and words."""
    old = (d.get("old") or "").upper().strip()
    for m in moments:
        if m["name"] == d["moment"]:
            for line in m["lines"]:
                if line["text"].upper() == old:
                    return line
    return None


class Conflict(Exception):
    pass


def locate(rows, row, raw):
    """The row's index now: where it was, or the one place it moved to."""
    if 0 <= row < len(rows) and rows[row] == raw:
        return row
    found = [i for i, r in enumerate(rows) if r == raw]
    if len(found) == 1:
        return found[0]
    raise Conflict("that line has changed in the file since the page loaded it")


def checked(conds, text, cond_names, tones):
    text = " ".join(text.upper().split())
    if not text:
        raise Conflict("a line needs words")
    if "#" in text or "@" in text or "|" in text:
        raise Conflict("# @ and | can't be in a line's words")
    for slot in re.findall(r"\{[^}]*\}?", text):
        if slot not in SLOTS:
            raise Conflict(f"no name called {slot}: use " + " ".join(sorted(SLOTS)))
    for c in conds:
        if c.lstrip("!") not in cond_names and c not in tones:
            raise Conflict(f"no condition or tone called '{c}'")
    return text


def edited_row(raw, conds, text):
    """The row with new words, and new conditions only if they changed, so a
    rewrite of the words alone keeps the row's own spacing."""
    words, old_text, comment = split_row(raw)
    if sorted(words) == sorted(conds):
        before = raw.split("|", 1)[0] + "| " if "|" in raw.split("#", 1)[0] else ""
        out = before + text
    else:
        out = make_row(conds, text)
    return out + ((" " + comment) if comment else "")


def section_end(rows, start):
    """The last line row of the moment whose header is at start."""
    last = start
    for i in range(start + 1, len(rows)):
        row = rows[i].strip()
        if row.startswith("["):
            break
        if row and not row.startswith("#"):
            last = i
    return last


def find_moment(rows, name):
    for i, raw in enumerate(rows):
        row = raw.strip()
        if row.startswith("[") and row[1:row.index("]")].split()[:1] == [name]:
            return i
    raise Conflict(f"no moment [{name}] in the file")


def mark(review, k, on):
    if on:
        review[k] = datetime.date.today().isoformat()
    else:
        review.pop(k, None)


def apply(op):
    rows = open(LINES, encoding="utf-8").read().split("\n")
    review = read_json(REVIEW, {})
    drafts_doc = read_json(DRAFTS, {"drafts": []})
    cond_names, tones = names("CONDS"), names("TONES")
    kind = op["op"]
    lines_changed = review_changed = drafts_changed = False

    def words_of(o):
        conds = [c for c in o.get("conds", []) if c]
        if o.get("tone") and o["tone"] != "plain":
            conds.append(o["tone"])
        return conds

    def edit(row, raw, moment, conds, text):
        at = locate(rows, row, raw)
        old_words, old_text, _ = split_row(raw)
        text = checked(conds, text, cond_names, tones)
        rows[at] = edited_row(raw, conds, text)
        mark(review, key(moment, old_words, old_text), False)
        mark(review, key(moment, conds, text), True)

    def add(moment, conds, text, after=None):
        text = checked(conds, text, cond_names, tones)
        start = find_moment(rows, moment)
        at = (locate(rows, after["row"], after["raw"]) if after else section_end(rows, start)) + 1
        rows.insert(at, make_row(conds, text))
        mark(review, key(moment, conds, text), True)

    if kind == "edit":
        edit(op["row"], op["raw"], op["moment"], words_of(op), op["text"])
        lines_changed = review_changed = True
    elif kind == "add":
        add(op["moment"], words_of(op), op["text"], op.get("after"))
        lines_changed = review_changed = True
    elif kind == "delete":
        at = locate(rows, op["row"], op["raw"])
        words, text, _ = split_row(op["raw"])
        del rows[at]
        mark(review, key(op["moment"], words, text), False)
        lines_changed = review_changed = True
    elif kind == "chance":
        at = find_moment(rows, op["moment"])
        chance = op.get("chance")
        rows[at] = f"[{op['moment']}{'' if chance in (None, '') else ' ' + str(float(chance))}]"
        lines_changed = True
    elif kind == "review":
        for k in op["keys"]:
            mark(review, k, op["on"])
        review_changed = True
    elif kind in ("accept", "reject"):
        drafts = drafts_doc["drafts"]
        d = next((d for d in drafts if d["id"] == op["id"]), None)
        if d is None:
            raise Conflict("that draft is gone")
        if kind == "accept":
            moments = parse(rows, tones)
            target = draft_target(moments, d)
            if d.get("old") and target is None:
                raise Conflict("the line this draft replaces isn't in the file any more")
            if d.get("delete"):
                del rows[target["row"]]
                mark(review, target["key"], False)
            elif target:
                edit(target["row"], target["raw"], d["moment"], words_of(op), op["text"])
            else:
                add(d["moment"], words_of(op), op["text"])
            lines_changed = review_changed = True
        drafts.remove(d)
        drafts_changed = True
    else:
        raise Conflict(f"no operation called {kind}")

    if lines_changed:
        write(LINES, "\n".join(rows))
    if review_changed:
        write_json(REVIEW, review)
    if drafts_changed:
        write_json(DRAFTS, drafts_doc)


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
        if path == "/api/book":
            with lock:
                return self.send(200, book())
        if path == "/api/diagram":
            with lock:
                return self.send(200, diagram())
        files = {"/": ("index.html", "text/html; charset=utf-8"), "/font.png": ("font.png", "image/png"),
                 "/font.json": ("font.json", "application/json")}
        if path in files:
            name, kind = files[path]
            with open(os.path.join(PAGE, name), "rb") as f:
                return self.send(200, f.read(), kind)
        self.send(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/api/op":
            return self.send(404, {"error": "not found"})
        op = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        try:
            with lock:
                apply(op)
                return self.send(200, book())
        except Conflict as e:
            return self.send(409, {"error": str(e)})

    def log_message(self, *args):
        pass


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"Lines editor on http://localhost:{port}  (Ctrl+C stops it)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
