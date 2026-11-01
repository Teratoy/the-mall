#!/usr/bin/env python3
"""Counts how often each life-log event happens to one human, over the life logs
printed by build/lifelog, to find what happens again and again in one life.
Usage: python3 tools/repeat_survey.py build/repeat-survey/s*.txt"""
import re, sys
from collections import defaultdict

per = defaultdict(lambda: defaultdict(int))  # event -> human -> count
humans = 0
for path in sys.argv[1:]:
    who = None
    for line in open(path, errors="replace"):
        if line.startswith("=="):
            humans += 1
            who = path + line
            continue
        m = re.match(r"\s+[\d.]+\s+(.*?)(?:\s+x(\d+))?$", line.rstrip())
        if not who or not m:
            continue
        what = m.group(1)
        if what.startswith(("HEARD", "SAID")) or " SAID " in what:
            continue
        what = re.sub(r'"[^"]*"', '"..."', what)
        what = re.sub(r"\b[A-Z][a-z]+\b('S)?", "@", what)  # names
        what = re.sub(r"\d+", "N", what)
        per[what][who] += int(m.group(2) or 1)

rows = []
for what, hs in per.items():
    counts = sorted(hs.values())
    rows.append((sum(1 for c in counts if c >= 5), max(counts), len(counts), what))
rows.sort(reverse=True)
print(f"{humans} humans. columns: humans with it 5+ times, most in one life, humans with it at all")
for five, most, n, what in rows:
    if most >= 3:
        print(f"{five:4} {most:5} {n:4}  {what}")
