#pragma once

#include "procedural/world/WorldSeed.h"

namespace procengine {

enum class BiomeType {
    Grassland,
    Forest,
    Highland
};

struct BiomeSample {
    float grasslandWeight = 1.0f;
    float forestWeight = 0.0f;
    float highlandWeight = 0.0f;

    BiomeType dominant() const;
};

}
