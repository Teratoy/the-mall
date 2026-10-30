#pragma once
#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

/// Seeded randomness, so a seed always grows the same human or world.
class Rng {
public:
    explicit Rng(uint64_t seed = 0) : e_(seed) {}

    uint64_t next() { return e_(); }
    /// Both ends included.
    int range(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(e_); }
    float range(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(e_); }
    float unit() { return range(0.0f, 1.0f); }
    bool chance(float p) { return unit() < p; }

    template <class T, size_t N>
    const T& pick(const T (&items)[N]) { return items[range(0, int(N) - 1)]; }
    template <class T>
    const T& pick(const std::vector<T>& items) { return items[range(0, int(items.size()) - 1)]; }
    template <class T>
    void shuffle(std::vector<T>& items) { std::shuffle(items.begin(), items.end(), e_); }

    /// Index of a weight, likelier the heavier.
    int weighted(const std::vector<float>& weights) {
        float total = 0.0f;
        for (float w : weights) total += w;
        float r = unit() * total;
        for (size_t i = 0; i < weights.size(); i++) {
            r -= weights[i];
            if (r < 0.0f) return int(i);
        }
        return int(weights.size()) - 1;
    }

private:
    std::mt19937_64 e_;
};
