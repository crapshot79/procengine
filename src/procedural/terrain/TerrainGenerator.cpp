#include "procedural/terrain/TerrainGenerator.h"
#include <cmath>
#include <algorithm>

namespace procengine {

MeshData TerrainGenerator::generate(Seed seed, int gridSize, float spacing, float heightScale) {
    MeshData mesh;
    Rng rng(seed);
    terrainSeed_ = seed;

    int vertCount = (gridSize + 1) * (gridSize + 1);
    mesh.vertices.resize(vertCount);

    float halfSize = gridSize * spacing * 0.5f;

    for (int z = 0; z <= gridSize; z++) {
        for (int x = 0; x <= gridSize; x++) {
            int idx = z * (gridSize + 1) + x;
            float wx = x * spacing - halfSize;
            float wz = z * spacing - halfSize;

            float h = getHeight(rng, static_cast<float>(x) / gridSize, static_cast<float>(z) / gridSize) * heightScale;

            mesh.vertices[idx].pos = glm::vec3(wx, h, wz);
            mesh.vertices[idx].color = getTerrainColor(h, heightScale);
            mesh.vertices[idx].normal = glm::vec3(0.0f, 1.0f, 0.0f);
            mesh.vertices[idx].materialType = MATERIAL_TERRAIN;
        }
    }

    for (int z = 0; z < gridSize; z++) {
        for (int x = 0; x < gridSize; x++) {
            int topLeft = z * (gridSize + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = topLeft + (gridSize + 1);
            int bottomRight = bottomLeft + 1;

            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);

            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }

    for (auto& v : mesh.vertices) {
        v.normal = glm::vec3(0.0f);
    }

    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];

        glm::vec3 v0 = mesh.vertices[i0].pos;
        glm::vec3 v1 = mesh.vertices[i1].pos;
        glm::vec3 v2 = mesh.vertices[i2].pos;

        glm::vec3 e1 = v1 - v0;
        glm::vec3 e2 = v2 - v0;
        glm::vec3 faceNormal = glm::cross(e1, e2);

        mesh.vertices[i0].normal += faceNormal;
        mesh.vertices[i1].normal += faceNormal;
        mesh.vertices[i2].normal += faceNormal;
    }

    for (auto& v : mesh.vertices) {
        float len = glm::length(v.normal);
        if (len > 0.0f) {
            v.normal = v.normal / len;
        } else {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    return mesh;
}

float TerrainGenerator::noise2D(Rng& rng, float x, float z) {
    (void)rng;
    int ix = static_cast<int>(std::floor(x));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - ix;
    float fz = z - iz;

    auto hash = [this](int hx, int hz) -> float {
        uint32_t n = static_cast<uint32_t>(hx) * 374761393 + static_cast<uint32_t>(hz) * 668265263;
        n ^= static_cast<uint32_t>(terrainSeed_ & 0xFFFFFFFF);
        n = (n ^ (n >> 13)) * 1274126177;
        n = n ^ (n >> 16);
        return static_cast<float>(n) / static_cast<float>(0xFFFFFFFF);
    };

    float v00 = hash(ix, iz);
    float v10 = hash(ix + 1, iz);
    float v01 = hash(ix, iz + 1);
    float v11 = hash(ix + 1, iz + 1);

    float sx = fx * fx * (3.0f - 2.0f * fx);
    float sz = fz * fz * (3.0f - 2.0f * fz);

    float v0 = v00 + sx * (v10 - v00);
    float v1 = v01 + sx * (v11 - v01);
    return v0 + sz * (v1 - v0);
}

float TerrainGenerator::smoothNoise2D(Rng& rng, float x, float z, float frequency) {
    return noise2D(rng, x * frequency, z * frequency);
}

float TerrainGenerator::getHeight(Rng& rng, float x, float z) {
    float h = 0.0f;
    h += smoothNoise2D(rng, x, z, 4.0f) * 0.5f;
    h += smoothNoise2D(rng, x, z, 8.0f) * 0.25f;
    h += smoothNoise2D(rng, x, z, 16.0f) * 0.125f;
    h += smoothNoise2D(rng, x, z, 32.0f) * 0.0625f;
    return h;
}

glm::vec3 TerrainGenerator::getTerrainColor(float height, float maxHeight) {
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