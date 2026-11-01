#!/usr/bin/env python3
"""Counts which talk moment follows which in soak-like worlds, for the lines
editor's DIAGRAM tab: beside each branch's chance word in data/talk_flow.txt
it shows how often the soak saw it happen. Run by `make talkcounts`, which
first runs build/talk_said (tools/talk_said.cpp) for every line said.

Usage: tools/talk_counts.py SAID_FILE [--args "seeds minutes crowd"]

Each line said is put back to its moment by its words, matched against
data/lines.txt (names filled in match any name). Words that more than one
moment has are left out as unsure, and so are words no moment has (a line
since rewritten).

A moment B follows a moment A when, within WINDOW seconds after A:
  - A was said to someone: B is said between the same two, either way.
  - A was said to nobody: B is said by the same one to nobody, by anyone
    to it, or by anyone else to nobody within NEAR cells of where A was
    said (a cry for help and the one setting off to answer it).
Each moment that follows is counted once per A, so its share is how many
of A's sayings it followed. Writes build/talk_flow_counts.json.
"""
import collections
import datetime
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "talk_flow_counts.json"

# How long after a line another still counts as following it, in world seconds.
WINDOW = 8.0
# How near another's line said to nobody must be to follow a line said to nobody, in cells:
# as far as a call carries (CALL_RADIUS in human_call.cpp).
NEAR = 24


def patterns():
    """[(moment, regex, has a name)] for every line in data/lines.txt."""
    out, cur = [], None
    for raw in (ROOT / "data" / "lines.txt").read_text().splitlines():
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        if s.startswith("["):
            cur = s[1:].split("]")[0].split()[0]
            continue
        if cur is None:
            continue
        words = (s.split("|", 1)[1] if "|" in s else s).split("#")[0].strip().upper()
        out.append((cur, re.compile("^" + re.sub(r"\\\{[A-Z]+\\\}", ".+", re.escape(words)) + "$"), "{" in words))
    return out


def moment_of(words, pats, cache):
    """The one moment these words belong to, or None when none or several do. A line
    word for word beats one with a name filled in, which can match a great deal."""
    if words not in cache:
        exact = {m for m, p, named in pats if not named and p.match(words)}
        found = exact or {m for m, p, named in pats if named and p.match(words)}
        cache[words] = found.pop() if len(found) == 1 else None
    return cache[words]


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    args = sys.argv[sys.argv.index("--args") + 1] if "--args" in sys.argv else ""
    pats, cache = patterns(), {}
    worlds = collections.defaultdict(list)
    unsure = 0
    for raw in pathlib.Path(sys.argv[1]).read_text().splitlines():
        seed, time, speaker, to, x, y, words = raw.split("\t", 6)
        m = moment_of(words, pats, cache)
        if m is None:
            unsure += 1
            continue
        worlds[seed].append((float(time), int(speaker), int(to), int(x), int(y), m))

    said = collections.Counter()
    follows = collections.defaultdict(collections.Counter)
    for lines in worlds.values():
        lines.sort()
        for i, (t, x, y, cx, cy, a) in enumerate(lines):
            said[a] += 1
            seen = set()
            for t2, x2, y2, cx2, cy2, b in lines[i + 1:]:
                if t2 - t > WINDOW:
                    break
                if y >= 0:
                    hit = {x2, y2} == {x, y}
                else:
                    near = max(abs(cx2 - cx), abs(cy2 - cy)) <= NEAR
                    hit = (x2 == x and y2 < 0) or y2 == x or (y2 < 0 and near)
                if hit and b not in seen:
                    seen.add(b)
                    follows[a][b] += 1

    OUT.write_text(json.dumps({
        "made": datetime.datetime.now().isoformat(timespec="minutes"),
        "args": args, "window": WINDOW, "unsure": unsure,
        "said": dict(sorted(said.items())),
        "follows": {a: dict(sorted(c.items())) for a, c in sorted(follows.items())},
    }, indent=1) + "\n")
    total = sum(said.values())
    print(f"{total} lines put to their moment, {unsure} left out as unsure; "
          f"{sum(len(c) for c in follows.values())} pairs that follow. Written to {OUT.relative_to(ROOT)}")


main()
