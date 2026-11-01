#pragma once
#include "audio/voice.h"
#include "human/talk.h"

class Human;

namespace voice {

/// How this human sounds saying a line in that tone: its own voice, grown
/// from its look, traits and name (never the sim's dice, so the same human
/// always sounds the same), shaded by the tone and by its dread and gloom.
Voice of(const Human& human, talk::Tone tone);

} // namespace voice
