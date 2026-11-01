#!/usr/bin/env python3
"""Draws a technical room from the sprites, to see the plant and its runs together as they'd stand in the mall:
electric boilers and air handlers against the walls, their pipes and ducts leaving by the outlets, along the
floor and down into it. A mock-up only: the floor and walls are plain, not the game's.

Usage (on the host, which has PIL): python3 tools/tech_room.py [out.png] [scale]
    defaults: build/tech-room.png, 3
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402
import plant_art  # noqa: E402
import sprite_preview as sp  # noqa: E402

CELL = 6
# The room in cells, walls included, and the doorway in the south wall.
ROOM_W, ROOM_H = 36, 21
DOOR = range(15, 19)
FLOOR = (110, 106, 100, 255)
FLOOR_SEAM = (100, 96, 90, 255)
WALL_TOP = (44, 42, 48, 255)
WALL_FACE = (88, 86, 94, 255)
WALL_FACE_LIT = (104, 102, 110, 255)
WALL_SKIRT = (56, 54, 60, 255)
WALL_SEAM = (78, 76, 84, 255)
SHADOW = (0, 0, 0, 60)
# The north wall stands this many cells tall, as tall as the machines, its face hanging below its top.
WALL_CELLS = 3

# The machines: (what, cell x, cell y of its top-left, outlets to the sides, front entry's column or None).
MACHINES = [
    ("boiler", 1, 1, "e", 0),
    ("air handler", 20, 1, "w", 6),
    ("air handler", 31, 1, "", 0),
    ("boiler side", 1, 12, None, None),
    ("boiler", 12, 13, "w", 6),
    ("air handler side", 24, 11, None, None),
]
# The runs: (sprite, cell x, cell y of its top-left). Each piece is two cells by two.
PIECES = [
    # The first boiler: its east outlet goes straight down into the floor; its front runs south past a valve, then
    # east, leaking, and down.
    ("pipe down w", 4, 4),
    ("pipe ns", 1, 6), ("pipe ns valve", 1, 8), ("pipe ne", 1, 10),
    ("pipe ew leaking", 3, 10), ("pipe ew", 5, 10), ("pipe down w", 7, 10),
    # The second boiler, against the west wall, sends its pipe south into the floor.
    ("pipe down n", 1, 18),
    # The first air handler: its west outlet turns south past a vent and down; its front runs south, then east
    # past another vent, and south again into the floor.
    ("duct es", 18, 4), ("duct ns", 18, 6), ("duct ns vent", 18, 8), ("duct down n", 18, 10),
    ("duct ns", 21, 6), ("duct ne", 21, 8), ("duct ew", 23, 8), ("duct ew vent", 25, 8),
    ("duct sw", 27, 8), ("duct ns", 27, 10), ("duct down n", 27, 12),
    # The second air handler sends its duct south into the floor.
    ("duct ns", 31, 6), ("duct down n", 31, 8),
    # The boiler standing in the room: its west outlet goes into the floor, its front south and into the floor.
    ("pipe down e", 10, 16), ("pipe down n", 13, 18),
    # The air handler seen from its right sends its duct south into the floor.
    ("duct down n", 24, 17),
]


def sprite_of(grid, inks, blink=None):
    """A sprite in the form sprite_preview draws, from rows of letters and their inks."""
    s = {"inks": {}, "blink": blink, "rows": ["".join(r) for r in grid]}
    for k, v, _ in inks:
        v = v if len(v) == 8 else v + "ff"
        s["inks"][k] = tuple(int(v[i:i + 2], 16) for i in range(0, 8, 2))
    return s


def machine(name, sides, front):
    """The machine's sprite with the outlets asked for; the side views have just the one."""
    if name == "boiler":
        return sprite_of(plant_art.boiler_front(sides, front), plant_art.BOILER_INKS)
    if name == "air handler":
        return sprite_of(plant_art.ahu_front(sides, front), plant_art.AHU_INKS)
    if name == "boiler side":
        return sprite_of(plant_art.boiler_side(), plant_art.BOILER_INKS)
    return sprite_of(plant_art.ahu_side(), plant_art.AHU_INKS)


def room():
    img = Image.new("RGBA", (ROOM_W * CELL, ROOM_H * CELL), FLOOR)
    px = img.load()
    for y in range(ROOM_H * CELL):
        for x in range(ROOM_W * CELL):
            if x % CELL == 0 or y % CELL == 0:
                px[x, y] = FLOOR_SEAM

    def wall(cx, cy):
        face = cy + 1 < ROOM_H and not is_wall(cx, cy + 1)
        for y in range(CELL):
            for x in range(CELL):
                px[cx * CELL + x, cy * CELL + y] = WALL_FACE if face and y >= CELL - 2 else WALL_TOP

    def is_wall(cx, cy):
        return cy == 0 or cx == 0 or cx == ROOM_W - 1 or (cy == ROOM_H - 1 and cx not in DOOR)

    for cy in range(ROOM_H):
        for cx in range(ROOM_W):
            if is_wall(cx, cy):
                wall(cx, cy)
    # The north wall's face, from its top down to the floor, panelled, with a skirting at its foot.
    for y in range(CELL, (1 + WALL_CELLS) * CELL):
        for x in range(CELL, (ROOM_W - 1) * CELL):
            foot = (1 + WALL_CELLS) * CELL - y
            px[x, y] = WALL_SKIRT if foot <= 2 else WALL_FACE_LIT if y == CELL else \
                WALL_SEAM if (x // CELL) % 3 == 0 and x % CELL == 0 else WALL_FACE
    # The walls' shadows on the floor: along the west wall, and under the north wall's foot.
    for y in range((1 + WALL_CELLS) * CELL, (ROOM_H - 1) * CELL):
        for x in range(CELL, CELL + 3):
            px[x, y] = blend(px[x, y], SHADOW)
    for y in range((1 + WALL_CELLS) * CELL, (1 + WALL_CELLS) * CELL + 2):
        for x in range(CELL, (ROOM_W - 1) * CELL):
            px[x, y] = blend(px[x, y], SHADOW)
    return img


def blend(under, over):
    a = over[3] / 255
    return tuple(round(over[k] * a + under[k] * (1 - a)) for k in range(3)) + (255,)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/tech-room.png"
    scale = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    sp.SCALE = 1
    sprites = sp.load("data/sprites.txt")
    img = room()
    # Everything drawn from the back of the room to the front, so a tall machine stands over the floor behind it.
    things = [(cy * CELL + 2 * CELL, sprites[name], cx, cy) for name, cx, cy in PIECES]
    for name, cx, cy, sides, front in MACHINES:
        s = machine(name, sides, front)
        things.append((cy * CELL + len(s["rows"]), s, cx, cy))
    for _, s, cx, cy in sorted(things, key=lambda t: t[0]):
        sp.paint(img, sp.pixels(s), cx * CELL, cy * CELL)
    img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    img.save(out)
    print("saved", out)


if __name__ == "__main__":
    main()
