#include "render/remains_art.h"
#include "human/wound.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

/// Torn flesh at the edge of a bite, flesh deeper in, and either once old.
constexpr Rgba MEAT_EDGE = hex(0xa3262a);
constexpr Rgba MEAT = hex(0x6e1216);
constexpr Rgba MEAT_OLD = hex(0x3a1c14);
constexpr Rgba BONE = hex(0xd8cfb4);
/// Skin as it goes: pale, livid grey-green, then brown; and what shows
/// through a hole in rotten clothes, or an empty eye.
constexpr Rgba PALE = hex(0xc9c6bb);
constexpr Rgba LIVID = hex(0x7f8a62);
constexpr Rgba ROT = hex(0x5a5236);
constexpr Rgba HOLE = hex(0x221d18);
/// The way to bare bones, 0 to 1: pale this far along (by this much),
/// livid by LIVID_BY, brown by ROT_BY; bone starts showing through the skin
/// from BONE_FROM, and the eyes are empty from ROT_BY.
constexpr float PALE_BY = 0.17f, PALE_MUCH = 0.6f;
constexpr float LIVID_BY = 0.5f, ROT_BY = 0.8f, BONE_FROM = 0.6f;
/// Bites spread this many pixels out from the belly over the whole meal;
/// the head and the feet come this much later; their edge is this ragged.
constexpr float BITE_REACH = 11.0f;
constexpr float HEAD_LATER = 0.35f, FEET_LATER = 0.25f;
constexpr float RAGGED = 0.15f;
/// At the end, what is left of the head is still left.
constexpr float BITE_END = 1.15f;
/// Eaten this far, ribs and bone show deep in; from LIMBS_GO_FROM, bits of
/// arm and leg are gone altogether, this many more for each bit further.
constexpr float RIBS_FROM = 0.4f, BONE_DEEP = 0.25f;
constexpr float LIMBS_GO_FROM = 0.66f, LIMBS_GO = 1.5f;
/// Rot makes flesh this much older for each bit of the way.
constexpr float MEAT_AGES = 1.5f;
/// Rotten through, clothes are this much darker and greyer, with holes
/// from HOLES_FROM, this many more for each bit further.
constexpr float CLOTH_DARKEN = 0.4f, CLOTH_GREY = 0.5f;
constexpr float HOLES_FROM = 0.7f, HOLES = 1.2f;
/// Skin is skin shaded by anything in this range, give or take this much
/// a channel; shaded below EYE_SHADE on the head, it is a shut eye.
constexpr float SKIN_SHADE_MIN = 0.5f, SKIN_SHADE_MAX = 1.2f;
constexpr int SKIN_SLACK = 10;
constexpr float EYE_SHADE = 0.8f;

/// 0 to 1, the same for the same seed and pixel.
float hash(uint64_t seed, int i) {
    uint64_t z = seed + 0x9e3779b97f4a7c15ULL * uint64_t(i + 1);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z ^= z >> 31;
    return float(z >> 40) / float(1ULL << 24);
}

Rgba mix(Rgba a, Rgba b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto m = [t](uint8_t x, uint8_t y) { return uint8_t(std::lround(x + (y - x) * t)); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), a.a};
}

Rgba shade(Rgba c, float k) {
    auto s = [k](uint8_t v) { return uint8_t(std::clamp(std::lround(v * k), 0L, 255L)); };
    return {s(c.r), s(c.g), s(c.b), c.a};
}

bool same(Rgba a, Rgba b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

/// Whether p is the skin, shaded by k.
bool isSkin(Rgba p, Rgba skin, float& k) {
    int sum = skin.r + skin.g + skin.b;
    if (!p.solid() || sum == 0) return false;
    k = float(p.r + p.g + p.b) / float(sum);
    if (k < SKIN_SHADE_MIN || k > SKIN_SHADE_MAX) return false;
    return std::abs(p.r - skin.r * k) <= SKIN_SLACK && std::abs(p.g - skin.g * k) <= SKIN_SLACK &&
           std::abs(p.b - skin.b * k) <= SKIN_SLACK;
}

/// Unshaded skin this far gone.
Rgba rotten(Rgba skin, float d) {
    if (d < PALE_BY) return mix(skin, PALE, d / PALE_BY * PALE_MUCH);
    Rgba pale = mix(skin, PALE, PALE_MUCH);
    if (d < LIVID_BY) return mix(pale, LIVID, (d - PALE_BY) / (LIVID_BY - PALE_BY));
    return mix(LIVID, ROT, (d - LIVID_BY) / (ROT_BY - LIVID_BY));
}

bool limb(uint8_t part) {
    return part == uint8_t(BodyPart::FrontArm) || part == uint8_t(BodyPart::BackArm) ||
           part == uint8_t(BodyPart::Legs) || part == uint8_t(BodyPart::Feet);
}

} // namespace

void paintRemains(std::array<Rgba, SPRITE_PX>& px, const SpriteFrame& f, const SpriteFrame& rest, Rgba skin,
                  float eaten, float decay, uint64_t seed) {
    if (eaten <= 0.0f && decay <= 0.0f) return;
    // The belly: the middle of the torso, standing.
    float bx = 0.0f, by = 0.0f;
    int n = 0;
    for (int r = 0; r < SPRITE_PX; r++)
        if (rest.part[size_t(r)] == uint8_t(BodyPart::Torso)) {
            bx += float(r % SPRITE_W);
            by += float(r / SPRITE_W);
            n++;
        }
    if (n) {
        bx /= float(n);
        by /= float(n);
    }

    // Which bits of the standing body are eaten.
    std::array<bool, SPRITE_PX> bitten{};
    if (eaten > 0.0f)
        for (int r = 0; r < SPRITE_PX; r++) {
            uint8_t part = rest.part[size_t(r)];
            if (part == NO_PART) continue;
            float d = std::hypot(float(r % SPRITE_W) - bx, float(r / SPRITE_W) - by) / BITE_REACH +
                      (part == uint8_t(BodyPart::Head)   ? HEAD_LATER
                       : part == uint8_t(BodyPart::Feet) ? FEET_LATER
                                                         : 0.0f) +
                      (hash(seed, r) - 0.5f) * 2.0f * RAGGED;
            bitten[size_t(r)] = d < eaten * BITE_END;
        }
    // At the edge of a bite: next to body not bitten.
    auto edge = [&](int r) {
        int x = r % SPRITE_W, y = r / SPRITE_W;
        for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= SPRITE_W || ny >= SPRITE_H) continue;
            size_t o = size_t(ny * SPRITE_W + nx);
            if (rest.part[o] != NO_PART && !bitten[o]) return true;
        }
        return false;
    };

    for (size_t i = 0; i < px.size(); i++) {
        int r = f.rest[i];
        if (r < 0) continue;
        uint8_t part = rest.part[size_t(r)];
        int y = r / SPRITE_W;
        float h = hash(seed ^ 0x5bd1e995ULL, r);
        if (bitten[size_t(r)]) {
            bool rim = edge(r);
            if (!rim && limb(part) && eaten >= LIMBS_GO_FROM && h < (eaten - LIMBS_GO_FROM) * LIMBS_GO) {
                px[i].a = 0;
                continue;
            }
            bool bone = !rim && eaten >= RIBS_FROM && ((part == uint8_t(BodyPart::Torso) && y % 2 == 0) || h < BONE_DEEP);
            Rgba c = bone ? BONE : mix(rim ? MEAT_EDGE : MEAT, MEAT_OLD, decay * MEAT_AGES);
            px[i] = {c.r, c.g, c.b, 255};
            continue;
        }
        if (decay <= 0.0f) continue;
        float k = 1.0f;
        if (isSkin(f.px[i], skin, k)) {
            Rgba c = shade(rotten(skin, decay), k);
            if (decay >= BONE_FROM && h < (decay - BONE_FROM) / (1.0f - BONE_FROM)) c = shade(BONE, std::min(1.0f, k));
            if (part == uint8_t(BodyPart::Head) && k < EYE_SHADE && decay >= ROT_BY) c = HOLE;
            // Blood on it shows through.
            px[i] = same(px[i], f.px[i]) ? Rgba{c.r, c.g, c.b, px[i].a} : mix(px[i], c, 0.5f);
            continue;
        }
        // Clothes, hair and shoes: darker and greyer, and in the end holey.
        Rgba c = px[i];
        uint8_t grey = uint8_t((c.r * 299 + c.g * 587 + c.b * 114) / 1000);
        c = shade(mix(c, {grey, grey, grey, c.a}, CLOTH_GREY * decay), 1.0f - CLOTH_DARKEN * decay);
        if (part != uint8_t(BodyPart::Head) && decay >= HOLES_FROM && h < (decay - HOLES_FROM) * HOLES)
            c = part == uint8_t(BodyPart::Torso) && y % 2 == 0 ? BONE : HOLE;
        px[i] = {c.r, c.g, c.b, px[i].a};
    }
}
