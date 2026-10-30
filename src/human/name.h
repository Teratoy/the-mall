#pragma once
#include <cstdint>
#include <string>

/// A made-up name, 3 to 12 letters, grown letter by letter from a Markov
/// chain over the names in data/names.txt: each letter is drawn by how often
/// it follows the three before it in those names, so the result sounds like
/// a name without having to be one. The same seed always gives the same name.
std::string randomName(uint64_t seed);
