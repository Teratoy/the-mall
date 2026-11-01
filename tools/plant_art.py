#!/usr/bin/env python3
"""Grows the letter art of the plant in data/sprites.txt: the electric boiler and the air handling unit, each 3
cells wide and 5 tall (18x30), seen from the front and from its right. Their outlets are cut from the pipe and duct
pieces of tools/conduit_art.py, pixel for pixel, so a piece set beside one joins it:

- From the front, an outlet leaves each side over the bottom two cells (rows 18 to 29), the way an east-west piece
  runs: a piece set against either side, level with those two cells, meets it.
- From the side, the outlet on the machine's right comes toward us and leaves by the bottom edge over columns 6 to
  17, the way a north-south piece runs: a piece set below, from the second cell in, meets it.

Usage: python3 tools/plant_art.py            prints the group
       python3 tools/plant_art.py --write    puts it in data/sprites.txt as "group Plant", before "group Map"
                                             (hand edits to the plant are lost)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conduit_art  # noqa: E402

W, H = 18, 30
# The bottom two cells, where the outlets run, start at this row; the side view's outlet at this column.
OUTLET_ROW = 18
OUTLET_COL = 6


def canvas():
    return [["."] * W for _ in range(H)]


def rect(g, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= x < W and 0 <= y < H:
                g[y][x] = c


def put(g, pts, c):
    for x, y in pts:
        g[y][x] = c


def stamp(g, art, x0, y0, letters):
    """Lays art (rows of letters) with its top-left at (x0, y0), its letters renamed by letters."""
    for j, row in enumerate(art):
        for i, c in enumerate(row):
            if c != ".":
                g[y0 + j][x0 + i] = letters.get(c, c)


def unbanded(pipe):
    """A pipe piece without its green band, which belongs to the middle of a run."""
    return [[{"a": "p", "A": "P"}.get(c, c) for c in row] for row in pipe]


def cut(piece, x0, x1, y0=0, y1=11):
    return [row[x0:x1 + 1] for row in piece[y0:y1 + 1]]


def inks_for(source, letters):
    """The inks of a pipe or duct piece under the names the plant sprite gives its letters."""
    return [(letters[k], v, what) for k, v, what in source if k in letters]


# ---------------------------------------------------------------- the electric boiler

# The pipe's letters, renamed so they don't clash with the boiler's own.
PIPE_LETTERS = {"h": "1", "l": "2", "p": "3", "P": "4", "D": "5", "K": "6", "q": "7", "Q": "8", "b": "9", "s": "0"}
BOILER_INKS = [
    ("Z", "f6f6f2", "enamel lit"),
    ("E", "e2e2da", "enamel"),
    ("e", "bcbcb4", "enamel shade"),
    ("K", "3c4046", "edge"),
    ("k", "1e2024", "plinth"),
    ("S", "6a7078", "steel"),
    ("s", "a0a6ae", "steel lit"),
    ("R", "c83c3c", "red band"),
    ("r", "8a2828", "red band shade"),
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
    ("x", "f4f4f4", "switch lit"),
] + inks_for(conduit_art.PIPE_INKS, PIPE_LETTERS)
GAUGE = [".KKK.", "KWWnK", "KWnWK", "KWWWK", ".KKK."]
BOLT = ["..Y..", ".YBY.", "YYBYY", "YBYYY", "YYYYY"]


def cabinet(g, x0, x1):
    """The boiler's steel cabinet, from row 4 to its plinth, with its power coming down in a conduit."""
    # Armoured conduit from the ceiling into a junction box on top, and the relief valve's pipe.
    rect(g, x1 - 4, 0, x1 - 3, 3, "c")
    rect(g, x1 - 3, 0, x1 - 3, 3, "C")
    rect(g, x1 - 6, 2, x1 - 1, 4, "S")
    rect(g, x1 - 6, 2, x1 - 1, 2, "s")
    put(g, [(x1 - 5, 3), (x1 - 2, 3)], "y")
    rect(g, x0 + 2, 1, x0 + 3, 4, "y")
    rect(g, x0 + 1, 1, x0 + 4, 1, "S")
    # The cabinet, lit from the left.
    rect(g, x0, 5, x1, 27, "E")
    for y in range(5, 28):
        g[y][x0], g[y][x0 + 1], g[y][x1] = "Z", "Z", "e"
    rect(g, x0, 5, x1, 5, "Z")
    rect(g, x0, 27, x1, 27, "e")
    rect(g, x0, 7, x1, 7, "R")
    rect(g, x0, 8, x1, 8, "r")
    rect(g, x0, 28, x1, 29, "k")
    rect(g, x0 + 2, 28, x1 - 2, 28, "K")


def boiler_front():
    g = canvas()
    x0, x1 = 3, 14
    cabinet(g, x0, x1)
    # Controls: the readout, the power and heating lights, the isolator switch.
    rect(g, x0 + 2, 10, x0 + 7, 12, "d")
    put(g, [(x0 + 3, 11), (x0 + 4, 11), (x0 + 6, 11)], "G")
    g[10][x0 + 9] = "g"
    g[12][x0 + 9] = "o"
    rect(g, x1 - 2, 10, x1 - 1, 12, "w")
    g[10][x1 - 2] = "x"
    # Pressure gauge and the high-voltage warning.
    stamp(g, GAUGE, x0 + 1, 14, {})
    stamp(g, BOLT, x0 + 6, 14, {})
    # Vents for the heat of the elements.
    for y in (21, 23, 25):
        rect(g, x0 + 3, y, x1 - 3, y, "v")
    # Flow and return: the pipe leaves both sides over the bottom two cells.
    pipe = unbanded(conduit_art.pipe("ew"))
    stamp(g, cut(pipe, 0, 2), 0, OUTLET_ROW, PIPE_LETTERS)
    stamp(g, cut(pipe, 9, 11), x1 + 1, OUTLET_ROW, PIPE_LETTERS)
    return g


def boiler_side():
    g = canvas()
    x0, x1 = 1, 16
    cabinet(g, x0, x1)
    # The side panel: louvres for the elements' heat and the maker's plate.
    for y in (10, 12, 14):
        rect(g, x0 + 3, y, x1 - 3, y, "v")
    rect(g, x0 + 2, 17, x0 + 6, 19, "y")
    # The outlet on its right comes toward us and runs off south, as a north-south pipe.
    stamp(g, unbanded(conduit_art.pipe("ns")), OUTLET_COL, OUTLET_ROW, PIPE_LETTERS)
    return g


# ---------------------------------------------------------------- the air handling unit

DUCT_LETTERS = {"L": "1", "l": "2", "m": "3", "x": "4", "X": "5", "e": "6", "E": "7", "g": "8", "G": "9",
                "b": "0", "f": "f", "F": "F", "K": "Z", "r": "R"}
AHU_INKS = [
    ("P", "cfcbbe", "panel"),
    ("p", "aeaa9c", "panel shade"),
    ("q", "e8e4d8", "panel lit"),
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
    ("c", "6a9ac4", "drain pipe"),
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


def ahu_box(g, x0, x1):
    """The casing with its lifting eyes, frame and base rail."""
    put(g, [(x0 + 2, 2), (x1 - 2, 2)], "A")
    put(g, [(x0 + 1, 3), (x0 + 3, 3), (x1 - 3, 3), (x1 - 1, 3)], "A")
    rect(g, x0, 4, x1, 26, "A")
    rect(g, x0, 4, x1, 4, "a")
    for y in range(4, 27):
        g[y][x0], g[y][x1] = "a", "K"
    rect(g, x0, 27, x1, 28, "k")
    rect(g, x0, 27, x1, 27, "A")
    rect(g, x0, 29, x0 + 1, 29, "k")
    rect(g, x1 - 1, 29, x1, 29, "k")


def plain(duct):
    """A duct piece without its creases, so a cut of it ends cleanly."""
    return [["m" if c in "xX" else c for c in row] for row in duct]


def ahu_front():
    g = canvas()
    x0, x1 = 3, 14
    ahu_box(g, x0, x1)
    # Fan section: the fan behind its window.
    panel(g, x0 + 1, 5, x1 - 1, 14)
    stamp(g, FAN, x0 + 2, 6, {})
    # Controls: a screen, its lights, two buttons.
    panel(g, x0 + 1, 15, x1 - 1, 18)
    rect(g, x0 + 2, 16, x0 + 5, 17, "s")
    put(g, [(x0 + 2, 16), (x0 + 3, 16)], "e")
    g[17][x0 + 5] = "r"
    put(g, [(x1 - 3, 16), (x1 - 2, 16)], "K")
    # Filter and coil door, labelled, with the coil's drain dripping to the floor.
    panel(g, x0 + 1, 19, x1 - 1, 26, latches=[(x1 - 2, 21), (x1 - 2, 24)])
    rect(g, x0 + 3, 21, x1 - 4, 22, "y")
    put(g, [(x0 + 4, 21), (x0 + 6, 21), (x0 + 4, 22), (x0 + 5, 22), (x0 + 7, 22)], "Y")
    rect(g, x0 + 2, 24, x0 + 2, 29, "c")
    # Supply and return: the duct leaves both sides over the bottom two cells.
    duct = plain(conduit_art.duct("ew"))
    stamp(g, cut(duct, 0, 2), 0, OUTLET_ROW, DUCT_LETTERS)
    stamp(g, cut(duct, 9, 11), x1 + 1, OUTLET_ROW, DUCT_LETTERS)
    return g


def ahu_side():
    g = canvas()
    x0, x1 = 0, 17
    ahu_box(g, x0, x1)
    # The long fresh-air intake, louvred.
    panel(g, 1, 5, 16, 14)
    rect(g, 2, 6, 15, 13, "A")
    for y in range(7, 13, 2):
        rect(g, 3, y, 14, y, "l")
    for y in range(6, 14, 2):
        rect(g, 3, y, 14, y, "a")
    # Access door on hinges, with a gauge for the filters.
    panel(g, 1, 15, 16, 26, latches=[(15, 17)])
    put(g, [(1, 16), (1, 19)], "K")
    rect(g, 3, 16, 5, 17, "K")
    g[16][4] = "w"
    # The outlet on its right comes toward us and runs off south, as a north-south duct.
    stamp(g, conduit_art.duct("ns"), OUTLET_COL, OUTLET_ROW, DUCT_LETTERS)
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
    joins_front = "Flow and return leave both sides over the bottom two cells, to join east-west pipes."
    joins_side = "Its outlet runs off the bottom from the second cell in, to join a north-south pipe."
    return "\n".join([
        "group Plant\n",
        sprite("boiler", ["An electric boiler, three cells wide, seen from the front: power in by a conduit from above,",
                          "the readout, the isolator, the gauge, the high-voltage sign. o, the heating light, blinks.",
                          joins_front], BOILER_INKS, boiler_front(), ("o", "O")),
        sprite("boiler side", ["The electric boiler seen from its right, its front toward the right.",
                               joins_side], BOILER_INKS, boiler_side()),
        sprite("air handler", ["An air handling unit, three cells wide, seen from the front: the fan behind its window,",
                               "the controls, the filter door and the coil's drain. x and X, the blades, trade colours as",
                               "it turns. Supply and return leave both sides over the bottom two cells, to join",
                               "east-west ducts."], AHU_INKS, ahu_front(), ("x", "X")),
        sprite("air handler side", ["The air handling unit seen from its right, its front toward the right: the louvred",
                                    "intake and the access door. Its outlet runs off the bottom from the second cell in,",
                                    "to join a north-south duct."], AHU_INKS, ahu_side()),
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
