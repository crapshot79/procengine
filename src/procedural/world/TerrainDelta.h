#pragma once

#include <cstdint>

namespace procengine {

struct TerrainDelta {
    int32_t localX = 0;
    int32_t localZ = 0;
    float newHeight = 0.0f;
};

}
