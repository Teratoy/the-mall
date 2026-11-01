#!/usr/bin/env python3
"""data/talk_flow.txt: what makes each moment get said, what it does, and which
moments can follow it, for the lines editor's DIAGRAM tab.

    python3 tools/talk_flow.py      (prints what the checks find)

The file's head explains its format. The checks, beside what the code shows
(tools/talk_map.py):
  missing     moments the code has with no section in the file
  unknown     sections, or branches, naming moments the code doesn't have
  bad         rows that aren't understood: a who, chance or field it doesn't know
  stranded    moments with no "when" that no branch leads to either
  unwritten   follows the code shows that the file has no branch for
None of them fail make check: the tab shows them, so a new feature is never
held up by the diagram.
"""
import os
import re
import sys

import talk_map

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLOW = os.path.join(ROOT, "data", "talk_flow.txt")

# Who a moment is said to.
WHO = ["someone", "nobody", "shout", "wall"]
# How often a branch is taken, most to least.
CHANCES = ["always", "often", "sometimes", "seldom"]
# The fields a section's rows can hold.
FIELDS = ["when", "does"]


def parse(rows):
    """{"sections": [...], "bad": [...]}: each section with its who, whether
    it is about someone, its whens, its does and its branches."""
    sections, bad, cur = [], [], None
    for n, raw in enumerate(rows):
        row = raw.split("#", 1)[0].strip() if not raw.strip().startswith("#") else ""
        if not row:
            continue
        if row.startswith("["):
            if "]" not in row:
                bad.append({"row": n, "raw": raw, "why": "a section's name needs a closing ]"})
                cur = None
                continue
            name = row[1:row.index("]")].strip()
            words = row[row.index("]") + 1:].split()
            cur = {"name": name, "row": n, "who": "", "about": False, "when": [], "does": [], "branches": []}
            for w in words:
                if w in WHO and not cur["who"]:
                    cur["who"] = w
                elif w == "about":
                    cur["about"] = True
                else:
                    bad.append({"row": n, "raw": raw, "why": f"'{w}' isn't a who: use " + " ".join(WHO) + " or about"})
            sections.append(cur)
            continue
        if cur is None:
            bad.append({"row": n, "raw": raw, "why": "a row before any [section]"})
            continue
        head, _, text = row.partition("|")
        head, text = head.split(), text.strip()
        if head and head[0] == "->":
            if len(head) < 2:
                bad.append({"row": n, "raw": raw, "why": "a branch needs the moment it leads to"})
                continue
            chance = head[2] if len(head) > 2 else ""
            if chance and chance not in CHANCES:
                bad.append({"row": n, "raw": raw, "why": f"'{chance}' isn't a chance: use " + " ".join(CHANCES)})
            cur["branches"].append({"row": n, "to": head[1], "chance": chance, "when": text})
        elif len(head) == 1 and head[0] in FIELDS:
            cur[head[0]].append({"row": n, "text": text})
        else:
            bad.append({"row": n, "raw": raw, "why": "a row needs when |, does | or -> moment chance |"})
    return {"sections": sections, "bad": bad}


def read():
    try:
        rows = open(FLOW, encoding="utf-8").read().split("\n")
    except FileNotFoundError:
        rows = []
    return parse(rows)


def checks(flow, code):
    """What the tab flags, from the file and what the code shows."""
    names = [m["name"] for m in code["moments"]]
    known = set(names)
    have = {s["name"] for s in flow["sections"]}
    led_to = {b["to"] for s in flow["sections"] for b in s["branches"]}
    branches = {(s["name"], b["to"]) for s in flow["sections"] for b in s["branches"]}
    return {
        "missing": [n for n in names if n not in have],
        "unknown": [s["name"] for s in flow["sections"] if s["name"] not in known] +
                   sorted({b["to"] for s in flow["sections"] for b in s["branches"] if b["to"] not in known}),
        "bad": flow["bad"],
        "stranded": [s["name"] for s in flow["sections"] if not s["when"] and s["name"] not in led_to],
        "unwritten": [f for f in code["follows"] if (f["from"], f["to"]) not in branches],
    }


def main():
    flow = read()
    found = checks(flow, talk_map.talk_map())
    print(f"{len(flow['sections'])} sections in {os.path.relpath(FLOW, ROOT)}")
    for name, items in found.items():
        print(f"{name}: {len(items)}")
        for item in items:
            if isinstance(item, dict) and "why" in item:
                print(f"  row {item['row'] + 1}: {item['why']}: {item['raw'].strip()}")
            elif isinstance(item, dict):
                print(f"  {item['from']} -> {item['to']}  ({item['at']})")
            else:
                print(f"  {item}")
    sys.exit(0)


if __name__ == "__main__":
    main()
