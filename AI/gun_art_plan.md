# Gun art

Sprites for guns, drawn before guns have any gameplay: nobody carries,
shoots or finds one yet. When guns get a plan of their own, it wires these
in. Everything here was asked for and shaped by the user step by step.

## What was decided

- **Carried gun:** tucked into the waistband behind the hip, only the grip
  and the slide's end showing above the waistline (`HumanLook::tuckedGun`).
  A belt and holster were tried first and dropped (user). The slide rests on
  the waistline on short torsos too.
- **Shooting:** eight ways. Facing right the arm aims straight up, up ahead,
  ahead, down ahead or south; facing left mirrors the three that face a
  way. Straight up and south are never mirrored.
  - The gun stays in the same hand: facing left the far arm shoots, out
    from behind the body, a shade darker; up ahead it rises from the
    shoulder's corner (user).
  - South is aimed at the one looking on: the arm comes round in front of
    the chest and the gun is seen end on, not pointed at the feet (user).
  - No muzzle flash (user): going off, the hand is knocked back a step, the
    muzzle tips up a step and the head jolts back, for `SHOT_TIME`.
- **Ammo box:** shut until someone opens it, then open showing what is
  left, up to 8 rounds in two rows, the last taken first (user: both).
- **Spent casings:** each shot throws one out of the slide to the gun's
  right, whichever way it aims (aimed east it lands south, aimed north
  east). It tumbles, bounces once and lies about a cell out (user).
- **Casings on the floor:** scattered only, each in its own spot on the
  cell, up to 8; more look the same as 8 (user). The first few keep clear
  of each other so they read one by one.
- **Brass:** muted, to sit with the greyed palette (user).
- **Reloading:** rounds are pressed into the gun one by one; there is no
  magazine (user). Side on, the gun held low in front of the chest, muzzle
  down ahead; the other hand takes a round from the hip pocket, brings it
  up to the top of the slide and thumbs it in, the gun dipping as it goes
  in. 1.2 s a round, to vary with traits (user). Facing left the gun stays
  in the same hand, as shooting.
- **Racking:** once the last round is in, the slide is racked (user): the
  loading hand grips the back of it, pulls it back, lets go, and the gun
  jumps as it springs home.

## Built (2026-11-01)

- `data/sprites.txt`: `gun held`, `gun held raised`, `gun held toward`,
  `ammo box`, `ammo box open`, `casing`.
- `HumanLook::shoot(aim, fire, far)`, `Pose::Shoot*`; `HumanSprites` draws
  the held gun (`drawHeldGun`) and the flying casing for `CASING_TIME`
  after the shot, and tells where it comes to rest (`casingRest`), for the
  floor to keep it.
- `drawAmmoBox(x, y, opened, left, lit)`, `drawCasings(x, y, count, lit)`
  in `render/gun_art`.
- `HumanLook::reload(frame, far)`, `Pose::Reload`: five frames a round
  (pocket, bringing it up, at the gun, pressing it in, the hand going back),
  a round every `RELOAD_ROUND_TIME`; the caller scales the time for a
  human quicker or slower at it.
- `HumanLook::rack(frame, far)`, `Pose::Rack`: grip, pull, let go, the
  hand dropped away (held on), over half a second; the game plays it after
  the last round.
- Tools: `make dirtysheet DIRTYSHEET_ARGS="42 4 gun"` (tucked gun on every
  bottom) and `"42 4 shoot"`; `make shootanim` and `tools/frames_gif.py`
  (one human shooting all eight ways, casings piling up, the ammo boxes;
  `SHOOTANIM_ARGS="42 8 reload"` reloads three rounds and racks the slide
  each way instead);
  `make spritesheet` has all of it.

## Still open

- Aiming straight up still points at the ceiling; north is not yet aimed
  into the screen the way south is aimed out of it.
- Which traits make reloading quicker or slower, and by how much.
- Gameplay: who finds, carries and shoots guns, what a shot does, where
  ammo boxes lie, whether the floor keeps casings and for how long. None
  of it is decided.
