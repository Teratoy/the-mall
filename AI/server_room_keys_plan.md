# Learning the server room airlock

Server rooms ([server_rooms_plan.md](server_rooms_plan.md)) sit behind two
locked doors in a row, each costing a key, which shut and relock behind
whoever goes through: every visit costs 2 keys. The user found humans
waste all their keys on data centre doors. This plan makes them learn the
price, and make the most of a visit once they've paid it.

Status: built (2026-10-31). See "Built" at the end.

## User's decisions (2026-10-31)

- **Learnt the second time.** A human that opens a door and finds another
  locked door right behind it is surprised the first time (a log line and
  a moment). The second time it learns: these double doors eat keys.
- **After that, curiosity decides.** Having learnt, a human doesn't spend
  keys on a server room's door to explore or look for a way out, unless
  it is curious, and then only with keys enough for both doors. It still
  spends them to go in and sleep by the hum (which already wants 2 keys).
- **Told at greetings.** A human that has learnt it passes the lesson on
  when it greets someone who hasn't, like traps, so the listener knows
  without paying.
- **Sleep while it's in there.** Inside a server room, once it has noticed
  the hum (the first time it is ever inside one), a human that is drowsy
  (tiredness 0.35, before the usual "tired" at 0.5) lies down there and
  then rather than leave and pay 2 keys to come back.

## How a human learns it (only from its own Memory)

- Seeing an airlock door, a human already keeps which way it opens at a
  push (`Memory::airlocks`): the doors look alike, so once it knows what
  they are it knows one when it sees one.
- Right after unlocking a door with a key (to explore, or on its way in
  to sleep), it looks: if the door was an airlock door and another airlock
  door it knows stands a few cells further in, it has met the airlock.
- Told the lesson at a greeting, it has learnt it as if it had paid.

## Too few keys

- Unchanged from before: with a single key a human that wants to sleep
  doesn't set off for an outer door, and from the lobby or the room the
  doors open freely, so nobody is shut in.
- A curious human that has learnt dares the outer door only with 2 keys or
  more, and the inner door with 1, so it never stops halfway for want of a
  key it knew it would need.

## Steps

1. `server_` state: `airlocksMet` (count), `keyWise` (learnt), `lulled`
   (has noticed the hum from inside), `passedUp` (airlock doors it has
   let be, so it says so once a door).
2. After unlocking a door (`arrive` for `Goal::OpenDoor`, and `seekHum`):
   `meetAirlock(door)`. First: `ANOTHER LOCKED DOOR BEHIND THE DOOR`,
   `[airlock_surprise]`. Second: `THE DOUBLE DOORS EAT KEYS`,
   `[airlock_lesson]`.
3. `doorWorthKey(c)`: as `lockedDoorBeside`, but a key-wise human leaves
   out airlock doors unless it dares (curious, with the keys for the rest
   of the way). Used for `Want::OpenDoor` and `Want::UrgedDoor`. The first
   time a search passes over an airlock door nearer than where it goes:
   `KEPT MY KEYS FROM THE DOUBLE DOORS`, `[spare_keys]`. A dare:
   `OPENED THE DOUBLE DOORS ANYWAY`, `[dare_airlock]`.
4. `Tell::Kind::Airlock` in `newsFor`, after traps: `TOLD @ THE DOUBLE
   DOORS EAT KEYS`, `[tell_airlock]`; the listener learns it.
5. Inside a known server room the first time: `THE HUM IN HERE IS MADE
   FOR SLEEP`, `[hum_lesson]`. Drowsy inside and lulled: wants sleep
   there (`SLEEPING HERE RATHER THAN PAY TO COME BACK`, `[stay_hum]`).
6. Scenarios: `airlock-learn`, `airlock-spare`, `airlock-dare`,
   `airlock-tell`, `hum-stay`.

## Built (2026-10-31)

- `human_server.cpp`: `keysFor`, `meetAirlock`, `sparesKeys`,
  `doorWorthKey`, `spareKeys`; the hum lesson in `seeServerRoom`. A
  curious human dares from curiosity 0.65 (`AIRLOCK_DARE_CURIOUS`); an
  inner door is one with a known airlock door up to 3 cells out from it,
  the same way in (`AIRLOCK_DEPTH`).
- `Want::OpenDoor` and `Want::UrgedDoor` search with `doorWorthKey`. The
  dead-end lesson, checking doors and the thirsty's door still see every
  locked door.
- `humStay` (`human_sleep.cpp`): lulled, drowsy (0.35) and inside a server
  room it knows, it wants sleep as the tired do; `staying` is cleared on
  falling asleep.
- `Tell::Kind::Airlock` in `newsFor`, after traps and before keys, to a
  non-foe with keys who hasn't learnt it.
- Moments at the end of `talk::Moment`: `AirlockSurprise`,
  `AirlockLesson`, `SpareKeys`, `DareAirlock`, `TellAirlock`, `HumLesson`,
  `StayHum`, with lines in `data/lines.txt`.
- Scenarios: `airlock-surprise`, `airlock-learn`, `airlock-spare`,
  `airlock-dare`, `airlock-tell`, `hum-stay` (the airlock ones wall the
  human into a little box whose only way out is the outer door).
- Soak: seed 2, whose spawn neighbours a data centre, went from 2 of 8
  humans alive in 2 chunks to 8 of 15 in 9. Stills, flips and loops are
  the same in number over all six seeds.
