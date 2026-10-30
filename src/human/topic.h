#pragma once
#include "core/vec.h"
#include "human/episode.h"
#include <cstdint>

/// What a human brings up in a chat (AI/chitchat_plan.md), picked by
/// Human::pickTopic as what matters most to it right then, never by
/// chance. Each kind only exists with something real behind it: a thing
/// in sight, an episode remembered, a person it has an opinion of, how it
/// feels.
struct Topic {
    enum class Kind : uint8_t {
        Here,    // something in sight: thing, cell, detail
        Shared,  // something both remember: episode
        Story,   // something only the speaker went through or saw: episode
        Someone, // a third human: who, and the speaker's opinion of them
        Feeling, // how it is doing: feel
        Way,     // whether there is a way out, and which way it thinks
        Group,   // the two of them together: who leads, how far they came
        Before,  // nobody remembers anything from before waking
        Count
    };
    /// For Here.
    enum class Thing : uint8_t {
        Statue, // detail: StatueKind * 16 + StatueMaterial
        Water,  // a pool or canal; detail: 1 if fouled
        Basin,  // detail: 1 if fouled
        Body,   // who: whose
        Blood,  // a pool of blood seen
        Mess,   // detail: 1 for feces, 0 for piss
        Count
    };
    /// For Feeling.
    enum class Feel : uint8_t {
        Peckish,  // getting hungry, not yet looking for food
        Dry,      // getting thirsty
        Footsore, // walking a long while without a rest
        Uneasy,   // dread
        Gloomy,   // losing hope
        Hopeful,  // sure there's a way out
        Count
    };
    /// How the listener answers, from its own memory and mind (see
    /// Human::replyTo): it remembers or thinks the same, has something of
    /// its own to add, thinks otherwise, knows nothing of it, or can't be
    /// bothered.
    enum class Reply : uint8_t { None, Agree, Add, Disagree, Unheard, Brush, Count };
    Kind kind;
    /// The conversation it starts: the line it is on (1 the opener, 2 the
    /// answer, 3 the opener's follow-up, 4 the answer's close), how many
    /// lines it runs to, and the answer given.
    int turn = 1, turns = 2;
    Reply reply = Reply::None;
    /// How much it matters to the speaker; the highest is brought up.
    float score = 0.0f;
    Episode episode{};
    Thing thing = Thing::Statue;
    Feel feel = Feel::Peckish;
    Vec2i cell{};
    int detail = 0;
    int who = -1;
    float opinion = 0.0f;
    /// For Group: how many places they went into together.
    int count = 0;
    /// What it is about, so it isn't raised with the same human again until
    /// something changes.
    uint64_t key = 0;
};

const char* topicName(Topic::Kind k);
