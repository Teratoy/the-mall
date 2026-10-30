# Chit-chat

Humans who are together with nothing pressing talk casually, and what
they talk about always comes from what is around them, what they went
through, and what they know and feel right now. Never a line pulled out
of a hat: the topic is picked by what matters most to the speaker at that
moment, and the answer by what the listener knows and thinks.

Asked for 2026-10-29, after a trace showed group followers standing for
30 s beside a companion busy greeting: idle time together is where this
belongs.

## User's decisions (2026-10-29)

- Now and then: about once a minute per pair, more between the sociable
  and between friends.
- Wording deterministic too: most matching conditions, ties to the least
  recently used line. No dice anywhere in chat.
- Effects: bonding trust (shared memories and agreement bring two
  closer, quarrels about a person push them apart; a new trust source),
  stories spread dread, place remarks teach.
- All eight topics.
- Still open: muttering alone (question 4).

## What "not random" means here

- **Whether** they talk: an urge to chat grows the longer two humans are
  together and silent, faster the more sociable they are, slower when
  frightened, gloomy or disliking each other. Above a bar, the one with
  the higher urge speaks. No dice.
- **What about**: the speaker scores every topic it could raise (below)
  by how much it matters: how strong the thing was, how recent, whether it
  is right here, whether the listener was there, and whether it already
  talked about it with them. The highest-scoring topic wins.
- **How it is said**: lines.txt still has several wordings per topic, but
  the one picked is the one with the most matching conditions, and among
  equals the one it used least recently (with anyone). So the wording
  follows the situation and doesn't repeat, without rolling dice.
- **The answer**: decided by the listener's own memory and opinion: it
  was there too, saw it differently, never heard of it, thinks otherwise
  of the person, or isn't in the mood.

## Where it happens

- Waiting together: a follower beside its companion, a group waiting
  for its leader, two humans resting or sitting side by side.
- Walking together in a group, between steps (only short lines).
- A catch-up greeting with no news to trade: they chat instead of saying
  nothing.
- Never with a foe or suspect, never while hungry/thirsty enough to go
  looking, never while bleeding or fleeing. Erratic humans may say odd
  things (still from their own state, e.g. the body they ate).

## Topics, from context

Each topic needs real material in the speaker's memory or surroundings;
without it the topic doesn't exist for them.

1. **Here.** Something in sight: a giant statue (its kind and material:
   "A TEACUP. MADE OF BRONZE. WHO MAKES THESE?"), water or a basin, a
   table, a blood stain (fresh or dried), mess, a body, a slammed gate,
   the kind of place (endless maze, wide plaza, flooded maze).
2. **Shared past.** Something both were there for: a trap going off, a
   fight, a body found, a cake shared, a door they went through together.
   "Remember when..." Both have the episode in memory.
3. **The speaker's own story.** Something only the speaker went through,
   told to someone who wasn't there: getting hit, a trap, waking up, the
   first body. A story, not a tip (tips are Tell, already built).
4. **Someone else.** A third human both know, absent or nearby: a friend
   missed, a foe complained about, the dead mourned. Builds on gossip.
5. **How it is doing.** Getting hungry or thirsty soon, sore feet after a
   long walk, needing to go (said by the less modest), scared, hopeful.
6. **The way out.** Musing on whether there is one, from its own hope and
   hunch; the gloomy say there isn't.
7. **The group.** How long they have been together, how many doors they
   have gone through, praise or doubt of whoever leads (by trust, and by
   whether the leader has found anything).
8. **Before.** Nobody remembers anything from before waking. Early on,
   the newly woken ask each other; later it comes back as a sore point.

## Replies and short conversations

- A topic is raised by an opener; the listener answers from its own
  state: agrees (has the same episode / similar opinion), adds its own
  side ("I SAW IT TOO, FROM THE DOOR"), disagrees (thinks better or worse
  of the person, more or less hopeful), hasn't heard of it, or brushes it
  off (loner, rude, scared, hungry).
- 2 to 4 lines per conversation, longer between the sociable and friends,
  using the existing turn-taking pacing. Something pressing ends it.
- The same topic isn't raised again with the same listener until
  something about it changes (a new episode, an opinion that moved).

## What it does (see question 3)

Options, from none to all:
- Shared memories and agreement bring the two a little closer; a quarrel
  about a person pushes them a little apart.
- A story about a trap or body spreads some dread to listeners.
- Place remarks can teach ("THAT WATER'S FOUL").

## Shape

1. **Episodes**: a structured short memory of notable things (kind, when,
   where, who did it, to whom, how strong, and who else saw it), filled
   at the existing note() sites. The log stays as it is; episodes are what
   chat draws on. Kept few (strongest/recentest), fading with time.
2. **Chat urge and when**: per pair, in human.cpp next to speech pacing.
3. **Topic picker**: scores topics from episodes, sight, needs, hope,
   trust; remembers what was talked about with whom.
4. **Lines**: new sections in data/lines.txt per topic and reply
   (opener, agree, add, disagree, unheard, brush-off), new conditions
   (they.was_there, agree/disagree, here.statue...) and fillers
   ({THING} for the statue/thing, {N} for a count, {WHO} as now).
   Wording picked by most conditions, then least recently used.
5. **Conversations**: replies chained to openers, 2 to 4 turns.
6. **Effects**, as chosen.
7. **Probe**: headless count of chats per human-hour, topic spread,
   repeats per pair, and how often a chat matched what was actually in
   sight / in memory (should be always).

## Open questions

1. How chatty: a line every few seconds while idle together, or now and
   then (say once a minute per pair)?
2. Wording: deterministic least-recently-used as above, or is a random
   pick of wording fine as long as the topic and reply are contextual?
3. Effects: flavour only, or should chat move trust / dread / knowledge?
   (Trust tuning was kept as is so far; bonding through chat would be a
   new source of trust, which would also let real friendships form.)
4. Mutter alone too (sitting alone, at a dead end), or only between two
   or more?
5. Which topics first? All eight, or start with Here, Shared past, How
   it's doing, Someone else?

## Built

### Step 1: episodes (2026-10-29)

`src/human/episode.h` (Episode, Episode::What: 23 kinds) and
`src/human/human_episode.cpp` (Human::remember / rememberAt, salience,
shares). Strengths are EPISODE_* constants next to the event code in
human.cpp, human_blood.cpp, human_flesh.cpp, human_hope.cpp (foul water in
human_thirst.cpp). Recorded at: waking, punches (hitter, hit, onlookers via
witness()), kills (killer and onlookers), bodies found, flesh eaten / seen
eaten, gnawed bodies, traps (set off, seen, darts, spotted hidden), gifts
(share, pay, yield; onlookers of shares made via startShare), doors
unlocked, new chunks, pools of blood, escorts, snapping, friend/foe made,
lies found, starving, parched, looting (and onlookers), foul water, hope
lost/found, settling.

- Who did what to whom is told the same by everyone, so `shares(e)` finds
  the listener's own memory of the same thing: same kind, people (a fight
  either way round) and detail, and for events at about the same time
  and place (SAME_TIME 5 s, SAME_DIST 3); for things that stay put
  (bodies, pools, foul water, spotted traps, places) whenever seen.
  Personal things have who = the rememberer, so they are never shared.
- `with`: the others it had in sight (refreshed at each observe()).
- The same thing again within 30 s (blows in one fight) adds to one
  episode; 32 kept, the least salient forgotten. Salience = strength
  halving every 900 s x (0.5 + strength).
- `toldTo` is there for step 3, unused yet.

Probe (scratchpad grp/ep.cpp of session 2a717a3a, 8 seeds x 20 min, 6
extra humans): 15 episodes kept per human, mean salience 0.39. Of those
noted as there, 29/30 remember the same kill, 120/184 the same fight, 36/40
the same flesh-eating, 31/56 the same new place. Stuck soak 30 x 20 min:
0% blocked, 0 give-ups, 0 frozen, 0 loops.

Fixed after (user asked): a share made on arrival (Human::step,
Goal::Share) skipped witness(), so onlookers of most shares neither
thought better of the giver (TRUST_SAW_FEED) nor remembered it; it now
calls witness(). Anyone who sees a door unlocked remembers it
(EPISODE_SAW_DOOR 0.3).

### Step 2: chat urge (2026-10-29)

`src/human/human_chat.cpp`, ticked from Human::update every CHAT_TICK.

- Human::chat_.urge[id], 0 to 1: grows while the human is easy in mind
  (not hungry, thirsty, needing to go, bleeding, frightened or just hurt)
  toward everyone it has met, not a foe or suspect, in sight within
  CHAT_SEEN (10): full in CHAT_EVERY (75 s) for sociability 0.5,
  sway(sociable) 0.5x to 1.5x, x1.5 a friend, x0.5 someone disliked,
  slowed by dread (up to 60%) and gloom (40%), half as fast beyond
  CHAT_RANGE (5). Out of sight or not easy, it ebbs at 1/300 per s.
- Full and close (CHAT_RANGE) with both calm (calmForChat: Idle, Sitting,
  or walking in the same group on nothing urgent) and neither talking:
  startChat(), which for now only starts the silence over (quiet()) and
  counts Human::chats. The fullest urge wins, ties to the one first in
  the list.
- Full and farther: wouldGreet() says yes, so it walks over and catches
  up (even without news, once CATCH_UP_TIME has passed).
- Only a chat resets the urge, not greeting lines.

First try (urge only while both calm within 3 cells) gave no chats at
all: calm-together spells last a median 0.2 s, 90th percentile 8 s; most
time together is spent greeting, and hungry. Probe (scratchpad grp/chat.cpp,
8 seeds x 20 min, 6 extra humans) after: 16 chats, 0.87 per pair-minute
calm and together, 1.5 per human-hour; loners 0, middling 0.1, sociable
0.94 per minute beside someone. Stuck soak 30 x 20 min: clean; crowded
(10 x 15, +8): 2 followers flagged standing by a busy companion (was 6-11).

### Step 3: topic picker (2026-10-29)

`src/human/topic.h` (Topic: Kind Here / Shared / Story / Someone /
Feeling / Way / Group / Before, with Thing and Feel) and
`src/human/human_topic.cpp` (Human::pickTopic, raised, owns). startChat()
now picks the topic and keeps it (Human::lastTopic); words are step 4.

Scores, highest wins, no chance anywhere:
- Shared / Story: salience of each episode not yet told to them (toldTo),
  x1.3 if the listener remembers it too (shares), x1.0 if not; under 0.1
  not worth it. Waking feeds Before instead. The steady keep quiet about
  their own kills, flesh eaten and bodies robbed (owns); the erratic don't.
- Here: within HERE_RANGE (10) in sight: a body (0.7, a friend's 0.9),
  a pool of blood it knows (0.5), a statue (0.45), water or a basin (0.3;
  0.5 as foul, only if it drank foul water there), mess (0.35); x0.75 to
  x1.25 by curiosity. Same thing again with them after 600 s.
- Someone: a third human it trusts or distrusts by 0.15 or more (0.8 x
  how firmly), or a dead friend (0.8); one the listener has met or the
  speaker can name; never anyone within earshot. Again once its opinion
  moved 0.2.
- Feeling: peckish / dry (hunger or thirst 0.35 to 0.5, up to 0.5),
  footsore (600 s without sitting, 0.35), uneasy (dread 0.25+, 0.7 x
  dread), gloomy (0.3 to 0.6), hopeful (hope 0.85+, 0.3). Again after 180 s.
- Way: wanting out 0.3+ and hunch sure 0.15+: 0.4 x out drive; 300 s.
- Group: one keeps with the other and they went into 2+ places together:
  0.3 + 0.05 per place; 300 s.
- Before: 0.45 newly woken, down to 0.05 by 10 min; once per listener.

Probe (scratchpad grp/topic.cpp, 8 seeds x 20 min, +6 and +12 humans):
every chat's topic checked against the world (thing in sight, listener
remembers a Shared one and not a Story one): 0 misfits, 0 repeats. Chats
happen mostly in the first two minutes at the crowded spawn, so they are
mostly Before. Sampled every 30 s for every pair in sight over the whole
run (not raised): Before 54% (mostly as the only thing left, at 0.05),
shared fights 6-11%, the way out 6-9%, peckish/dry 8-9%, a lie found 3-4%,
starving 4%, statues 1-4%, bodies, flesh, gifts, traps, doors. Someone
almost never: trust seldom reaches 0.15 (expected to change with step 6's
bonding). Stuck soak 15 x 20 min clean.

### Step 4: lines (2026-10-29)

- Moments chat_here, chat_shared, chat_story, chat_someone, chat_feeling,
  chat_way, chat_group, chat_before (talk::chatMoment(kind)); sections at
  the end of data/lines.txt, with the new conditions and fillers listed
  in the file's comment (ep.*, by.me/to.me/by.you/to.you, here.*, statue.*,
  feel.*, about.*, i.lead/you.lead, spikes/darts/slam, gift.*, dark,
  strong, recent; {THING} the statue, {N} places together).
- Wording is fixed, not rolled: of the fitting lines, one not said by the
  speaker in the last CHAT_REPEAT (300 s) if any, then most conditions,
  then said longest ago (Human::voice_.chatLines). Chat sections ignore any
  [moment chance].
- startChat queues a Saying carrying the topic, about whoever the topic is
  about (the third person in an episode); the topic counts as raised only
  once the line is said.
- No tones on chat lines yet except a few warm thank-yous; dread from
  stories and teaching are step 6.

Probe (scratchpad grp/lines.cpp, 8 seeds x 20 min, +6 and +12): every chat
started found a line (16/16, 17/17). Before the 300 s rule a timid human
said the same "before" line to three listeners in 20 s; after, no speaker
repeats a line within the run.

### Step 5: replies and conversations (2026-10-29)

- Topic carries the conversation: turn (1 opener, 2 answer, 3 follow-up,
  4 close), turns, and the answer given (Topic::Reply: Agree, Add,
  Disagree, Unheard, Brush). Human::chatOn (human_chat.cpp), called from
  say() once a chat line is said, queues the next line for the other.
- Human::replyTo decides the answer from the listener's own state, no
  chance: brush off when not easy in mind, distrusting the speaker, or a
  loner (< 0.3) who isn't a friend; Shared / Group / Before agree; a Story
  gets Add if it went through the same kind of thing, else Unheard; Here
  agrees (Add for a friend's body it knew dead); Someone: Unheard if it
  doesn't know them or their death, or thinks nothing firm (< 0.15),
  else Agree / Disagree by the sign of its own opinion; Feeling: Agree if
  it feels the same (hunger/thirst 0.35+, 600 s unrested, dread 0.25+,
  gloom / hope 0.7+), Disagree for gloom against hope, else Add (comfort);
  Way: Disagree if gloomy, Unheard if unsure (< 0.15), else Agree within
  1 radian of its own hunch, Disagree beyond.
- Length: 2 lines, 3 when the two average 0.5+ sociable, 4 when they are
  also friends both ways or both 0.6+ sociable; a brush-off ends it.
  Idlers in a chat stand at least CHAT_HOLD (2.5 s) each line.
- Answering marks the topic talked of with them too (Human::answered: its
  own copy of a shared episode, its own opinion of the person).
- Moments reply_agree / reply_add / reply_disagree / reply_unheard /
  reply_brush, chat_follow, chat_close in lines.txt; new conditions is.here
  ... is.before (what was brought up) and agreed / added / disagreed /
  unheard (the answer), seen from whoever speaks.
- Ties in wording (same freshness, conditions and last use) now go to each
  human's own fixed favourite (a mixed hash of line and human id), so
  different humans word the same answer differently.

Probe (scratchpad grp/convo.cpp, prints conversations as said), 8 seeds x
20 min, +6: 13 chats, 2 of one line (answer never came: the other walked
off), 4 of two, 5 of three, 2 of four. Sample:
  DENE: I DON'T REMEMBER ANYTHING. IT SCARES ME.
  MASIA: SAME. I JUST WOKE UP HERE.
  DENE: I'M GLAD IT'S NOT JUST ME.
  TOVENJI: MY MOUTH IS DRY. / MASIA: ME TOO. WATER FIRST. /
  TOVENJI: WE'RE IN THIS TOGETHER, THEN.
  MIRAFA: I KEEP HEARING THAT FIGHT. / ABELE: I KNOW. I WAS THERE.
