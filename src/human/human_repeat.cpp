// Repeats: what keeps happening to a human, counted (AI/repeat_lines_plan.md).
// The same cake again, another body, another door onto more of the same. At
// the 4th, 8th, 15th and 30th time it notes so, and may say how that feels;
// and each body after the first strikes it a little differently, as it
// hardens or wears down.
#include "human/human.h"
#include <algorithm>
#include <string>

namespace {

/// The times that something happening again is marked: noted, and maybe said.
constexpr int REPEAT_MILESTONES[] = {4, 8, 15, 30};
/// The chance it says anything at a milestone (the moment's own chance in
/// data/lines.txt comes on top).
constexpr float REPEAT_SAY_CHANCE = 0.7f;
/// Blows between the same two this far apart (seconds) are another fight.
constexpr float REPEAT_FIGHT_GAP = 30.0f;
/// Each body found after the first changes its dread by this much for each
/// step of hardness (brave over timid, loner over sociable, -1 to 1)...
constexpr float BODY_DREAD_STEP = 0.1f;
/// ...but never below or above these times what the first body did.
constexpr float BODY_DREAD_MIN = 0.6f;
constexpr float BODY_DREAD_MAX = 1.4f;

} // namespace

void Human::repeated(Repeat what, int about) {
    int n = ++repeat_.count[size_t(what)];
    if (std::find(std::begin(REPEAT_MILESTONES), std::end(REPEAT_MILESTONES), n) == std::end(REPEAT_MILESTONES))
        return;
    const std::string nth = std::to_string(n) + "TH";
    switch (what) {
    case Repeat::Cakes: note("ATE MY " + nth + " CAKE"); break;
    case Repeat::Bodies: note("FOUND MY " + nth + " BODY"); break;
    case Repeat::Water: note("HAD MY " + nth + " DRINK OF WATER"); break;
    case Repeat::Doors: note("UNLOCKED MY " + nth + " DOOR"); break;
    case Repeat::Traps: note("CAUGHT BY A TRAP THE " + nth + " TIME"); break;
    case Repeat::Blood: note("WASHED OFF BLOOD THE " + nth + " TIME"); break;
    case Repeat::Fights: note("IN MY " + nth + " FIGHT"); break;
    case Repeat::Going: note("WENT FOR THE " + nth + " TIME"); break;
    case Repeat::Sleep: note("WOKE UP HERE THE " + nth + " TIME"); break;
    case Repeat::Quiet: note("ALONE IN THE QUIET THE " + nth + " TIME"); break;
    case Repeat::Faces: note("SAW A FACE I KNEW THE " + nth + " TIME"); break;
    case Repeat::Low: note("SANK THIS LOW THE " + nth + " TIME"); break;
    case Repeat::Count: break;
    }
    if (!rng_.chance(REPEAT_SAY_CHANCE)) return;
    Saying s(talk::Moment(int(talk::Moment::CakesAgain) + int(what)), -1, about);
    s.repeat = n;
    sayLater(std::move(s));
}

void Human::fought(int other) {
    if (other != repeat_.fightWith || clock_ - repeat_.fightAt > REPEAT_FIGHT_GAP) repeated(Repeat::Fights, other);
    repeat_.fightWith = other;
    repeat_.fightAt = clock_;
}

float Human::bodyDread(bool friendDead) const {
    float hardness = (traits.brave - 0.5f) + (0.5f - traits.sociable);
    float before = float(repeat_.count[size_t(Repeat::Bodies)]);
    float f = std::clamp(1.0f - BODY_DREAD_STEP * hardness * before, BODY_DREAD_MIN, BODY_DREAD_MAX);
    return friendDead ? std::max(f, 1.0f) : f;
}
