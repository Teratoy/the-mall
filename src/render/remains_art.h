#pragma once
#include "human/human_look.h"
#include <array>
#include <cstdint>

/// Paints what eating and time do to a body onto px, one frame's pixels
/// (blood and all), drawn from frame f. The bites spread out from the belly,
/// to the arms and legs, and the head last of all: torn edges bright, deeper
/// in darker, with ribs and bone showing as more is eaten, and whole bits of
/// limb gone near the end. Rot turns skin pale, then grey-green, then
/// brown, then bare bone with empty eyes, and clothes darken and fall into
/// holes. eaten and decay are 0 to 1; seed keeps each body's bites its own.
/// rest is the standing frame f maps back to, for where the body parts are.
void paintRemains(std::array<Rgba, SPRITE_PX>& px, const SpriteFrame& f, const SpriteFrame& rest, Rgba skin,
                  float eaten, float decay, uint64_t seed);
