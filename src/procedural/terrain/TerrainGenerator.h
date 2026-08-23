#pragma once

#include "engine/renderer/Mesh.h"
#include "procedural/core/Seed.h"

namespace procengine {

class TerrainGenerator {
public:
    MeshData generate(Seed seed, int gridSize, float spacing, float heightScale);

private:
    float noise2D(Rng& rng, float x, float z);
    float smoothNoise2D(Rng& rng, float x, float z, float frequency);
    float getHeight(Rng& rng, float x, float z);
    glm::vec3 getTerrainColor(float height, float max_height);
    Seed terrainSeed_ = 0;
};

}