# PC layout

The phone layout keeps the world in a tall 216x468 canvas and hides the
stats, log and map behind buttons, one at a time, over the world. On a PC
monitor that wastes the sides of the screen. The PC layout makes the world
a square and lays the stats, log and map beside it, always open, so you
can read the log while watching the world.

Nothing here touches the sim: it is drawing and input only, so `make
check` must pass unchanged.

## User's decisions (2026-10-30)

- **Canvas:** 1014x468. The world is a 468x468 square; every column beside
  it is 182 px wide, the width the stats and log have on the phone.
- **Columns, left to right:** stats | log | world | HUD and map. Stats
  and log are side by side on the left, each full height. The right
  column has the HUD on top and the map filling the rest.
- **Strips:** the phone's top and bottom strips go into the columns: the
  name, items and the HP and need bars over the stats (left); the world
  size and the buttons over the map (right). The square shows the world,
  its speech bubbles, what they are doing in a strip along its bottom, and
  NEXT once the followed human is dead.
- **Moved (2026-10-30, after seeing it):** name, items and bars went from
  the right column to over the stats.
- **Moved (2026-10-30):** what they are doing went from the right column
  to a strip along the bottom of the square, NEXT just above it.
- **Switching:** by the window's shape. A window wider than tall uses the
  PC layout, taller than wide the phone one; resizing switches live. The
  desktop build opens wide. The menu, create and obituary screens stay
  phone-shaped, centred in a wide window.

## 1. A canvas of two shapes

- `Canvas` holds a phone texture (216x468, `Canvas::W`/`H` as now) and a
  wide one (`Canvas::WIDE_W` 1014 x 468). Each scene says whether it can
  draw wide (`Scene::wide()`, false by default); the world scene does.
  `Canvas` draws, scales and maps the mouse with the texture in use.
- The window opens wide, as large as fits the monitor comfortably.

## 2. The world in a viewport

- `cameraFor`, `drawWorld` and `drawSpeech` take the rectangle of canvas
  the world is shown in, instead of assuming the whole canvas: on the
  phone the whole canvas, on the PC the square.

## 3. The PC world scene

- Stats (`drawStats`) and log (`drawLog`) take their area; on the PC they
  are always drawn, each in its column. The log scrolls with the wheel or
  a drag over it.
- The map is always drawn under the HUD in the right column; the wheel
  over it zooms it, dragging moves it.
- The wheel over the world zooms the world, and + and - zoom the world.
  L, S and M do nothing on the PC, as all three are open.
- Over the stats: the name, items, then HP and the three needs as
  full-width bars on rows of their own.
- Along the bottom of the square: what they are doing, in a shaded strip.
- Over the map: world size and REVEAL, then the zoom, speed and pause
  buttons in a row.
- Pause darkens the whole canvas, its buttons in the middle.

## 4. Shots

- `make shot SHOT_ARGS="--wide"` saves the PC layout.

### Built (2026-10-30)

All four steps. The phone layout is pixel-identical to before in shots
(world, log, stats, zoom 0 and 3), and `make check` passes unchanged.

## Open questions

1. **Scale on a 1080p screen.** 1014 doesn't fit twice into 1920, so in
   whole steps the PC canvas would show at 1x, tiny. For now the canvas
   scales by whole steps only from 2x up and smoothly below that, so it
   fills the screen, at the cost of slightly uneven pixels on 1080p. The
   other ways out: narrower columns, or a shorter canvas.
