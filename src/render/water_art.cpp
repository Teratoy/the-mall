#include "render/water_art.h"
#include "render/blood_art.h"
#include "render/mess_art.h"

namespace {

Color over(Color under, Color c) {
    float a = c.a / 255.0f;
    auto m = [a](unsigned char u, unsigned char v) { return (unsigned char)(u + (v - u) * a); };
    return {m(under.r, c.r), m(under.g, c.g), m(under.b, c.b), under.a};
}

} // namespace

Color waterColor(float red, float foul) {
    Color c = WATER;
    if (red > 0.0f) c = over(c, waterBloodCloud(red));
    if (foul > 0.0f) c = over(c, waterFoulCloud(foul));
    return c;
}
