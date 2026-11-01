# Choosing in the world

Today the create screen is a grey panel apart from the game: a big preview
cycling poses (stand, walk, wave, sit, sleep, wake), the name to tap and
type over, the human again at world size, four trait bars, and REFRESH and
CONTINUE. Then a separate `WorldScene` grows the spawn room and the human
lies there and comes to.

This plan makes choosing happen in the world itself. The screen is the
spawn room the human will wake in, with the human asleep where it lies,
and the choosing is laid over it. Pressing WAKE UP is the first tick of
the world: the overlay gives way to the HUD and the human wakes as it
always does. No cut between choosing someone and becoming them.

## User's decisions (2026-11-01)

- REFRESH rolls a new human in the same room: same floor, table, cake,
  key, statue and bottle spot; only the sleeper changes.
- The world is frozen until WAKE UP. The sim does not tick; only the
  sleeper's breathing (and its Z) is drawn moving. Waking is the first
  tick, so a world played after choosing runs exactly as one made today.
- Shown while choosing: the name (tap to type one, as now) and the four
  trait bars, in the top and bottom strips. The sleeper is the only
  preview of the look; the big posing preview and the small one go.
- Camera: close on the sleeper while choosing; on WAKE UP the overlay
  fades into the HUD and the zoom eases back to the play zoom.

## How it works

### Step 1: the world scene before waking

- `WorldScene` gets a choosing phase (`choosing_`), entered from the menu
  in place of `CreateScene`. While choosing, `update()` skips the world's
  tick, the hearing, the pause menu and the HUD; it draws the world as
  usual round the sleeper, zoomed in.
- The sleeper is drawn lying, eyes shut, breathing: the `Wake` pose held
  at its start, plus the sleeper's Z from `data/sprites.txt` drifting up,
  so it reads as asleep and not dead. Nothing in the sim changes for it.
- Rolling: the world is remade from the same world seed with a new look
  and human seed (`World` built afresh, `makeSpawn` with the new human).
  The room comes from the world's own dice before the human is added, and
  the bottle spot from dice of its own, so the room stays put across
  rolls. Remaking is cheap (one chunk) and keeps everything deterministic.
- The overlay, in the phone's two shaded strips:
  - Top strip: BACK, and the name centred, tappable to type (the same
    field, hint and rules as now: 14 characters, no @ or #, none on a
    touch-only phone).
  - Bottom strip: the four trait bars (BRAVE, GREEDY, CURIOUS, SOCIAL, 0–9
    pips), then REFRESH and WAKE UP.
  - All laid out from measured text, inside the strips.
- Keys as now: R rolls, ENTER wakes, ESC or back goes to the menu.
- The wide PC layout: the same overlay over the square of world, the
  name and traits in the stats column's place.

### Step 2: waking

- WAKE UP: the overlay fades out over about half a second while the HUD
  fades in, and the zoom eases from the close zoom to the play zoom
  (`zoom_` = 2) over about two seconds. The world starts ticking at once,
  so the human's 3–6 s of lying and its first waking lines play out under
  the eased camera.
- Nothing is put in the obituary for a human never woken: BACK from
  choosing leaves no record.
- `CreateScene` is removed, with its files. The genview "play this" path
  (`tryOut`) skips choosing and wakes at once, as today.

### Step 3: checks

- `make check` must pass unchanged: the soak never goes through the
  scenes, and a chosen world is the same world as before.
- `make shot` gets `--scene choose` to save the choosing screen, so the
  layout can be looked at without a window.

## Still open

- Whether the close zoom is the closest play zoom or closer still (start
  with the closest, `zoom_` = the last index).
- Whether BACK should fade too, or cut straight to the menu as now.
