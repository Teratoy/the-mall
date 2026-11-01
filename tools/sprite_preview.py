#!/usr/bin/env python3
"""Draws the sprites of some groups of data/sprites.txt, scaled up, on a floor: each sprite in both blinks,
then, for the air ducts and water pipes, a layout of runs with the pieces turned and joined up, to see that
they meet. Sprites the game has no code for yet can be seen here before the sprite sheet draws them.

Usage (on the host, which has PIL): python3 tools/sprite_preview.py [out.png] [group ...]
    defaults: build/sprite-preview.png, the groups "Air ducts" and "Water pipes"
"""
import sys
from PIL import Image

SCALE = 8
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


# A layout of runs for one kind, prefix "duct" or "pipe": (piece, quarter turns clockwise) by cell, None
# for bare floor. Straights are drawn west to east, corners east to south, tees west, east and south.
LAYOUT = [
    [("end", 0), ("", 0), ("tee", 0), ("", 0), ("corner", 1), None],
    [None, None, ("", 1), None, ("", 1), None],
    [("corner", 0), ("", 0), ("cross", 0), ("", 0), ("tee", 1), None],
    [("", 1), None, ("", 1), None, ("", 1), None],
    [("corner", 3), ("", 0), ("tee", 2), ("", 0), ("corner", 2), None],
]
EXTRAS = {"duct": [("grille", 2, 5)], "pipe": [("valve", 0, 1), ("leaking", 0, 3)]}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/sprite-preview.png"
    groups = sys.argv[2:] or ["Air ducts", "Water pipes"]
    sprites = load("data/sprites.txt")
    shown = [(n, s) for n, s in sprites.items() if s["group"] in groups]
    cell = 6 * SCALE
    width = max(len(shown) * (cell + GAP * SCALE), 2 * 7 * cell) + GAP * SCALE
    img = Image.new("RGBA", (width, 2 * cell + 3 * GAP * SCALE + 6 * cell), FLOOR)
    for k, (name, s) in enumerate(shown):
        x = GAP * SCALE + k * (cell + GAP * SCALE)
        w = len(s["rows"][0]) * SCALE
        paint(img, pixels(s), x + (cell - w) // 2, GAP * SCALE)
        paint(img, pixels(s, blinked=True), x + (cell - w) // 2, 2 * GAP * SCALE + cell)
    top = 3 * GAP * SCALE + 2 * cell
    for side, kind in enumerate(["duct", "pipe"]):
        left = GAP * SCALE + side * 7 * cell
        for j, row in enumerate(LAYOUT):
            for i, piece in enumerate(row):
                name = kind + (" " + piece[0] if piece and piece[0] else "")
                if piece and name in sprites:
                    paint(img, pixels(sprites[name], turns=piece[1]), left + i * cell, top + j * cell)
        for piece, turns, i in EXTRAS[kind]:
            name = kind + " " + piece
            if name in sprites:
                paint(img, pixels(sprites[name], turns=turns), left + i * cell, top + 5 * cell)
    img.save(out)
    print("saved", out)


if __name__ == "__main__":
    main()
