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
            cur = {"name": name, "row": n, "raw": raw, "who": "", "about": False, "when": [], "does": [], "branches": []}
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
            cur["branches"].append({"row": n, "raw": raw, "to": head[1], "chance": chance, "when": text})
        elif len(head) == 1 and head[0] in FIELDS:
            cur[head[0]].append({"row": n, "raw": raw, "text": text})
        else:
            bad.append({"row": n, "raw": raw, "why": "a row needs when |, does | or -> moment chance |"})
    return {"sections": sections, "bad": bad}


class Conflict(Exception):
    pass


def field_row(kind, text):
    return f"{kind} | {text}"


def branch_row(to, chance, text):
    return f"-> {to} {chance} | {text}" if chance else f"-> {to} | {text}"


def header_row(name, who, about):
    return " ".join([f"[{name}]"] + ([who] if who else []) + (["about"] if about else []))


def clean(text):
    """A row's words, on one line, without the marks the format reads."""
    text = " ".join(str(text).split())
    if "|" in text or "#" in text:
        raise Conflict("| and # can't be in a row's words")
    return text


def locate(rows, row, raw):
    """The row's index now: where it was, or the one place it moved to."""
    if 0 <= row < len(rows) and rows[row] == raw:
        return row
    found = [i for i, r in enumerate(rows) if r == raw]
    if len(found) == 1:
        return found[0]
    raise Conflict("that row has changed in talk_flow.txt since the page loaded it")


def apply(op, moments):
    """Makes one edit the DIAGRAM tab asks for, re-reading the file first so
    rows another session added meanwhile are kept. moments: the code's names,
    in the enum's order, for new sections and branch targets."""
    try:
        rows = open(FLOW, encoding="utf-8").read().split("\n")
    except FileNotFoundError:
        rows = [""]
    flow = parse(rows)
    section = next((s for s in flow["sections"] if s["name"] == op.get("moment")), None)
    kind = op["op"]

    def branch_of(o):
        if o.get("to") not in moments:
            raise Conflict(f"no moment called '{o.get('to')}'")
        if o.get("chance", "") not in CHANCES + [""]:
            raise Conflict(f"'{o['chance']}' isn't a chance: use " + " ".join(CHANCES))
        return branch_row(o["to"], o.get("chance", ""), clean(o.get("text", "")))

    def made(o):
        if o["kind"] == "branch":
            return branch_of(o)
        if o["kind"] not in FIELDS:
            raise Conflict(f"no row kind called {o['kind']}")
        text = clean(o.get("text", ""))
        if not text:
            raise Conflict("a row needs words")
        return field_row(o["kind"], text)

    if kind == "section":
        if section:
            raise Conflict(f"[{op['moment']}] already has a section")
        if op["moment"] not in moments:
            raise Conflict(f"no moment called '{op['moment']}'")
        # After the section of the nearest moment before it in the enum, or at the end.
        before = moments[:moments.index(op["moment"])]
        have = {s["name"]: s for s in flow["sections"]}
        prev = next((have[m] for m in reversed(before) if m in have), None)
        at = section_end(rows, prev["row"]) + 1 if prev else len(rows)
        while at > 0 and at == len(rows) and rows[at - 1] == "":
            at -= 1
        rows[at:at] = ["", header_row(op["moment"], op.get("who", ""), False)]
    elif section is None:
        raise Conflict(f"no section [{op.get('moment')}] in talk_flow.txt")
    elif kind == "who":
        if op.get("who", "") not in WHO + [""]:
            raise Conflict(f"'{op['who']}' isn't a who: use " + " ".join(WHO))
        at = locate(rows, section["row"], section["raw"])
        rows[at] = header_row(section["name"], op.get("who", ""), bool(op.get("about")))
    elif kind == "add":
        # After the last row of the same kind, keeping whens, then does, then branches.
        order = ["when", "does", "branch"]
        lists = {"when": section["when"], "does": section["does"], "branch": section["branches"]}
        last = section["row"]
        for k in order[:order.index(op["kind"]) + 1]:
            if lists[k]:
                last = lists[k][-1]["row"]
        rows.insert(last + 1, made(op))
    elif kind == "edit":
        at = locate(rows, op["row"], op["raw"])
        rows[at] = made(op)
    elif kind == "delete":
        del rows[locate(rows, op["row"], op["raw"])]
    else:
        raise Conflict(f"no operation called {kind}")
    tmp = FLOW + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(rows))
    os.replace(tmp, FLOW)


def section_end(rows, start):
    """The last row of the section whose header is at start."""
    last = start
    for i in range(start + 1, len(rows)):
        row = rows[i].strip()
        if row.startswith("["):
            break
        if row and not row.startswith("#"):
            last = i
    return last


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
