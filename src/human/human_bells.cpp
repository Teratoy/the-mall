// The mall's bells (AI/light_plan.md step 6). A bell rings every mall hour,
// and another at opening and at closing, heard all through the mall. At
// closing the lights go out: it is uneasy, the timid most, and it is ready
// to sleep sooner. At opening the lights come on: it takes heart, and wakes
// if it is not worn through. After a few closings it knows what the bell
// means. The hour bell it mostly lets pass.
#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"

namespace {

/// Dread at the closing bell, before bravery sways it, and hope at the
/// opening one, the bolder the more.
constexpr float CLOSING_DREAD = 0.15f;
constexpr float OPENING_HOPE = 0.03f;
/// Closing bells heard before it knows the lights go out with them.
constexpr int CLOCK_LEARN = 2;

} // namespace

void Human::hearBells(World& world) {
    if (bells_.heard < 0) bells_.heard = world.bellsRung();
    if (bells_.heard == world.bellsRung()) return;
    bells_.heard = world.bellsRung();
    const bool asleep = state == State::Sleeping;
    switch (world.lastBell()) {
    case Bell::Hour:
        if (asleep) return;
        if (!bells_.hourHeard) {
            bells_.hourHeard = true;
            note("HEARD A BELL TOLL THE HOUR");
        }
        sayLater(talk::Moment::HourBell);
        return;
    case Bell::Closing:
        // Asleep, it hears the bell in its sleep and knows no more.
        bells_.closed = true;
        if (asleep) return;
        note("THE BELL RANG AND THE LIGHTS WENT OUT");
        sayLater(talk::Moment::ClosingBell);
        scare(CLOSING_DREAD);
        if (++bells_.closings == CLOCK_LEARN) {
            note("THE LIGHTS GO OUT AT THE BELL. EVERY TIME");
            sayLater(talk::Moment::ClockLesson);
        }
        rethink_ = true;
        return;
    case Bell::Opening:
        bells_.closed = false;
        lift(OPENING_HOPE * (0.5f + traits.brave));
        if (asleep) {
            if (!exhausted()) wakeUp(world, false, -1, "WOKE TO THE BELL");
            return;
        }
        note("THE BELL RANG AND THE LIGHTS CAME ON");
        sayLater(talk::Moment::OpeningBell);
        rethink_ = true;
        return;
    }
}
