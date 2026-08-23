#pragma once

#include "engine/renderer/Mesh.h"
#include "procedural/core/Seed.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/TerrainDelta.h"
#include "procedural/world/WorldSeed.h"
#include <cstdint>
#include <vector>

namespace procengine {

class ChunkGenerator {
public:
    static constexpr float DEFAULT_CHUNK_SIZE = 32.0f;
    static constexpr int   DEFAULT_GRID_SIZE  = 32;
    static constexpr float DEFAULT_HEIGHT_SCALE = 6.0f;

    MeshData generate(WorldSeed world,
                      ChunkCoord cc,
                      float chunkSize = DEFAULT_CHUNK_SIZE,
                      int gridSize = DEFAULT_GRID_SIZE,
                      float heightScale = DEFAULT_HEIGHT_SCALE) const;

    static float queryHeight(WorldSeed world, float wx, float wz,
                             float heightScale = DEFAULT_HEIGHT_SCALE);

    static void applyDeltas(MeshData& mesh, const std::vector<TerrainDelta>& deltas,
                            int gridSize, float chunkSize, float heightScale);

    static void recomputeNormalsAndColors(MeshData& mesh, int gridSize, float chunkSize,
                                          float heightScale);
};

}