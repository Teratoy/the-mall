#!/usr/bin/env python3
"""Grows the letter art of the plant in data/sprites.txt: the electric boiler and the air handling unit. Each is a
box three cells wide, two deep and three tall, drawn the way the pipes and ducts are: the floor seen from above,
anything with height showing its top raised by that height and its south face hanging below it.

- From the front (18x30): its footprint is the bottom two cells, rows 18 to 29; its roof takes rows 0 to 11 and
  its front face rows 12 to 29.
- From its right (12x36), its front toward the right: its footprint is the bottom three cells, rows 18 to 35; its
  roof takes rows 0 to 17 and its side, facing us, rows 18 to 35.

Their outlets are cut from the pipe and duct pieces of tools/conduit_art.py, pixel for pixel, so a piece set
beside one joins it:

- From the front, a run from the south goes into the base of the front face (a piece set below, at the column
  given), and one leaves the east side over the footprint (a piece set against that side, level with it).
- From the side, a run from the south goes into the base of its side face (a piece set below it).

Usage: python3 tools/plant_art.py            prints the group
       python3 tools/plant_art.py --write    puts it in data/sprites.txt as "group Plant", before "group Map"
                                             (hand edits to the plant are lost)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conduit_art  # noqa: E402

PIECE = conduit_art.SIZE
# The front view: its size, and the row its roof gives way to its face.
FRONT_W, FRONT_H, FRONT_FACE = 18, 30, 12
# The side view.
SIDE_W, SIDE_H, SIDE_FACE = 12, 36, 18
# The outlets the sprites in data/sprites.txt are grown with: from the front, a run into the base at this column
# and one out of these sides.
FRONT_ENTRY = 0
FRONT_SIDES = "e"
# How high the base the runs go into stands, in rows up from the bottom.
BASE_H = 8
# A pipe going into the base: the middle of its collar, this far down the piece-high block at the bottom.
PIPE_ENTRY_Y = 7.5


def canvas(w, h):
    return [["."] * w for _ in range(h)]


def rect(g, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= y < len(g) and 0 <= x < len(g[0]):
                g[y][x] = c


def put(g, pts, c):
    for x, y in pts:
        g[y][x] = c


def stamp(g, art, x0, y0, letters):
    """Lays art (rows of letters) with its top-left at (x0, y0), its letters renamed by letters."""
    for j, row in enumerate(art):
        for i, c in enumerate(row):
            if c != "." and 0 <= y0 + j < len(g) and 0 <= x0 + i < len(g[0]):
                g[y0 + j][x0 + i] = letters.get(c, c)


def unbanded(pipe):
    """A pipe piece without its green band, which belongs to the middle of a run."""
    return [[{"a": "p", "A": "P"}.get(c, c) for c in row] for row in pipe]


def plain(duct):
    """A duct piece without its creases, so a cut of it ends cleanly."""
    return [["m" if c in "xX" else c for c in row] for row in duct]


def cut(piece, x0, x1):
    return [row[x0:x1 + 1] for row in piece]


def inks_for(source, letters):
    """The inks of a pipe or duct piece under the names the plant sprite gives its letters."""
    return [(letters[k], v, what) for k, v, what in source if k in letters]


def side_outlets(g, x0, x1, run, letters, sides):
    """Lays the outlets out of the sides given, cut from run, an east-west piece, over the footprint: each with a
    bolted collar where it leaves the box."""
    y = len(g) - PIECE
    if "w" in sides:
        stamp(g, cut(run, 9, 10), 0, y, letters)
        stamp(g, cut(run, 0, 0), x0 - 1, y, letters)
    if "e" in sides:
        stamp(g, cut(run, 0, 2), x1 + 1, y, letters)


def shadow(g, x1, face_top, letter):
    """The box's shadow on the floor east of it, longer the nearer the floor."""
    for y in range(face_top + 2, len(g)):
        for x in range(x1 + 1, min(len(g[0]), x1 + 1 + (2 if y > face_top + 4 else 1))):
            if g[y][x] == ".":
                g[y][x] = letter


def pipe_entry():
    """A pipe from the south going into the base: its rounded top pushing out of a bolted collar low on the face,
    running on toward us to the bottom edge, where the floor piece's flange meets it."""
    g = canvas(PIECE, PIECE)
    centre = (conduit_art.CENTRE, PIPE_ENTRY_Y)
    for y in range(PIECE):
        for x in range(PIECE):
            dx, dy = x + 0.5 - centre[0], y + 0.5 - centre[1]
            d = (dx * dx + dy * dy) ** 0.5
            if conduit_art.RADIUS - 0.2 < d < conduit_art.FLANGE_RADIUS + 0.4:
                g[y][x] = "q" if dx + dy < 0 else "Q"
    for x in (2, 9):
        g[3][x] = "b"
    segs = [(centre, (conduit_art.CENTRE, PIECE + 1.0), False, False)]
    on = [[conduit_art.tube((x + 0.5, y + 0.5), segs, conduit_art.RADIUS) for x in range(PIECE)] for y in range(PIECE)]
    sx, sy = conduit_art.SHADOW
    for y in range(PIECE):
        for x in range(PIECE):
            if on[y][x]:
                g[y][x] = conduit_art.shade(on[y][x])
            elif g[y][x] == "." and x >= sx and y >= sy and on[y - sy][x - sx]:
                g[y][x] = "s"
    return g


def duct_entry():
    """A duct from the south going into the base through a bolted frame: its top leaving the opening and running
    on toward us to the bottom edge, its shadow beside it."""
    g = canvas(PIECE, PIECE)
    cols = conduit_art.DUCT_COLS
    mouth = PIECE - 1 - conduit_art.DUCT_HEIGHT
    rect(g, cols[0] - 1, mouth - 1, cols[-1] + 1, mouth - 1, "g")
    put(g, [(cols[1], mouth - 1), (cols[-2], mouth - 1)], "b")
    for y in range(mouth - 1, PIECE):
        g[y][cols[0] - 1], g[y][cols[-1] + 1] = "g", "G"
    for y in range(mouth, PIECE):
        for x in cols:
            g[y][x] = "L" if y == mouth else "l" if y == mouth + 1 else "e" if x == cols[0] else \
                "E" if x == cols[-1] else "m"
        for dx in (1, 2):
            if cols[-1] + 1 + dx < PIECE:
                g[y][cols[-1] + 1 + dx] = "s"
    return g


# ---------------------------------------------------------------- the electric boiler

# The pipe's letters, renamed so they don't clash with the boiler's own.
PIPE_LETTERS = {"h": "1", "l": "2", "p": "3", "P": "4", "D": "5", "K": "6", "q": "7", "Q": "8", "b": "9", "s": "0"}
BOILER_INKS = [
    ("T", "fbfbf8", "roof"),
    ("t", "d8d8d0", "roof edge"),
    ("Z", "f2f2ec", "enamel lit"),
    ("E", "dcdcd2", "enamel"),
    ("e", "b0b0a8", "enamel shade"),
    ("K", "3c4046", "edge"),
    ("k", "1e2024", "plinth"),
    ("S", "6a7078", "steel"),
    ("s", "a0a6ae", "steel lit"),
    ("R", "c83c3c", "red"),
    ("r", "8a2828", "red shade"),
    ("Y", "f0c828", "warning"),
    ("B", "1a1a1a", "warning print"),
    ("d", "142014", "display"),
    ("G", "60e070", "digits"),
    ("g", "50e070", "power light"),
    ("o", "ffa020", "heating light"),
    ("O", "5a3a10", "heating light, off"),
    ("W", "f0eee6", "dial"),
    ("n", "d02020", "needle"),
    ("v", "2a2e34", "vent slot"),
    ("c", "8a9096", "conduit"),
    ("C", "5a6066", "conduit shade"),
    ("y", "c8a040", "brass"),
    ("w", "d04040", "switch"),
    ("x", "f4f4f4", "shine"),
] + inks_for(conduit_art.PIPE_INKS, PIPE_LETTERS)
GAUGE = [".KKK.", "KWWnK", "KWnWK", "KWWWK", ".KKK."]
BOLT = ["..Y..", ".YBY.", "YYBYY", "YBYYY", "YYYYY"]
# The expansion vessel on the roof, a red drum seen from above, its shadow in 0.
VESSEL = [".rRRr.", "rRxRRr", "RRRRRr", "rRRRrr", ".rrrr0", "..000."]


def boiler_box(g, x0, x1, face):
    """The boiler's box: its roof, with the expansion vessel, the junction box its power comes into down a conduit
    and the relief valve; its enamel face; and the steel base the pipes go into."""
    h = len(g)
    rect(g, x0, 0, x1, face - 1, "T")
    rect(g, x0, 0, x1, 0, "t")
    for y in range(face):
        g[y][x1] = "t"
    stamp(g, VESSEL, x0 + 1, 2, {})
    jx = x1 - 5
    rect(g, jx, face - 7, jx + 4, face - 5, "s")
    rect(g, jx, face - 4, jx + 4, face - 3, "S")
    put(g, [(jx + 1, face - 3), (jx + 3, face - 3)], "y")
    rect(g, jx + 2, 0, jx + 2, face - 8, "c")
    rect(g, jx + 3, 0, jx + 3, face - 8, "C")
    put(g, [(x0 + 2, face - 3), (x0 + 3, face - 3)], "y")
    g[face - 2][x0 + 2] = "S"
    # The face, lit from the left, a red band across its top.
    rect(g, x0, face, x1, h - 1, "E")
    for y in range(face, h):
        g[y][x0], g[y][x1] = "Z", "e"
    rect(g, x0, face, x1, face, "R")
    rect(g, x0, face + 1, x1, face + 1, "r")
    # The base.
    rect(g, x0, h - BASE_H, x1, h - 2, "S")
    rect(g, x0, h - BASE_H, x1, h - BASE_H, "s")
    rect(g, x0, h - 1, x1, h - 1, "k")
    for x in range(x0 + 1, x1, 4):
        g[h - BASE_H + 1][x] = "K"


def base_vents(g, x0, x1, entry, letter):
    """Vent slots in the base, on the side away from a run going into it."""
    h = len(g)
    if entry is None:
        xs = range(x0 + 3, x1 - 2)
    elif entry + PIECE / 2 > (x0 + x1) / 2:
        xs = range(x0 + 1, x0 + 4)
    else:
        xs = range(x1 - 3, x1)
    for y in range(h - BASE_H + 3, h - 2, 2):
        for x in xs:
            g[y][x] = letter


def boiler_front(sides=FRONT_SIDES, front=FRONT_ENTRY):
    g = canvas(FRONT_W, FRONT_H)
    # The box keeps room beside it only for the side outlets.
    x0, x1 = (3 if "w" in sides else 0), (14 if "e" in sides else 15)
    boiler_box(g, x0, x1, FRONT_FACE)
    f = FRONT_FACE
    # Controls: the readout, the power and heating lights, the isolator switch.
    rect(g, x0 + 1, f + 2, x0 + 5, f + 3, "d")
    put(g, [(x0 + 2, f + 2), (x0 + 3, f + 2), (x0 + 5, f + 2)], "G")
    g[f + 2][x0 + 7] = "g"
    g[f + 3][x0 + 7] = "o"
    rect(g, x1 - 2, f + 2, x1 - 1, f + 4, "w")
    g[f + 2][x1 - 2] = "x"
    # Pressure gauge and the high-voltage warning.
    stamp(g, GAUGE, x0 + 1, f + 4, {})
    stamp(g, BOLT, x0 + 6, f + 4, {})
    base_vents(g, x0, x1, front, "v")
    if "e" not in sides:
        shadow(g, x1, f, "0")
    # Flow and return.
    side_outlets(g, x0, x1, unbanded(conduit_art.pipe("ew")), PIPE_LETTERS, sides)
    if front is not None:
        stamp(g, pipe_entry(), front, FRONT_H - PIECE, PIPE_LETTERS)
    return g


def boiler_side():
    g = canvas(SIDE_W, SIDE_H)
    boiler_box(g, 0, SIDE_W - 1, SIDE_FACE)
    f = SIDE_FACE
    # The side: louvres for the elements' heat and the maker's plate.
    for y in (f + 3, f + 5):
        rect(g, 2, y, SIDE_W - 3, y, "v")
    rect(g, 2, f + 7, 5, f + 8, "y")
    stamp(g, pipe_entry(), 0, SIDE_H - PIECE, PIPE_LETTERS)
    return g


# ---------------------------------------------------------------- the air handling unit

DUCT_LETTERS = {"L": "1", "l": "2", "m": "3", "x": "4", "X": "5", "e": "6", "E": "7", "g": "8", "G": "9",
                "b": "0", "f": "f", "F": "F", "K": "Z", "r": "R", "s": "j"}
AHU_INKS = [
    ("P", "cfcbbe", "panel"),
    ("p", "aeaa9c", "panel shade"),
    ("q", "e8e4d8", "panel lit"),
    ("T", "dedad0", "roof"),
    ("t", "b8b4a8", "roof edge"),
    ("A", "8a9096", "frame"),
    ("a", "b8bec4", "frame lit"),
    ("K", "3a3e44", "edge"),
    ("k", "22252a", "rail dark"),
    ("D", "1a1c22", "fan housing"),
    ("x", "c4cad0", "blade"),
    ("X", "40464e", "blade, turned"),
    ("h", "e0e4e8", "hub"),
    ("w", "eceff2", "latch"),
    ("l", "16181c", "louvre gap"),
    ("e", "50e070", "LED"),
    ("r", "f0a030", "LED, amber"),
    ("s", "202428", "screen"),
    ("y", "e8c547", "label"),
    ("Y", "2a2a2a", "label print"),
] + inks_for(conduit_art.DUCT_INKS, DUCT_LETTERS)
# The fan behind its round window: blades in x and X trade colours as it turns.
FAN = [
    "..DDDD..",
    ".DxDDXD.",
    "DXDxXDxD",
    "DDXhhxDD",
    "DDxhhXDD",
    "DxDXxDXD",
    ".DXDDxD.",
    "..DDDD..",
]


def panel(g, x0, y0, x1, y1, latches=()):
    """A removable panel: lit top and left, shaded bottom and right, quarter-turn latches."""
    rect(g, x0, y0, x1, y1, "P")
    rect(g, x0, y0, x1, y0, "q")
    for y in range(y0, y1 + 1):
        g[y][x0] = "q"
        g[y][x1] = "p"
    rect(g, x0 + 1, y1, x1, y1, "p")
    for x, y in latches:
        g[y][x] = "w"


def ahu_box(g, x0, x1, face):
    """The casing: its roof with the exhaust grille and lifting eyes, its framed face, and the steel base the
    ducts go into."""
    h = len(g)
    rect(g, x0, 0, x1, face - 1, "T")
    rect(g, x0, 0, x1, 0, "t")
    for y in range(face):
        g[y][x1] = "t"
    rect(g, x0 + 2, 2, x1 - 3, face - 3, "A")
    for y in range(3, face - 3):
        rect(g, x0 + 3, y, x1 - 4, y, "l" if y % 2 else "a")
    put(g, [(x0 + 1, 1), (x1 - 1, 1), (x0 + 1, face - 2), (x1 - 1, face - 2)], "A")
    rect(g, x0, face, x1, h - 1, "A")
    rect(g, x0, face, x1, face, "a")
    for y in range(face, h):
        g[y][x0], g[y][x1] = "a", "K"
    rect(g, x0, h - BASE_H, x1, h - 2, "A")
    rect(g, x0, h - BASE_H, x1, h - BASE_H, "a")
    rect(g, x0, h - 1, x1, h - 1, "k")
    for x in range(x0 + 1, x1, 4):
        g[h - BASE_H + 1][x] = "K"


def ahu_front(sides=FRONT_SIDES, front=FRONT_ENTRY):
    g = canvas(FRONT_W, FRONT_H)
    # The casing keeps room beside it only for the side outlets.
    x0, x1 = (3 if "w" in sides else 0), (14 if "e" in sides else 15)
    ahu_box(g, x0, x1, FRONT_FACE)
    f = FRONT_FACE
    # The fan behind its window, and beside it the controls and the filter door's label.
    panel(g, x0 + 1, f + 1, x1 - 1, FRONT_H - BASE_H - 1, latches=[(x1 - 1, f + 7)])
    stamp(g, FAN, x0 + 1, f + 1, {})
    rect(g, x0 + 9, f + 2, x0 + 10, f + 3, "s")
    put(g, [(x0 + 9, f + 2)], "e")
    g[f + 3][x0 + 10] = "r"
    rect(g, x0 + 9, f + 5, x0 + 10, f + 6, "y")
    g[f + 5][x0 + 9] = "Y"
    base_vents(g, x0, x1, front, "l")
    if "e" not in sides:
        shadow(g, x1, f, "j")
    # Supply and return.
    side_outlets(g, x0, x1, plain(conduit_art.duct("ew")), DUCT_LETTERS, sides)
    if front is not None:
        stamp(g, duct_entry(), front, FRONT_H - PIECE, DUCT_LETTERS)
    return g


def ahu_side():
    g = canvas(SIDE_W, SIDE_H)
    ahu_box(g, 0, SIDE_W - 1, SIDE_FACE)
    f = SIDE_FACE
    # The long fresh-air intake, louvred.
    panel(g, 1, f + 1, SIDE_W - 2, SIDE_H - BASE_H - 1)
    for y in range(f + 2, SIDE_H - BASE_H - 1):
        rect(g, 2, y, SIDE_W - 3, y, "l" if y % 2 else "a")
    stamp(g, duct_entry(), 0, SIDE_H - PIECE, DUCT_LETTERS)
    return g


# ---------------------------------------------------------------- writing it out

def sprite(name, comment, inks, g, blink=None):
    used = {c for row in g for c in row}
    lines = [f"sprite {name}", *[f"# {c}" for c in comment]]
    lines += [f"ink {k} {v} {what}" for k, v, what in inks if k in used or (blink and k in blink)]
    if blink:
        lines.append(f"blink {blink[0]} {blink[1]}")
    lines += ["".join(row) for row in g]
    return "\n".join(lines) + "\n"


def group():
    box = "A box three cells wide, two deep and three tall: its roof above, its face below, standing on the bottom two."
    front = ("A run from the south goes into the base on the left, and one leaves the east side over the bottom two"
             " cells.")
    side = ("Seen from its right, its front toward the right: three cells deep, standing on the bottom three. A run"
            " from the south goes into the base.")
    return "\n".join([
        "group Plant\n",
        sprite("boiler", ["An electric boiler. " + box,
                          "On the roof, the expansion vessel, the junction box its power comes down into, the relief",
                          "valve; on the face, the readout, the isolator, the gauge, the high-voltage sign. o, the",
                          "heating light, blinks. " + front], BOILER_INKS, boiler_front(), ("o", "O")),
        sprite("boiler side", ["The electric boiler. " + side], BOILER_INKS, boiler_side()),
        sprite("air handler", ["An air handling unit. " + box,
                               "On the roof, the exhaust grille; on the face, the fan behind its window, the controls.",
                               "x and X, the blades, trade colours as it turns. " + front],
               AHU_INKS, ahu_front(), ("x", "X")),
        sprite("air handler side", ["The air handling unit, its louvred intake toward us. " + side],
               AHU_INKS, ahu_side()),
    ]) + "\n"


def write(path="data/sprites.txt"):
    text = open(path).read()
    if "group Plant\n" in text:
        a = text.index("group Plant\n")
        text = text[:a] + text[text.index("\ngroup ", a) + 1:]
    i = text.index("group Map\n")
    open(path, "w").write(text[:i] + group() + text[i:])
    print("wrote the plant to", path)


if __name__ == "__main__":
    if "--write" in sys.argv:
        write()
    else:
        sys.stdout.write(group())
