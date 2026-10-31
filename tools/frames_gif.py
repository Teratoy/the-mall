#!/usr/bin/env python3
"""Joins numbered PNG frames into a looping GIF. Run on the host, which has
PIL (the toolbox doesn't).

Usage: tools/frames_gif.py DIR OUT.gif [MS]   frames DIR/frame*.png, MS each (50)
"""
import sys
from pathlib import Path

from PIL import Image


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    frames = sorted(Path(sys.argv[1]).glob("frame*.png"))
    if not frames:
        sys.exit(f"no frames in {sys.argv[1]}")
    ms = int(sys.argv[3]) if len(sys.argv) > 3 else 50
    images = [Image.open(f).convert("RGB") for f in frames]
    images[0].save(sys.argv[2], save_all=True, append_images=images[1:], duration=ms, loop=0, optimize=False)
    print(f"wrote {len(images)} frames to {sys.argv[2]}")


if __name__ == "__main__":
    main()
