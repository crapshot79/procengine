#include "procedural/world/BiomeGenerator.h"
#include "procedural/world/WorldSeed.h"

#include <cmath>
#include <algorithm>

namespace procengine {

namespace {

inline uint32_t hash2D(WorldSeed seed, int32_t ix, int32_t iz) {
    uint64_t h = mixSplitMix64(static_cast<uint64_t>(seed));
    uint64_t k = mixSplitMix64((static_cast<uint64_t>(static_cast<uint32_t>(ix)) << 32) |
                                static_cast<uint32_t>(iz));
    uint64_t m = mixSplitMix64(h ^ k);
    return static_cast<uint32_t>(m >> 32);
}

inline float hash2DFloat(WorldSeed seed, int32_t ix, int32_t iz) {
    return static_cast<float>(hash2D(seed, ix, iz)) / static_cast<float>(0xFFFFFFFFu);
}

inline float valueNoiseAt(WorldSeed seed, float wx, float wz) {
    int32_t ix = static_cast<int32_t>(std::floor(wx));
    int32_t iz = static_cast<int32_t>(std::floor(wz));
    float fx = wx - static_cast<float>(ix);
    float fz = wz - static_cast<float>(iz);

    float v00 = hash2DFloat(seed, ix,     iz);
    float v10 = hash2DFloat(seed, ix + 1, iz);
    float v01 = hash2DFloat(seed, ix,     iz + 1);
    float v11 = hash2DFloat(seed, ix + 1, iz + 1);

    float sx = fx * fx * (3.0f - 2.0f * fx);
    float sz = fz * fz * (3.0f - 2.0f * fz);

    float a = v00 + sx * (v10 - v00);
    float b = v01 + sx * (v11 - v01);
    return a + sz * (b - a);
}

}

BiomeType BiomeSample::dominant() const {
    if (grasslandWeight >= forestWeight && grasslandWeight >= highlandWeight)
        return BiomeType::Grassland;
    if (forestWeight >= grasslandWeight && forestWeight >= highlandWeight)
        return BiomeType::Forest;
    return BiomeType::Highland;
}

BiomeSample BiomeGenerator::sampleBiome(WorldSeed world, float wx, float wz, float scale) {
    WorldSeed biomeSeed = world + 0xB10B1E5ULL;

    float nx = valueNoiseAt(biomeSeed, wx / scale, wz / scale);
    float nz = valueNoiseAt(biomeSeed + 12345, wx / (scale * 0.75f), wz / (scale * 0.75f));

    constexpr float GRASS_CX = 0.5f, GRASS_CZ = 0.5f;
    constexpr float FOREST_CX = 0.1f, FOREST_CZ = 0.85f;
    constexpr float HIGHL_CX = 0.9f, HIGHL_CZ = 0.15f;

    float dgx = nx - GRASS_CX, dgz = nz - GRASS_CZ;
    float dgx2 = dgx * dgx + dgz * dgz;

    float dfx = nx - FOREST_CX, dfz = nz - FOREST_CZ;
    float dfx2 = dfx * dfx + dfz * dfz;

    float dhx = nx - HIGHL_CX, dhz = nz - HIGHL_CZ;
    float dhx2 = dhx * dhx + dhz * dhz;

    constexpr float sigma = 0.24f;
    constexpr float invTwoSigmaSq = 1.0f / (2.0f * sigma * sigma);
    float gw = std::exp(-dgx2 * invTwoSigmaSq);
    float fw = std::exp(-dfx2 * invTwoSigmaSq);
    float hw = std::exp(-dhx2 * invTwoSigmaSq);

    float total = gw + fw + hw;
    return {gw / total, fw / total, hw / total};
}

float BiomeGenerator::treeDensity(WorldSeed world, float wx, float wz, float scale) {
    BiomeSample bs = sampleBiome(world, wx, wz, scale);
    return bs.grasslandWeight * 12.0f + bs.forestWeight * 24.0f + bs.highlandWeight * 4.0f;
}

float BiomeGenerator::rockDensity(WorldSeed world, float wx, float wz, float scale) {
    BiomeSample bs = sampleBiome(world, wx, wz, scale);
    return bs.grasslandWeight * 6.0f + bs.forestWeight * 8.0f + bs.highlandWeight * 18.0f;
}

}
