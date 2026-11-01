#include "world/light.h"
#include <cmath>

int lightLevel(LightKind kind, float d) {
    int radius = lightRadius(kind);
    if (d > float(radius)) return 0;
    return LIGHT_FULL - int(std::floor(float(LIGHT_FULL) * d / float(radius + 1)));
}
