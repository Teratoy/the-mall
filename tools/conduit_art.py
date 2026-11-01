#!/usr/bin/env python3
"""Grows the letter art of the air ducts and water pipes in data/sprites.txt: one 12x12 sprite (2x2 cells) for
every way a piece can join its neighbours, lit from the north-west, so a run reads right without turning
anything; the pieces bending down into the floor; and a few with something on them (vents, valves, leaks).

A piece is named by the sides it joins, in the order n e s w: "duct ew" runs west to east, "pipe nes" is a tee,
"duct" alone joins nothing. "duct down e" comes in from the east and bends down into the floor.

Usage: python3 tools/conduit_art.py            prints the two groups
       python3 tools/conduit_art.py --write    puts them in data/sprites.txt, from "group Air ducts" up to the
                                               next group after "group Water pipes" (hand edits there are lost)
"""
import math
import sys

SIZE = 12
SIDES = "nesw"
CENTRE = 5.5

# --- Air ducts: boxes of galvanised sheet, 8 px across and 3 high, seen from above and the south. A run shows its
# top raised by its height, its front face hanging below where it faces south, and a shadow to the south-east.

DUCT_INKS = [
    ("L", "e8ecee", "lip, lit"),
    ("l", "c4cacf", "metal lit"),
    ("m", "a6adb3", "metal"),
    ("x", "bac1c6", "crease lit"),
    ("X", "959ca2", "crease shade"),
    ("e", "8a9298", "west edge"),
    ("E", "6c737a", "east edge"),
    ("g", "d6dce0", "flange lit"),
    ("G", "7e868c", "flange shade"),
    ("b", "4a5056", "bolt"),
    ("f", "8c9399", "face, top"),
    ("F", "70777d", "face"),
    ("K", "4a4f55", "face, foot"),
    ("r", "b6bcc2", "rivet"),
    ("d", "858c92", "bending down"),
    ("D", "62686e", "bending down, far"),
    ("c", "5c6064", "floor collar"),
    ("C", "3a3d40", "floor collar shade"),
    ("n", "9aa0a6", "collar bolt"),
    ("o", "141618", "hole"),
    ("s", "00000050", "shadow"),
]
VENT_INKS = [("v", "16181c", "vent dark"), ("V", "2c3036", "vent dark, stirred"), ("w", "d0d6da", "vent slat")]
# The top of a west-east run takes these rows, its front face the next DUCT_HEIGHT; a north-south run's top
# takes these columns. Where they cross is the middle of the piece.
DUCT_ROWS = range(0, 8)
DUCT_COLS = range(1, 9)
DUCT_HEIGHT = 3
# The shadow falls this far east and south of the duct.
DUCT_SHADOW = ((1, 1), (2, 1))
# Room round the piece, so arms that join reach past its edges and cast their shadows into it.
PAD = 3


def blank():
    return [["."] * SIZE for _ in range(SIZE)]


def duct_body(joins, middle=True):
    """The top and front face of a duct joining the given sides, as two sets of (x, y), on a canvas reaching PAD
    past the piece; the arms that join run on past the edge."""
    top = set()
    lo, hi = -PAD, SIZE + PAD
    if middle:
        top |= {(x, y) for x in DUCT_COLS for y in DUCT_ROWS}
    if "e" in joins:
        top |= {(x, y) for x in range(DUCT_COLS[-1] + 1, hi) for y in DUCT_ROWS}
    if "w" in joins:
        top |= {(x, y) for x in range(lo, DUCT_COLS[0]) for y in DUCT_ROWS}
    if "s" in joins:
        top |= {(x, y) for x in DUCT_COLS for y in range(DUCT_ROWS[-1] + 1, hi)}
    if "n" in joins:
        top |= {(x, y) for x in DUCT_COLS for y in range(lo, DUCT_ROWS[0])}
    face = set()
    for x, y in top:
        if (x, y + 1) not in top:
            face |= {(x, y + k) for k in range(1, DUCT_HEIGHT + 1)}
    return top, face - top


def duct(joins):
    """The duct piece joining the given sides."""
    n, e, s, w = (side in joins for side in SIDES)
    top, face = duct_body(joins)
    g = blank()

    def inside(x, y):
        return 0 <= x < SIZE and 0 <= y < SIZE

    # The shadow first, on the floor, then the duct over it.
    solid = top | face
    for x, y in solid:
        for dx, dy in DUCT_SHADOW:
            p = (x + dx, y + dy)
            if p not in solid and inside(*p):
                g[p[1]][p[0]] = "s"
    for x, y in top:
        if inside(x, y):
            if (x, y - 1) not in top:
                g[y][x] = "L"
            elif (x, y - 2) not in top:
                g[y][x] = "l"
            elif (x - 1, y) not in top:
                g[y][x] = "e"
            elif (x + 1, y) not in top:
                g[y][x] = "E"
            else:
                g[y][x] = "m"
    for x, y in face:
        if inside(x, y):
            depth = next(j for j in range(1, DUCT_HEIGHT + 1) if (x, y - j) in top)
            g[y][x] = "fFK"[depth - 1]
            if depth == 2 and x % 3 == 1:
                g[y][x] = "r"
    # Round off the top's outer corners.
    for x, y in top:
        if inside(x, y) and (x, y - 1) not in top and ((x - 1, y) not in top or (x + 1, y) not in top):
            g[y][x] = "l"
    # The flange where this piece meets the one before it, bolted.
    if w:
        for y in range(SIZE):
            if g[y][0] in "LlmeE":
                g[y][0] = "g"
            elif g[y][0] in "fFrK":
                g[y][0] = "G"
        g[DUCT_ROWS[1]][0] = g[DUCT_ROWS[-1] - 1][0] = "b"
    if n:
        for x in DUCT_COLS:
            g[0][x] = "g"
            g[1][x] = "G"
        g[0][DUCT_COLS[1]] = g[0][DUCT_COLS[-2]] = "b"
    # On a straight run the sheet is creased in an X to stiffen it; where runs meet, an inspection plate.
    if joins in ("ew", "ns"):
        if joins == "ew":
            x0, x1, y0, y1 = (1 if w else 0), SIZE - 1, DUCT_ROWS[2], DUCT_ROWS[-1]
        else:
            x0, x1, y0, y1 = DUCT_COLS[1], DUCT_COLS[-2], 2, SIZE - 1
        span = x1 - x0
        for i in range(span + 1):
            ya = y0 + round(i * (y1 - y0) / span)
            yb = y1 - round(i * (y1 - y0) / span)
            if g[ya][x0 + i] == "m":
                g[ya][x0 + i] = "x"
            if g[yb][x0 + i] == "m":
                g[yb][x0 + i] = "X"
    elif len(joins) >= 2 or not joins:
        px0, py0, px1, py1 = DUCT_COLS[1] + 1, DUCT_ROWS[2], DUCT_COLS[-2] - 1, DUCT_ROWS[-2]
        for y in range(py0, py1 + 1):
            for x in range(px0, px1 + 1):
                if y in (py0, py1) or x in (px0, px1):
                    g[y][x] = "g" if (y == py0 or x == px0) else "G"
        for x, y in ((px0, py0), (px1, py0), (px0, py1), (px1, py1)):
            g[y][x] = "b"
    return g


def duct_down(side):
    """The duct coming in from one side and bending down into the floor, seen as everything is, from above and the
    south: its top rounds over and drops away at the bend, its face runs on to the floor, and a thin bolted rim
    on the floor rings where it goes in, a dark gap inside it."""
    g = duct(side)
    rows, cols, h = DUCT_ROWS, DUCT_COLS, DUCT_HEIGHT
    floor = rows[-1] + h + 1

    def rim(x, y, letter="c"):
        if 0 <= x < SIZE and 0 <= y < SIZE:
            g[y][x] = letter

    if side in "ew":
        # The bend at the far end: the top rounds over its last three columns, each lower and darker than the
        # last, down toward the floor; the face below runs on to the end.
        end = cols[0] if side == "e" else cols[-1]
        step = 1 if side == "e" else -1
        for k, (drop, tone) in enumerate(((3, "D"), (2, "d"), (1, "m"))):
            x = end + k * step
            for y in rows:
                g[y][x] = "." if y < rows[0] + drop else "L" if y == rows[0] + drop else tone
        # The rim on the floor beyond the end, and under the face.
        ring = end - step
        for y in range(rows[0] + h, floor):
            rim(ring, y, "n" if y in (rows[0] + h, floor - 1) else "o")
        for x in range(min(ring, end + 3 * step), max(ring, end + 3 * step) + 1):
            rim(x, floor, "C")
    elif side == "n":
        # Bending toward us: the top runs on, its crown catches the light, and its face drops to the floor.
        bend = rows[-1] - 2
        for x in cols:
            g[bend][x], g[bend + 1][x], g[bend + 2][x] = "l", "L", "f"
            for y in range(bend + 3, floor - 1):
                g[y][x] = "F" if y < floor - 2 else "K"
            g[floor - 1][x] = "o"
            rim(x, floor, "c")
        for y in range(bend + 2, floor + 1):
            rim(cols[0] - 1, y, "c")
            rim(cols[-1] + 1, y, "C")
        rim(cols[0] - 1, floor, "n")
        rim(cols[-1] + 1, floor, "n")
    else:
        # Going down away from us: its far end rounds over and drops, the rim showing beside it.
        for x in cols:
            g[0][x], g[1][x] = "D", "d"
        for y in range(0, h + 1):
            rim(cols[0] - 1, y, "c")
            rim(cols[-1] + 1, y, "C")
        rim(cols[0] - 1, h, "n")
        rim(cols[-1] + 1, h, "n")
    return g


def duct_vent(joins):
    """A straight duct with a louvred vent in its top, breathing out into the mall."""
    g = duct(joins)
    if joins == "ew":
        x0, x1, y0, y1 = 3, 8, DUCT_ROWS[1], DUCT_ROWS[-1]
    else:
        x0, x1, y0, y1 = DUCT_COLS[0] + 1, DUCT_COLS[-1] - 1, 3, 9
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if y in (y0, y1) or x in (x0, x1):
                g[y][x] = "G" if (y == y1 or x == x1) else "g"
            else:
                g[y][x] = "w" if (y - y0) % 2 == 0 else ("v" if x % 3 else "V")
    return g


# --- Water pipes: round, 8 px across, lying on the floor and casting a shadow to the south-east.

PIPE_INKS = [
    ("h", "e4f2fa", "shine"),
    ("l", "a6c8de", "pipe lit"),
    ("p", "7aa4c4", "pipe"),
    ("P", "5a82a2", "pipe shade"),
    ("D", "3e5e7a", "pipe dark"),
    ("K", "2a3e52", "pipe underside"),
    ("q", "c2c8ce", "flange lit"),
    ("Q", "7c848c", "flange shade"),
    ("b", "3c4248", "bolt"),
    ("a", "6cb878", "green band"),
    ("A", "3e7a4a", "green band shade"),
    ("c", "6a6e72", "floor collar"),
    ("C", "3c3f42", "floor collar shade"),
    ("n", "a4aaae", "collar bolt"),
    ("o", "121416", "hole"),
    ("s", "00000050", "shadow"),
]
VALVE_INKS = [("w", "d04040", "wheel"), ("W", "8a2626", "wheel shade"), ("t", "c8ccd0", "valve stem")]
LEAK_INKS = [("u", "6cc4f4", "drop"), ("U", "6cc4f450", "drop, fallen"), ("y", "5cb4ec50", "puddle")]
RADIUS = 4.0
FLANGE_RADIUS = 5.2
LIGHT = (-0.45, -0.65, 0.62)
# Where a capped end stops, past the centre away from the side it joins.
CAP_REACH = 2.5
# How far a lone piece reaches each way from the centre.
LONE_REACH = 4.5
# The cast shadow's offset, east and south.
SHADOW = (1, 2)
# A pipe bending down into the floor: where the floor meets it, below its middle, and the collar round it there,
# a flat ring seen from the side. Going down away from us, its crown stops here, the collar behind it.
DOWN_FLOOR_Y = CENTRE + 4.5
COLLAR_RX, COLLAR_RY = 5.8, 2.0
DOWN_FAR_Y = 3.5


def norm(v):
    m = math.sqrt(sum(c * c for c in v))
    return tuple(c / m for c in v)


L = norm(LIGHT)
EDGE = {"n": (CENTRE, -1.0), "e": (SIZE + 1.0, CENTRE), "s": (CENTRE, SIZE + 1.0), "w": (-1.0, CENTRE)}
STEP = {"n": (0, -1), "e": (1, 0), "s": (0, 1), "w": (-1, 0)}
OPPOSITE = {"n": "s", "e": "w", "s": "n", "w": "e"}


def closest(p, a, b, clip_start=False, clip_end=False):
    """The nearest point to p on the segment a-b, or None if p lies behind a cut end."""
    ax, ay = a
    dx, dy = b[0] - ax, b[1] - ay
    t = ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)
    if (clip_start and t < 0) or (clip_end and t > 1):
        return None
    t = max(0.0, min(1.0, t))
    return (ax + t * dx, ay + t * dy)


def shade(u):
    """The letter for a point on a tube, u its offset from the tube's middle over its radius."""
    z = math.sqrt(max(0.0, 1 - u[0] * u[0] - u[1] * u[1]))
    b = u[0] * L[0] + u[1] * L[1] + z * L[2]
    for limit, letter in ((0.95, "h"), (0.78, "l"), (0.52, "p"), (0.25, "P"), (0.0, "D")):
        if b > limit:
            return letter
    return "K"


def pipe_segments(joins, end="cap"):
    """The pipe's middle lines, each (from, to, cut at from, cut at to)."""
    if not joins:
        return [((CENTRE - LONE_REACH, CENTRE), (CENTRE + LONE_REACH, CENTRE), True, True)]
    if len(joins) == 1:
        side = joins
        sx, sy = STEP[side]
        if end == "cap":
            start = (CENTRE - sx * CAP_REACH, CENTRE - sy * CAP_REACH)
            return [(start, EDGE[side], True, False)]
        if side == "s":
            # Going down at its far end, away from us: the bend's crown is all that shows.
            return [((CENTRE, DOWN_FAR_Y), EDGE[side], False, False)]
        # Going down at this end: it bends toward the floor, which lies this far below its middle.
        return [((CENTRE, CENTRE), EDGE[side], False, False), ((CENTRE, CENTRE), (CENTRE, DOWN_FLOOR_Y), False, False)]
    return [((CENTRE, CENTRE), EDGE[side], False, False) for side in joins]


def tube(p, segs, radius):
    """The offset over radius of p from the nearest middle line, if p is on the tube."""
    best = None
    for a, b, cut_a, cut_b in segs:
        c = closest(p, a, b, cut_a, cut_b)
        if c is None:
            continue
        d = math.dist(p, c)
        if d < radius and (best is None or d < best[0]):
            best = (d, ((p[0] - c[0]) / radius, (p[1] - c[1]) / radius))
    return best and best[1]


def pipe(joins, end="cap"):
    """The pipe piece joining the given sides; a lone end is capped, or (end="down") bends into the floor."""
    g = blank()
    segs = pipe_segments(joins, end)
    on = [[tube((x + 0.5, y + 0.5), segs, RADIUS) for x in range(SIZE)] for y in range(SIZE)]
    if end == "down":
        # The collar on the floor where it goes in: a flat ring, its dark gap round the pipe.
        cy = DOWN_FLOOR_Y if joins != "s" else DOWN_FAR_Y - 1.5
        for y in range(SIZE):
            for x in range(SIZE):
                u, v = (x + 0.5 - CENTRE) / COLLAR_RX, (y + 0.5 - cy) / COLLAR_RY
                r = math.hypot(u, v)
                if r < 0.72:
                    g[y][x] = "o"
                elif r < 1.0:
                    g[y][x] = "c" if v < 0.25 else "C"
        for x in (1, SIZE - 2):
            if 0 <= int(cy) < SIZE:
                g[int(cy)][x] = "n"
        if joins != "s":
            # Below the floor the pipe is gone.
            for y in range(SIZE):
                for x in range(SIZE):
                    if y + 0.5 > DOWN_FLOOR_Y:
                        on[y][x] = None
    for y in range(SIZE):
        for x in range(SIZE):
            sx, sy = x - SHADOW[0], y - SHADOW[1]
            if g[y][x] == "." and 0 <= sx < SIZE and 0 <= sy < SIZE and on[sy][sx]:
                g[y][x] = "s"
    for y in range(SIZE):
        for x in range(SIZE):
            if on[y][x]:
                g[y][x] = shade(on[y][x])
    # Flanges where this piece meets the one before it, and on a capped end.
    flanges = [side for side in "wn" if side in joins]
    for side in flanges:
        for y in range(SIZE):
            for x in range(SIZE):
                inside = x < 2 if side == "w" else y < 2
                off = (y + 0.5 - CENTRE) if side == "w" else (x + 0.5 - CENTRE)
                if inside and abs(off) < FLANGE_RADIUS:
                    u = off / FLANGE_RADIUS
                    g[y][x] = "q" if u < 0.2 else "Q"
        for k in (1, SIZE - 2):
            if side == "w":
                g[k][0] = "b"
            else:
                g[0][k] = "b"
    # The caps on a capped end, and on both ends of a lone piece.
    if end == "cap" and len(joins) <= 1:
        ends = [(-CAP_REACH * sum(STEP[joins]), joins in "ew")] if joins else [(-LONE_REACH, True), (LONE_REACH, True)]
        for reach, across in ends:
            a = CENTRE + reach
            lo, hi = (a, a + 2) if reach < 0 else (a - 2, a)
            for y in range(SIZE):
                for x in range(SIZE):
                    along = x + 0.5 if across else y + 0.5
                    off = (y + 0.5 - CENTRE) if across else (x + 0.5 - CENTRE)
                    if lo <= along < hi and abs(off) < FLANGE_RADIUS:
                        g[y][x] = "q" if off / FLANGE_RADIUS < 0.2 else "Q"
    # On a straight run, a green band says it carries water.
    if joins in ("ew", "ns"):
        for y in range(SIZE):
            for x in range(SIZE):
                band = x in (7, 8) if joins == "ew" else y in (7, 8)
                if band and on[y][x]:
                    g[y][x] = "a" if g[y][x] in "hlp" else "A"
    return g


def pipe_valve(joins):
    """A straight pipe with a red handwheel on top, to shut the water off."""
    g = pipe(joins)
    cx, cy = CENTRE, CENTRE - 0.5
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            d = math.hypot(dx, dy)
            if 3.4 < d < 4.9:
                g[y][x] = "w" if dx + dy < 1 else "W"
            elif d <= 3.4 and (abs(dx) < 0.6 or abs(dy) < 0.6):
                g[y][x] = "t" if d < 1.3 else ("w" if dx + dy < 0 else "W")
    return g


def pipe_leaking(joins):
    """A straight pipe dripping from a crack underneath into a puddle."""
    g = pipe(joins)
    if joins == "ew":
        for x in range(3, 10):
            g[11][x] = "y"
        g[10][5], g[11][5] = "u", "U"
    else:
        for y in range(4, 10):
            g[y][11] = "y"
        g[6][10], g[7][11] = "u", "U"
    return g


# --- Writing it out.

def all_joins():
    """Every set of sides, in n e s w order, fewest first."""
    sets = ["".join(s for i, s in enumerate(SIDES) if m >> i & 1) for m in range(16)]
    return sorted(sets, key=lambda s: (len(s), [SIDES.index(c) for c in s]))


def describe(kind, joins):
    if not joins:
        return f"A lone piece of {kind}, joining nothing."
    if len(joins) == 1:
        return f"The capped end of a {kind} run going on to the {dict(n='north', e='east', s='south', w='west')[joins]}."
    names = dict(n="north", e="east", s="south", w="west")
    return f"The {kind} joining " + ", ".join(names[s] for s in joins[:-1]) + " and " + names[joins[-1]] + "."


def sprite(name, comment, inks, grid, blink=None):
    used = {c for row in grid for c in row}
    lines = [f"sprite {name}", *[f"# {c}" for c in comment]]
    lines += [f"ink {k} {v} {what}" for k, v, what in inks if k in used or (blink and k in blink)]
    if blink:
        lines.append(f"blink {blink[0]} {blink[1]}")
    lines += ["".join(row) for row in grid]
    return "\n".join(lines) + "\n"


def groups():
    out = ["group Air ducts\n"]
    for joins in all_joins():
        name = "duct" + (" " + joins if joins else "")
        note = [describe("duct", joins), "Two cells by two, seen from above: its front face shows to the south where nothing joins it."]
        out.append(sprite(name, note, DUCT_INKS, duct(joins)))
    for side in SIDES:
        note = [f"The duct coming in from the {dict(n='north', e='east', s='south', w='west')[side]} and bending down into the floor, through a bolted collar."]
        out.append(sprite(f"duct down {side}", note, DUCT_INKS, duct_down(side)))
    for joins in ("ew", "ns"):
        note = ["A straight duct with a vent in its top. v and V, the dark between the slats, trade colours as the air stirs."]
        out.append(sprite(f"duct {joins} vent", note, DUCT_INKS + VENT_INKS, duct_vent(joins), ("v", "V")))
    out.append("group Water pipes\n")
    for joins in all_joins():
        name = "pipe" + (" " + joins if joins else "")
        note = [describe("water pipe", joins), "Two cells by two, seen from above, lying on the floor; a straight run wears a green band."]
        out.append(sprite(name, note, PIPE_INKS, pipe(joins)))
    for side in SIDES:
        note = [f"The pipe coming in from the {dict(n='north', e='east', s='south', w='west')[side]} and bending down into the floor, through a bolted collar."]
        out.append(sprite(f"pipe down {side}", note, PIPE_INKS, pipe(side, end="down")))
    for joins in ("ew", "ns"):
        out.append(sprite(f"pipe {joins} valve", ["A straight pipe with a red handwheel on it, to shut the water off."],
                          PIPE_INKS + VALVE_INKS, pipe_valve(joins)))
    for joins in ("ew", "ns"):
        out.append(sprite(f"pipe {joins} leaking",
                          ["A straight pipe dripping into a puddle. u and U, the drop forming and fallen, trade colours",
                           "on every other blink."], PIPE_INKS + LEAK_INKS, pipe_leaking(joins), ("u", "U")))
    return "\n".join(out) + "\n"


def write(path="data/sprites.txt"):
    text = open(path).read()
    start = text.index("group Air ducts\n")
    pipes = text.index("group Water pipes\n", start)
    end = text.index("\ngroup ", pipes) + 1
    open(path, "w").write(text[:start] + groups() + text[end:])
    print("wrote the ducts and pipes to", path)


if __name__ == "__main__":
    if "--write" in sys.argv:
        write()
    else:
        sys.stdout.write(groups())
