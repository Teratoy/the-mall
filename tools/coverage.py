#!/usr/bin/env python3
"""What the tests never got to: log lines never written and talk moments
never said. Run by `make coverage`, which first runs the soak and the
scenarios with LOGLINES set.

Usage: tools/coverage.py LOGLINES_FILE...   (their lines are pooled)

A log line counts as written when some line in the soak's log contains one
of the string literals in a note(...) call (ternaries give one literal per
outcome, so each is checked on its own). A talk moment counts as said when
some SAID line in the log matches one of its lines in data/lines.txt.
Anything listed is code no soak exercised: a bug there would go unseen.

It also writes build/coverage.json, which the lines editor's DIAGRAM tab reads:
how many times each moment was said, and when the count was made.
"""
import datetime
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def note_literals():
    """(file, line, literal) for every string literal inside a note(...) call."""
    out = []
    for path in sorted((ROOT / "src").rglob("*.cpp")):
        text = path.read_text()
        for m in re.finditer(r"\bnote\(", text):
            # The argument list, up to the matching parenthesis, strings skipped.
            i, depth, args = m.end(), 1, []
            while i < len(text) and depth:
                c = text[i]
                if c == '"':
                    j = i + 1
                    while text[j] != '"':
                        j += 2 if text[j] == "\\" else 1
                    args.append(text[i + 1 : j])
                    i = j
                elif c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                i += 1
            line = text.count("\n", 0, m.start()) + 1
            for lit in args:
                if len(lit.strip()) >= 3:
                    out.append((path.relative_to(ROOT), line, lit))
    return out


def moments():
    """{moment: [regex of each of its lines]} from data/lines.txt."""
    book, cur = {}, None
    for raw in (ROOT / "data" / "lines.txt").read_text().splitlines():
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        if s.startswith("["):
            cur = s[1:].split("]")[0].split()[0]
            book.setdefault(cur, [])
            continue
        if cur is None:
            continue
        words = s.split("|", 1)[1].strip() if "|" in s else s
        pat = re.sub(r"\\\{[A-Z]+\\\}", ".+", re.escape(words))
        book[cur].append(re.compile("^" + pat + "$"))
    return book


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    logged = []
    for path in sys.argv[1:]:
        for raw in pathlib.Path(path).read_text().splitlines():
            n, what = raw.split("\t", 1)
            logged.append(what)

    lits = note_literals()
    unseen = [(f, ln, lit) for f, ln, lit in lits if not any(lit.strip() in w for w in logged)]
    print(f"log lines: {len(lits) - len(unseen)} of {len(lits)} written")
    for f, ln, lit in unseen:
        print(f"  never: {f}:{ln}  {lit.strip()}")

    said = [m.group(1) for w in logged if (m := re.match(r'^SAID "(.*)"( TO @)?$', w))]
    book = moments()
    times = {k: sum(1 for t in said if any(p.match(t) for p in pats)) for k, pats in book.items()}
    quiet = [k for k in book if not times[k]]
    print(f"talk moments: {len(book) - len(quiet)} of {len(book)} said")
    for k in quiet:
        print(f"  never said: [{k}]")

    out = ROOT / "build" / "coverage.json"
    out.write_text(json.dumps({"made": datetime.datetime.now().isoformat(timespec="minutes"), "said": times},
                              indent=1, sort_keys=True) + "\n")


main()
