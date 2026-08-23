#include "procedural/world/ChunkGenerator.h"
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

inline float fbm(WorldSeed seed, float wx, float wz) {
    float h = 0.0f;
    h += valueNoiseAt(seed, wx * (1.0f / 8.0f),  wz * (1.0f / 8.0f))  * 0.5f;
    h += valueNoiseAt(seed, wx * (1.0f / 4.0f),  wz * (1.0f / 4.0f))  * 0.25f;
    h += valueNoiseAt(seed, wx * (1.0f / 2.0f),  wz * (1.0f / 2.0f))  * 0.125f;
    h += valueNoiseAt(seed, wx * (1.0f / 1.0f),  wz * (1.0f / 1.0f))  * 0.0625f;
    return h;
}

inline float terrainHeight(WorldSeed seed, float wx, float wz, float heightScale) {
    return fbm(seed, wx, wz) * heightScale;
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

inline glm::vec3 terrainColor(float height, float maxHeight) {
    float t = height / maxHeight;
    if (t < 0.15f) {
        return glm::vec3(0.55f, 0.7f, 0.3f);
    } else if (t < 0.4f) {
        float s = (t - 0.15f) / 0.25f;
        return glm::vec3(0.25f + s * 0.15f, 0.55f - s * 0.1f, 0.15f);
    } else if (t < 0.7f) {
        float s = (t - 0.4f) / 0.3f;
        return glm::vec3(0.45f + s * 0.15f, 0.4f + s * 0.1f, 0.25f + s * 0.15f);
    } else {
        float s = (t - 0.7f) / 0.3f;
        return glm::vec3(0.6f + s * 0.3f, 0.55f + s * 0.35f, 0.45f + s * 0.45f);
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

            float h = terrainHeight(world, wx, wz, heightScale);

            mesh.vertices[idx].pos = glm::vec3(wx, h, wz);
            mesh.vertices[idx].color = terrainColor(h, heightScale);
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
                                                float heightScale) {
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
            mesh.vertices[idx].color = terrainColor(mesh.vertices[idx].pos.y, heightScale);
        }
    }
}

}
