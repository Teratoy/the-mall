#!/usr/bin/env python3
"""What the code says about each talk moment, for the lines editor's DIAGRAM tab.

    python3 tools/talk_map.py [moment]     (prints what it found)

Read fresh from the source every time, so new moments show up by themselves:
  groups   the comment above each group of names in talk.h's enum Moment,
           which says what each moment is and who it is said to.
  uses     every place outside talk.h and talk.cpp that names a moment:
           where it is said ("say"), or where the code acts on it having
           been said ("check", such as s.moment == talk::Moment::AskWay).
           Each with its file and line, the function it is in, that
           function's doc and the nearest comment above it.
  follows  moments that follow one another in the code: a check of one on
           the same line as the saying of another (Refuse, then Snubbed),
           or a check of one that calls a function that says others
           (AskWay calls answerWay, which says TellWay or NoIdea).
Moments said through arithmetic on the enum (the *_again ones) have no use
of their own; the tab flags them.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
TALK_H = os.path.join(SRC, "human", "talk.h")
TALK_CPP = os.path.join(SRC, "human", "talk.cpp")
HUMAN_H = os.path.join(SRC, "human", "human.h")

# How far above a use a comment still counts as being about it, in lines.
NEAR = 12

# A moment named in code: talk::Moment::Beg, Moment::Beg where talk::Moment is
# brought in, or M::Beg where "using M = talk::Moment" names it so.
USE = r"\b(?:talk::Moment|Moment{alias})::([A-Z]\w*)"
ALIAS = re.compile(r"using\s+(\w+)\s*=\s*talk::Moment\s*;")
# A moment worked out from the first of a run of them: Moment(int(Moment::ChatHere) + int(k)).
RANGE = re.compile(r"Moment\(\s*int\(\s*$")
# talk.h's helpers that pick a moment out of such a run, by the run's first.
HELPER = re.compile(r"inline Moment (\w+)\(.*?Moment\(int\(Moment::(\w+)\)")
CHECK = re.compile(r"[=!]=\s*$")
# A function's definition starting at column 0: "bool Human::plead(World& world) {".
DEFN = re.compile(r"^[A-Za-z][\w:<>,*& ]*?\b(?:(\w+)::)?(~?\w+)\s*\(")
CALL = re.compile(r"(?:->|\.|\b)(\w+)\s*\(")


def section_names():
    """The moments' names in data/lines.txt, in the enum's order, from talk.cpp."""
    src = open(TALK_CPP, encoding="utf-8").read()
    body = re.search(r"const char\* MOMENTS\[\] = \{(.*?)\};", src, re.S).group(1)
    return re.findall(r'"([^"]*)"', body)


def groups():
    """[(enum name, group doc)] in the enum's order, from talk.h."""
    src = open(TALK_H, encoding="utf-8").read()
    body = re.search(r"enum class Moment \{(.*?)\n\};", src, re.S).group(1)
    out, doc, fresh = [], [], True
    for raw in body.split("\n"):
        row = raw.strip()
        if not row:
            continue
        if row.startswith("//"):
            if not fresh:
                doc, fresh = [], True
            doc.append(row.lstrip("/").strip())
            continue
        fresh = False
        for name in row.split("//")[0].split(","):
            name = name.split("=")[0].strip()
            if name and name != "Count":
                out.append((name, " ".join(doc)))
    return out


def comment_above(rows, at):
    """The block of // comments ending right above row at, as one string."""
    out = []
    i = at - 1
    while i >= 0 and rows[i].strip().startswith("//"):
        out.insert(0, rows[i].strip().lstrip("/").strip())
        i -= 1
    return " ".join(out)


def header_docs():
    """{function name: its /// doc in human.h}, for functions documented there."""
    rows = open(HUMAN_H, encoding="utf-8").read().split("\n")
    out = {}
    for i, raw in enumerate(rows):
        m = re.match(r"\s*(?:[\w:<>,*&]+\s+)+?(\w+)\s*\(", raw)
        if m and not raw.strip().startswith("//"):
            doc = comment_above(rows, i)
            if doc:
                out.setdefault(m.group(1), doc)
    return out


def helpers():
    """{helper name: the first moment of the run it picks from}, from talk.h."""
    return dict(HELPER.findall(open(TALK_H, encoding="utf-8").read()))


def runs(names):
    """{enum name: the names of its group from it on}: the moments a run starting there can be."""
    out = {}
    for i, (enum, doc) in enumerate(names):
        out[enum] = [e for e, d in names[i:] if d == doc]
    return out


def sources():
    for base, _, files in sorted(os.walk(SRC)):
        for name in sorted(files):
            path = os.path.join(base, name)
            if name.endswith((".cpp", ".h")) and path not in (TALK_H, TALK_CPP):
                yield path


def scan(names):
    """(uses, says by function): every use of a moment in the code, and which
    moments each function says."""
    hdocs, picks, run = header_docs(), helpers(), runs(names)
    pick = re.compile(r"\b(" + "|".join(picks) + r")\(") if picks else None
    uses, said_in = [], {}
    for path in sources():
        text = open(path, encoding="utf-8").read()
        rows = text.split("\n")
        rel = os.path.relpath(path, ROOT)
        aliases = "".join("|" + re.escape(a) for a in sorted(set(ALIAS.findall(text))))
        use = re.compile(USE.format(alias=aliases))
        fn, fn_doc, fn_row = "", "", 0
        for n, raw in enumerate(rows):
            m = DEFN.match(raw)
            if m and not raw.rstrip().endswith(";"):
                fn = (m.group(1) + "::" if m.group(1) else "") + m.group(2)
                fn_doc = comment_above(rows, n) or hdocs.get(m.group(2), "")
                fn_row = n
            if raw.lstrip().startswith("//"):
                continue
            found = []
            for u in use.finditer(raw):
                before = raw[:u.start()]
                if RANGE.search(before):
                    found += [(m, "say", u.group(1)) for m in run.get(u.group(1), [u.group(1)])]
                elif CHECK.search(before) or re.search(r"\bint\(\s*$", before):
                    found.append((u.group(1), "check", ""))
                else:
                    found.append((u.group(1), "say", ""))
            for p in pick.finditer(raw) if pick else []:
                first = picks[p.group(1)]
                found += [(m, "say", first) for m in run.get(first, [first])]
            for name, kind, of_run in found:
                if name == "Count":
                    continue
                near, i = "", n
                while i > fn_row and n - i < NEAR:
                    if rows[i - 1].strip().startswith("//"):
                        near = comment_above(rows, i)
                        break
                    i -= 1
                uses.append({
                    "moment": name, "kind": kind, "file": rel, "line": n + 1, "fn": fn,
                    "fnDoc": fn_doc, "near": near, "code": raw.strip(), "run": of_run,
                })
                if kind == "say" and fn:
                    said_in.setdefault(fn.split("::")[-1], set()).add(name)
    return uses, said_in


def follows(uses, said_in):
    """[(from, to, file:line)]: moments the code says because another was said."""
    by_line = {}
    for u in uses:
        by_line.setdefault((u["file"], u["line"]), []).append(u)
    out = []
    for (path, line), here in sorted(by_line.items()):
        checks = [u for u in here if u["kind"] == "check"]
        if not checks:
            continue
        code = here[0]["code"]
        after = code[code.find(")", code.find(checks[-1]["moment"])) + 1:] if ")" in code else ""
        then = {u["moment"] for u in here if u["kind"] == "say"}
        for call in CALL.findall(after):
            then |= said_in.get(call, set())
        for c in checks:
            for t in sorted(then):
                if t != c["moment"]:
                    out.append((c["moment"], t, f"{path}:{line}"))
    return out


def talk_map():
    """{"moments": [...], "follows": [...]}, keyed by the moments' section names."""
    names = groups()
    sections = section_names()
    section = {enum: sections[i] if i < len(sections) else enum.lower() for i, (enum, _) in enumerate(names)}
    uses, said_in = scan(names)
    moments = []
    for enum, doc in names:
        mine = [u for u in uses if u["moment"] == enum]
        moments.append({"name": section[enum], "enum": enum, "group": doc,
                        "uses": [{k: v for k, v in u.items() if k != "moment"} for u in mine]})
    flow = [{"from": section.get(a, a), "to": section.get(b, b), "at": at} for a, b, at in follows(uses, said_in)]
    return {"moments": moments, "follows": flow}


def main():
    m = talk_map()
    if len(sys.argv) > 1:
        one = next((x for x in m["moments"] if x["name"] == sys.argv[1]), None)
        if one is None:
            sys.exit(f"no moment {sys.argv[1]}")
        one["follows"] = [f for f in m["follows"] if sys.argv[1] in (f["from"], f["to"])]
        print(json.dumps(one, indent=1))
        return
    quiet = [x["name"] for x in m["moments"] if not any(u["kind"] == "say" for u in x["uses"])]
    print(f"{len(m['moments'])} moments, {sum(len(x['uses']) for x in m['moments'])} uses, "
          f"{len(m['follows'])} follows")
    for f in m["follows"]:
        print(f"  {f['from']} -> {f['to']}  ({f['at']})")
    print(f"never said by name ({len(quiet)}): " + " ".join(quiet))


if __name__ == "__main__":
    main()
