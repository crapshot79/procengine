#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/WorldSeed.h"
#include "procedural/world/BiomeGenerator.h"
#include "procedural/world/Biome.h"

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

struct TerrainCharacter {
    float baseElevation;
    float broadAmp;
    float broadScale;
    float hillAmp;
    float hillScale;
    float ridgeAmp;
    float ridgeScale;
    float detailAmp;
    float detailScale;
};

constexpr TerrainCharacter GRASSLAND_TERRAIN{
    0.42f,
    0.26f, 72.0f,
    0.30f, 18.0f,
    0.00f, 160.0f,
    0.06f, 9.0f
};

constexpr TerrainCharacter FOREST_TERRAIN{
    0.50f,
    0.40f, 64.0f,
    0.66f, 15.0f,
    0.28f, 30.0f,
    0.11f, 8.0f
};

constexpr TerrainCharacter HIGHLAND_TERRAIN{
    0.55f,
    0.50f, 68.0f,
    0.84f, 13.0f,
    0.62f, 26.0f,
    0.16f, 7.0f
};

inline float centeredNoise(WorldSeed seed, float wx, float wz, float scale) {
    return valueNoiseAt(seed, wx / scale, wz / scale) - 0.5f;
}

inline float ridgedNoise(WorldSeed seed, float wx, float wz, float scale) {
    float n = valueNoiseAt(seed, wx / scale, wz / scale);
    return 1.0f - std::abs(n * 2.0f - 1.0f);
}

inline float terrainShape(WorldSeed seed, float wx, float wz, const TerrainCharacter& character,
                          uint64_t domain) {
    WorldSeed broadSeed = seed + domain + 0xA110ULL;
    WorldSeed hillSeed = seed + domain + 0xB220ULL;
    WorldSeed ridgeSeed = seed + domain + 0xC330ULL;
    WorldSeed detailSeed = seed + domain + 0xD440ULL;

    float h = character.baseElevation;
    h += centeredNoise(broadSeed, wx, wz, character.broadScale) * character.broadAmp;
    h += centeredNoise(hillSeed, wx, wz, character.hillScale) * character.hillAmp;
    h += (ridgedNoise(ridgeSeed, wx, wz, character.ridgeScale) - 0.45f) * character.ridgeAmp;
    h += centeredNoise(detailSeed, wx, wz, character.detailScale) * character.detailAmp;
    return h;
}

inline float biomeTerrainHeight(WorldSeed seed, float wx, float wz, float heightScale,
                                 const BiomeSample& biome) {
    float grassland = terrainShape(seed, wx, wz, GRASSLAND_TERRAIN, 0x10000ULL);
    float forest = terrainShape(seed, wx, wz, FOREST_TERRAIN, 0x20000ULL);
    float highland = terrainShape(seed, wx, wz, HIGHLAND_TERRAIN, 0x30000ULL);

    float h = grassland * biome.grasslandWeight
            + forest * biome.forestWeight
            + highland * biome.highlandWeight;

    return std::clamp(h, 0.0f, 1.0f) * heightScale;
}

inline float terrainHeight(WorldSeed seed, float wx, float wz, float heightScale) {
    BiomeSample biome = BiomeGenerator::sampleBiome(seed, wx, wz);
    return biomeTerrainHeight(seed, wx, wz, heightScale, biome);
}

inline glm::vec3 terrainNormal(WorldSeed seed, float wx, float wz, float heightScale, float sampleStep) {
    float step = std::max(sampleStep, 0.001f);
    float hL = terrainHeight(seed, wx - step, wz, heightScale);
    float hR = terrainHeight(seed, wx + step, wz, heightScale);
    float hD = terrainHeight(seed, wx, wz - step, heightScale);
    float hU = terrainHeight(seed, wx, wz + step, heightScale);

    glm::vec3 n(hL - hR, 2.0f * step, hD - hU);
    float len = glm::length(n);
    return len > 0.0f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
}

inline glm::vec3 grasslandColor(float t) {
    if (t < 0.15f) {
        return glm::vec3(0.05f, 0.95f, 0.05f);
    } else if (t < 0.4f) {
        float s = (t - 0.15f) / 0.25f;
        return glm::vec3(0.04f + s * 0.04f, 0.88f + s * 0.08f, 0.04f + s * 0.04f);
    } else if (t < 0.7f) {
        float s = (t - 0.4f) / 0.3f;
        return glm::vec3(0.08f + s * 0.10f, 0.96f - s * 0.06f, 0.06f);
    } else {
        float s = (t - 0.7f) / 0.3f;
        return glm::vec3(0.18f + s * 0.10f, 0.90f - s * 0.08f, 0.06f);
    }
}

inline glm::vec3 forestColor(float t) {
    if (t < 0.15f) {
        return glm::vec3(0.00f, 0.16f, 0.04f);
    } else if (t < 0.4f) {
        float s = (t - 0.15f) / 0.25f;
        return glm::vec3(0.00f, 0.12f + s * 0.08f, 0.03f + s * 0.04f);
    } else if (t < 0.7f) {
        float s = (t - 0.4f) / 0.3f;
        return glm::vec3(0.01f + s * 0.03f, 0.20f + s * 0.08f, 0.06f + s * 0.05f);
    } else {
        float s = (t - 0.7f) / 0.3f;
        return glm::vec3(0.04f + s * 0.04f, 0.28f + s * 0.08f, 0.10f + s * 0.06f);
    }
}

inline glm::vec3 highlandColor(float t) {
    if (t < 0.15f) {
        return glm::vec3(0.44f, 0.36f, 0.26f);
    } else if (t < 0.4f) {
        float s = (t - 0.15f) / 0.25f;
        return glm::vec3(0.42f + s * 0.18f, 0.36f + s * 0.16f, 0.28f + s * 0.14f);
    } else if (t < 0.7f) {
        float s = (t - 0.4f) / 0.3f;
        return glm::vec3(0.60f + s * 0.16f, 0.52f + s * 0.14f, 0.42f + s * 0.14f);
    } else {
        float s = (t - 0.7f) / 0.3f;
        return glm::vec3(0.76f + s * 0.16f, 0.66f + s * 0.16f, 0.56f + s * 0.16f);
    }
}

}

MeshData ChunkGenerator::generate(WorldSeed world,
                                  ChunkCoord cc,
                                  float chunkSize,
                                  int gridSize,
                                  float heightScale) const {
    MeshData mesh;
    if (gridSize < 1) gridSize = 1;
    if (chunkSize <= 0.0f) chunkSize = DEFAULT_CHUNK_SIZE;

    int stride = gridSize + 1;
    int vertCount = stride * stride;
    mesh.vertices.resize(vertCount);

    float spacing = chunkSize / static_cast<float>(gridSize);

    float originX = static_cast<float>(cc.x) * chunkSize;
    float originZ = static_cast<float>(cc.z) * chunkSize;

    for (int zi = 0; zi <= gridSize; ++zi) {
        for (int xi = 0; xi <= gridSize; ++xi) {
            int idx = zi * stride + xi;
            float wx = originX + static_cast<float>(xi) * spacing;
            float wz = originZ + static_cast<float>(zi) * spacing;

            BiomeSample biome = BiomeGenerator::sampleBiome(world, wx, wz);
            float h = biomeTerrainHeight(world, wx, wz, heightScale, biome);

            mesh.vertices[idx].pos = glm::vec3(wx, h, wz);
            mesh.vertices[idx].color = biomeColor(h, heightScale, biome);
            mesh.vertices[idx].normal = terrainNormal(world, wx, wz, heightScale, spacing);
            mesh.vertices[idx].materialType = MATERIAL_TERRAIN;
        }
    }

    for (int zi = 0; zi < gridSize; ++zi) {
        for (int xi = 0; xi < gridSize; ++xi) {
            uint32_t topLeft     = static_cast<uint32_t>(zi * stride + xi);
            uint32_t topRight    = topLeft + 1u;
            uint32_t bottomLeft  = static_cast<uint32_t>((zi + 1) * stride + xi);
            uint32_t bottomRight = bottomLeft + 1u;

            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);

            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }

    return mesh;
}

float ChunkGenerator::queryHeight(WorldSeed world, float wx, float wz, float heightScale) {
    return terrainHeight(world, wx, wz, heightScale);
}

glm::vec3 ChunkGenerator::biomeColor(float height, float maxHeight, const BiomeSample& biome) {
    float t = height / maxHeight;
    glm::vec3 gc = grasslandColor(t);
    glm::vec3 fc = forestColor(t);
    glm::vec3 hc = highlandColor(t);
    return gc * biome.grasslandWeight + fc * biome.forestWeight + hc * biome.highlandWeight;
}

void ChunkGenerator::applyDeltas(MeshData& mesh, const std::vector<TerrainDelta>& deltas,
                                  int gridSize, float chunkSize, float heightScale) {
    int stride = gridSize + 1;
    for (const auto& td : deltas) {
        if (td.localX < 0 || td.localX > gridSize) continue;
        if (td.localZ < 0 || td.localZ > gridSize) continue;
        int idx = td.localZ * stride + td.localX;
        mesh.vertices[idx].pos.y = td.newHeight;
    }
}

void ChunkGenerator::recomputeNormalsAndColors(MeshData& mesh, int gridSize, float chunkSize,
                                                float heightScale, WorldSeed world) {
    int stride = gridSize + 1;
    float spacing = chunkSize / static_cast<float>(gridSize);

    for (int zi = 0; zi <= gridSize; ++zi) {
        for (int xi = 0; xi <= gridSize; ++xi) {
            int idx = zi * stride + xi;
            float hL = (xi > 0)            ? mesh.vertices[zi * stride + (xi - 1)].pos.y : mesh.vertices[idx].pos.y;
            float hR = (xi < gridSize)      ? mesh.vertices[zi * stride + (xi + 1)].pos.y : mesh.vertices[idx].pos.y;
            float hD = (zi > 0)            ? mesh.vertices[(zi - 1) * stride + xi].pos.y : mesh.vertices[idx].pos.y;
            float hU = (zi < gridSize)      ? mesh.vertices[(zi + 1) * stride + xi].pos.y : mesh.vertices[idx].pos.y;

            glm::vec3 n(hL - hR, 2.0f * spacing, hD - hU);
            float len = glm::length(n);
            mesh.vertices[idx].normal = len > 0.0f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);

            float wx = mesh.vertices[idx].pos.x;
            float wz = mesh.vertices[idx].pos.z;
            BiomeSample biome = BiomeGenerator::sampleBiome(world, wx, wz);
            mesh.vertices[idx].color = biomeColor(mesh.vertices[idx].pos.y, heightScale, biome);
        }
    }
}

}
