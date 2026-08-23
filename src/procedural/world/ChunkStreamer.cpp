#include "procedural/world/ChunkStreamer.h"
#include "procedural/vegetation/TreeGenerator.h"
#include "procedural/rock/RockGenerator.h"
#include "engine/renderer/Renderer.h"
#include "engine/renderer/Mesh.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace procengine {

ChunkStreamer::ChunkStreamer(WorldSeed world, float chunkSize, float heightScale, int loadRadius)
    : world_(world), chunkSize_(chunkSize), heightScale_(heightScale), loadRadius_(loadRadius) {}

ChunkCoord ChunkStreamer::worldToChunk(float wx, float wz) const {
    return {
        static_cast<int32_t>(std::floor(wx / chunkSize_)),
        static_cast<int32_t>(std::floor(wz / chunkSize_))
    };
}

void ChunkStreamer::update(const glm::vec3& cameraPos, Renderer& renderer) {
    ChunkCoord camCC = worldToChunk(cameraPos.x, cameraPos.z);

    std::vector<ChunkCoord> desired;
    for (int dz = -loadRadius_; dz <= loadRadius_; ++dz) {
        for (int dx = -loadRadius_; dx <= loadRadius_; ++dx) {
            desired.push_back({camCC.x + dx, camCC.z + dz});
        }
    }

    for (const auto& cc : desired) {
        if (loaded_.find(cc) == loaded_.end()) {
            loadChunk(cc, renderer);
        }
    }

    std::vector<ChunkCoord> toUnload;
    for (auto& [cc, state] : loaded_) {
        bool keep = false;
        for (const auto& d : desired) {
            if (d == cc) { keep = true; break; }
        }
        if (!keep) toUnload.push_back(cc);
    }

    for (const auto& cc : toUnload) {
        unloadChunk(cc, renderer);
    }
}

void ChunkStreamer::loadChunk(const ChunkCoord& cc, Renderer& renderer) {
    ChunkGenerator gen;
    ChunkPlacer placer;
    TreeGenerator treeGen;
    RockGenerator rockGen;

    ChunkState state;
    state.mesh = gen.generate(world_, cc, chunkSize_, ChunkGenerator::DEFAULT_GRID_SIZE, heightScale_);
    state.placements = placer.place(world_, cc);

    state.terrainMesh = renderer.uploadMesh(state.mesh);

    for (const auto& obj : state.placements) {
        MeshData objMesh;
        if (obj.type == PlacedType::Tree) {
            objMesh = treeGen.generate(obj.seed);
        } else {
            objMesh = rockGen.generate(obj.seed);
        }

        float meshMinY = computeMeshMinY(objMesh);
        float worldY = obj.position.y - meshMinY;
        glm::mat4 xform = glm::translate(glm::mat4(1.0f),
            glm::vec3(obj.position.x, worldY, obj.position.z));

        ChunkState::ObjectGpu og;
        og.mesh = renderer.uploadMesh(objMesh);
        og.transform = xform;
        state.objectGpus.push_back(og);
    }

    loaded_[cc] = std::move(state);
}

void ChunkStreamer::unloadChunk(const ChunkCoord& cc, Renderer& renderer) {
    auto it = loaded_.find(cc);
    if (it == loaded_.end()) return;

    ChunkState& state = it->second;
    renderer.destroyMesh(state.terrainMesh);
    for (auto& og : state.objectGpus) {
        renderer.destroyMesh(og.mesh);
    }
    state.objectGpus.clear();

    loaded_.erase(it);
}

void ChunkStreamer::shutdown(Renderer& renderer) {
    std::vector<ChunkCoord> all;
    for (auto& [cc, _] : loaded_) all.push_back(cc);
    for (const auto& cc : all) {
        unloadChunk(cc, renderer);
    }
}

}
