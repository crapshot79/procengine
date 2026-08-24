#pragma once

#include "procedural/world/Biome.h"
#include "procedural/world/WorldSeed.h"

namespace procengine {

class BiomeGenerator {
public:
    static constexpr float BIOME_SCALE = 320.0f;
    static constexpr float DEFAULT_BIOME_SCALE = BIOME_SCALE;

    static BiomeSample sampleBiome(WorldSeed world, float wx, float wz,
                                   float scale = DEFAULT_BIOME_SCALE);
    static float treeDensity(WorldSeed world, float wx, float wz,
                             float scale = DEFAULT_BIOME_SCALE);
    static float rockDensity(WorldSeed world, float wx, float wz,
                             float scale = DEFAULT_BIOME_SCALE);
};

}
