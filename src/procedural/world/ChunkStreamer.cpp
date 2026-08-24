#include "procedural/world/ChunkStreamer.h"
#include "procedural/world/WorldStore.h"
#include "procedural/world/TerrainDelta.h"
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

void ChunkStreamer::reloadChunk(const ChunkCoord& cc, Renderer& renderer) {
    unloadChunk(cc, renderer);
    loadChunk(cc, renderer);
}

void ChunkStreamer::loadChunk(const ChunkCoord& cc, Renderer& renderer) {
    ChunkGenerator gen;
    ChunkPlacer placer;
    TreeGenerator treeGen;
    RockGenerator rockGen;

    ChunkState state;
    state.mesh = gen.generate(world_, cc, chunkSize_, ChunkGenerator::DEFAULT_GRID_SIZE, heightScale_);
    state.placements = placer.place(world_, cc);

    std::vector<uint64_t> removedKeys;
    std::vector<TerrainDelta> terrainDeltas;
    if (store_) {
        removedKeys = store_->getRemovals(cc);
        terrainDeltas = store_->getTerrainDeltas(cc);
    }

    if (!terrainDeltas.empty()) {
        ChunkGenerator::applyDeltas(state.mesh, terrainDeltas,
                                     ChunkGenerator::DEFAULT_GRID_SIZE, chunkSize_, heightScale_);
        ChunkGenerator::recomputeNormalsAndColors(state.mesh, ChunkGenerator::DEFAULT_GRID_SIZE,
                                                   chunkSize_, heightScale_, world_);
    }

    auto isRemoved = [&](const PlacedObject& obj) -> bool {
        uint64_t key = (obj.id.hi << 1) ^ obj.id.lo;
        for (uint64_t rk : removedKeys) {
            if (rk == key) return true;
        }
        return false;
    };

    state.terrainMesh = renderer.uploadMesh(state.mesh);

    state.surface.build(ChunkGenerator::DEFAULT_GRID_SIZE,
                        chunkSize_ / ChunkGenerator::DEFAULT_GRID_SIZE,
                        state.mesh.vertices.data(),
                        static_cast<int>(state.mesh.vertices.size()));

    for (const auto& obj : state.placements) {
        if (isRemoved(obj)) continue;

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

    if (store_) {
        auto& added = store_->getAddedObjects(cc);
        for (const auto& stored : added) {
            MeshData objMesh;
            if (stored.type == 1) {
                objMesh = treeGen.generate(stored.seed);
            } else {
                objMesh = rockGen.generate(stored.seed);
            }

            float meshMinY = computeMeshMinY(objMesh);
            float worldY = stored.posY - meshMinY;

            glm::mat4 xform = glm::mat4(1.0f);
            xform = glm::translate(xform, glm::vec3(stored.posX, worldY, stored.posZ));
            if (stored.rotY != 0.0f) {
                xform = glm::rotate(xform, stored.rotY, glm::vec3(0, 1, 0));
            }
            if (stored.scaleX != 1.0f || stored.scaleY != 1.0f || stored.scaleZ != 1.0f) {
                xform = glm::scale(xform, glm::vec3(stored.scaleX, stored.scaleY, stored.scaleZ));
            }

            ChunkState::ObjectGpu og;
            og.mesh = renderer.uploadMesh(objMesh);
            og.transform = xform;
            state.objectGpus.push_back(og);
        }
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

float ChunkStreamer::queryHeight(float wx, float wz) const {
    ChunkCoord cc = worldToChunk(wx, wz);
    auto it = loaded_.find(cc);
    if (it != loaded_.end()) {
        float halfChunk = chunkSize_ * 0.5f;
        float localX = wx - static_cast<float>(cc.x) * chunkSize_ - halfChunk;
        float localZ = wz - static_cast<float>(cc.z) * chunkSize_ - halfChunk;
        return it->second.surface.getHeight(localX, localZ);
    }
    return ChunkGenerator::queryHeight(world_, wx, wz, heightScale_);
}

}
