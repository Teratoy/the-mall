#pragma once
#include <cstdint>

/// The mall's own day (AI/light_plan.md step 6): MALL_HOURS hours of
/// MALL_HOUR seconds each, 20 minutes in all. It is open from OPENING to
/// midnight, and closed from midnight until OPENING, a quarter of the day,
/// with its ceiling lights out but for a few night lights. The world begins
/// at opening on the first day.
inline constexpr float MALL_HOUR = 50.0f;
inline constexpr int MALL_HOURS = 24;
inline constexpr float MALL_DAY = MALL_HOUR * MALL_HOURS;
inline constexpr int OPENING = 6;

/// A bell rings each mall hour; at opening and at closing (midnight) it is
/// another bell.
enum class Bell : uint8_t { Hour, Opening, Closing };
