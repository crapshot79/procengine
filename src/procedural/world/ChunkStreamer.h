#pragma once

#include "engine/renderer/Mesh.h"
#include "procedural/core/Seed.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/ChunkPlacer.h"
#include "procedural/world/WorldSeed.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace procengine {

class WorldStore;

struct ChunkState {
    MeshData mesh;
    std::vector<PlacedObject> placements;

    struct ObjectGpu {
        GpuMesh mesh;
        glm::mat4 transform;
    };

    GpuMesh terrainMesh = {};
    std::vector<ObjectGpu> objectGpus;
};

class ChunkStreamer {
public:
    ChunkStreamer(WorldSeed world, float chunkSize, float heightScale, int loadRadius);

    void setWorldStore(WorldStore* store) { store_ = store; }

    ChunkCoord worldToChunk(float wx, float wz) const;

    void update(const glm::vec3& cameraPos, class Renderer& renderer);
    void reloadChunk(const ChunkCoord& cc, Renderer& renderer);

    const std::unordered_map<ChunkCoord, ChunkState, ChunkCoordHash>& getLoaded() const { return loaded_; }
    size_t loadedCount() const { return loaded_.size(); }

    void shutdown(class Renderer& renderer);

private:
    void loadChunk(const ChunkCoord& cc, Renderer& renderer);
    void unloadChunk(const ChunkCoord& cc, Renderer& renderer);

    WorldSeed world_;
    float chunkSize_;
    float heightScale_;
    int loadRadius_;
    WorldStore* store_ = nullptr;
    std::unordered_map<ChunkCoord, ChunkState, ChunkCoordHash> loaded_;
};

}
