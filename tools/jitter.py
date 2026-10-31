#!/usr/bin/env python3
"""Counts how often the followed human's head turns back on screen, frame to frame.

Reads frames saved by `build/shot out.png --frames N` (out_000.png, ...), finds
the skin pixels nearest the middle of the view, and tracks the top-left of
them. A walker the camera follows smoothly moves one way or holds still; a
step back the other way, one frame after a step forward, is a jitter.

Usage: tools/jitter.py build/camcheck/f   (the prefix before _000.png)
"""
import glob
import sys

import numpy as np
from PIL import Image

SKIN = [93, 84, 78]

prefix = sys.argv[1]
frames = sorted(glob.glob(prefix + "_[0-9][0-9][0-9].png"))
pos = []
for f in frames:
    a = np.asarray(Image.open(f).convert("RGB")).astype(int)
    h, w = a.shape[:2]
    ys, xs = np.nonzero((a == SKIN).all(-1))
    near = (abs(xs - w // 2) < 60) & (abs(ys - h // 2) < 120)
    pos.append((int(xs[near].min()), int(ys[near].min())) if near.any() else None)
flips = moves = 0
for i in range(2, len(pos)):
    if None in (pos[i - 2], pos[i - 1], pos[i]):
        continue
    for k in (0, 1):
        d1, d2 = pos[i - 1][k] - pos[i - 2][k], pos[i][k] - pos[i - 1][k]
        moves += d2 != 0
        flips += d1 * d2 < 0
print(f"{len(frames)} frames, {moves} moves on screen, {flips} turned straight back")
