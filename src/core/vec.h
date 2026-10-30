#pragma once
#include <cstddef>
#include <cstdint>

/// A cell or slot on an integer grid.
struct Vec2i {
    int x = 0, y = 0;

    constexpr Vec2i() = default;
    constexpr Vec2i(int x, int y) : x(x), y(y) {}
    constexpr Vec2i operator+(Vec2i o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2i operator-(Vec2i o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2i operator-() const { return {-x, -y}; }
    constexpr Vec2i operator*(int k) const { return {x * k, y * k}; }
    constexpr bool operator==(const Vec2i&) const = default;
};

struct Vec2iHash {
    size_t operator()(Vec2i v) const noexcept {
        // splitmix64 over both coordinates, so nearby cells spread over buckets.
        uint64_t z = (uint64_t(uint32_t(v.x)) << 32) | uint32_t(v.y);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return size_t(z ^ (z >> 31));
    }
};

/// Steps between two cells when diagonals count as one (Chebyshev distance).
constexpr int cheb(Vec2i a, Vec2i b) {
    int dx = a.x > b.x ? a.x - b.x : b.x - a.x, dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx > dy ? dx : dy;
}

/// Division rounding toward negative infinity, so cell -1 lies in slot -1.
constexpr int floorDiv(int a, int b) { return (a >= 0 ? a : a - b + 1) / b; }

inline constexpr Vec2i DIRS4[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
inline constexpr Vec2i DIRS8[8] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1},
                                   {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
