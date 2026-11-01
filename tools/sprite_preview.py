#!/usr/bin/env python3
"""Draws the sprites of some groups of data/sprites.txt, scaled up, on a floor: each sprite in both blinks,
then, for the air ducts and water pipes, a map of runs with each piece picked by its neighbours, to see that
they meet. Sprites the game has no code for yet can be seen here before the sprite sheet draws them.

Usage (on the host, which has PIL): python3 tools/sprite_preview.py [out.png] [group ...]
    defaults: build/sprite-preview.png, the groups "Air ducts" and "Water pipes"
"""
import sys
from PIL import Image

SCALE = 4
FLOOR = (58, 56, 62, 255)
GAP = 2


def load(path):
    """The sprites of the file, by name: group, inks, blink pair and rows."""
    sprites, group, cur = {}, "", None
    for line in open(path):
        line = line.rstrip("\r\n")
        if line.startswith("group "):
            group = line[6:].strip()
        elif line.startswith("sprite "):
            cur = {"group": group, "inks": {}, "blink": None, "rows": []}
            sprites[line[7:].strip()] = cur
        elif cur is None or line.startswith("#") or not line.strip():
            continue
        elif line.startswith("ink "):
            _, letter, colour = line.split()[:3]
            if len(colour) == 6:
                colour += "ff"
            cur["inks"][letter] = tuple(int(colour[i:i + 2], 16) for i in range(0, 8, 2))
        elif line.startswith("blink "):
            cur["blink"] = tuple(line.split()[1:3])
        else:
            cur["rows"].append(line)
    return sprites


def pixels(s, blinked=False, turns=0):
    """The sprite as rows of colours (None where see-through), blinked, then turned clockwise a quarter each."""
    inks = dict(s["inks"])
    if blinked and s["blink"]:
        a, b = s["blink"]
        inks[a], inks[b] = inks[b], inks[a]
    grid = [[inks.get(c) if c != "." else None for c in row] for row in s["rows"]]
    for _ in range(turns % 4):
        grid = [list(r) for r in zip(*grid[::-1])]
    return grid


def paint(img, grid, x, y):
    for j, row in enumerate(grid):
        for i, c in enumerate(row):
            if c is None:
                continue
            for dy in range(SCALE):
                for dx in range(SCALE):
                    px, py = x + i * SCALE + dx, y + j * SCALE + dy
                    under = img.getpixel((px, py))
                    a = c[3] / 255
                    img.putpixel((px, py), tuple(round(c[k] * a + under[k] * (1 - a)) for k in range(3)) + (255,))


# A map of runs, drawn for ducts and for pipes: '#' a piece, 'v' one bending down into the floor, 'o' a straight
# with something on it (a vent, a valve) and 'L' a leaking pipe. Each picks its sprite by the pieces beside it.
LAYOUT = [
    "v###.#v",
    "...#...",
    "#o###L#",
    "#..#..#",
    "####..v",
    "o...v..",
    "v.#.#..",
]
EXTRAS = {"duct": {"o": "vent", "L": "vent"}, "pipe": {"o": "valve", "L": "leaking"}}
STEPS = {"n": (0, -1), "e": (1, 0), "s": (0, 1), "w": (-1, 0)}


def piece(kind, i, j):
    """The sprite name for the piece at column i, row j of LAYOUT."""
    def at(x, y):
        return 0 <= y < len(LAYOUT) and 0 <= x < len(LAYOUT[y]) and LAYOUT[y][x] != "."
    joins = "".join(s for s, (dx, dy) in STEPS.items() if at(i + dx, j + dy))
    c = LAYOUT[j][i]
    if c == "v":
        return f"{kind} down {joins}"
    if c in EXTRAS[kind]:
        return f"{kind} {joins} {EXTRAS[kind][c]}"
    return kind + (" " + joins if joins else "")


# The plant joined to its runs: (machine, kind of run). From the front a run goes into the base on the left and one
# leaves the east side over the bottom two cells; from the side one goes into the base.
JOINED = [("boiler", "pipe"), ("air handler", "duct")]


def joined(img, sprites, left, top):
    """Each machine of JOINED front and side, with pieces of its run set where they join it."""
    for k, (machine, kind) in enumerate(JOINED):
        x = left + k * 100 * SCALE
        front, side = sprites.get(machine), sprites.get(machine + " side")
        if not front or not side:
            continue
        paint(img, pixels(front), x, top)
        for name, dx, dy in ((kind + " ew", 18, 18), (kind + " down w", 30, 18),
                             (kind + " ns", 0, 30), (kind + " down n", 0, 42)):
            paint(img, pixels(sprites[name]), x + dx * SCALE, top + dy * SCALE)
        sx = x + 50 * SCALE
        paint(img, pixels(side), sx, top)
        paint(img, pixels(sprites[kind + " down n"]), sx, top + 36 * SCALE)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/sprite-preview.png"
    groups = sys.argv[2:] or ["Air ducts", "Water pipes"]
    sprites = load("data/sprites.txt")
    shown = [(n, s) for n, s in sprites.items() if s["group"] in groups]
    box = max(len(s["rows"][0]) for _, s in shown) * SCALE + GAP * SCALE
    tall = max(len(s["rows"]) for _, s in shown) * SCALE + GAP * SCALE
    kinds = [k for k, g in (("duct", "Air ducts"), ("pipe", "Water pipes")) if g in groups]
    per_row = 12
    rows = (len(shown) + per_row - 1) // per_row
    piece_px = 12 * SCALE
    map_w = len(LAYOUT[0]) * piece_px
    width = max(min(per_row, len(shown)) * box, len(kinds) * (map_w + 2 * box)) + GAP * SCALE
    map_h = len(LAYOUT) * piece_px if kinds else 0
    plant = "Plant" in groups
    if plant:
        width = max(width, len(JOINED) * 100 * SCALE)
        map_h += 60 * SCALE
    img = Image.new("RGBA", (width, rows * 2 * tall + 2 * GAP * SCALE + map_h), FLOOR)
    for k, (name, s) in enumerate(shown):
        x = GAP * SCALE + k % per_row * box
        y = GAP * SCALE + k // per_row * 2 * tall
        paint(img, pixels(s), x, y)
        paint(img, pixels(s, blinked=True), x, y + tall)
    top = rows * 2 * tall + 2 * GAP * SCALE
    for side, kind in enumerate(kinds):
        left = GAP * SCALE + side * (map_w + 2 * box)
        for j, row in enumerate(LAYOUT):
            for i, c in enumerate(row):
                if c == ".":
                    continue
                name = piece(kind, i, j)
                if name not in sprites:
                    print("no sprite", name)
                    continue
                paint(img, pixels(sprites[name]), left + i * piece_px, top + j * piece_px)
    if plant:
        joined(img, sprites, GAP * SCALE, top + (len(LAYOUT) * piece_px if kinds else 0))
    img.save(out)
    print("saved", out)


if __name__ == "__main__":
    main()
