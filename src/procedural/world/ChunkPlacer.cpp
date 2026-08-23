#include "procedural/world/ChunkPlacer.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/WorldSeed.h"

#include <algorithm>
#include <cmath>

namespace procengine {

namespace {

bool tooClose(const std::vector<PlacedObject>& placed, float x, float z, float minDist) {
    float minDistSq = minDist * minDist;
    for (const auto& obj : placed) {
        float dx = obj.position.x - x;
        float dz = obj.position.z - z;
        if (dx * dx + dz * dz < minDistSq) return true;
    }
    return false;
}

void placeType(std::vector<PlacedObject>& out,
               WorldSeed world, ChunkCoord cc,
               PlacedType type, uint64_t domainTag,
               int count, float minSpacing, float chunkSize, float heightScale) {
    float originX = static_cast<float>(cc.x) * chunkSize;
    float originZ = static_cast<float>(cc.z) * chunkSize;
    float margin  = 0.5f;

    for (int i = 0; i < count; ++i) {
        Seed objSeed = objectSeed(world, cc, static_cast<uint64_t>(i));
        ObjectId oid = makeObjectId(world, cc, domainTag, static_cast<uint64_t>(i));
        Rng rng(objSeed);

        for (int attempt = 0; attempt < 500; ++attempt) {
            float lx = margin + rng.nextFloat() * (chunkSize - 2.0f * margin);
            float lz = margin + rng.nextFloat() * (chunkSize - 2.0f * margin);
            float wx = originX + lx;
            float wz = originZ + lz;

            if (tooClose(out, wx, wz, minSpacing)) continue;

            float wy = ChunkGenerator::queryHeight(world, wx, wz, heightScale);
            out.push_back({oid, glm::vec3(wx, wy, wz), objSeed, type});
            break;
        }
    }
}

}

std::vector<PlacedObject> ChunkPlacer::place(WorldSeed world, ChunkCoord cc, const Config& cfg) {
    std::vector<PlacedObject> result;
    result.reserve(cfg.treeCount + cfg.rockCount);

    placeType(result, world, cc, PlacedType::Tree, DOMAIN_TREE,
              cfg.treeCount, cfg.treeSpacing, cfg.chunkSize, cfg.heightScale);
    placeType(result, world, cc, PlacedType::Rock, DOMAIN_ROCK,
              cfg.rockCount, cfg.rockSpacing, cfg.chunkSize, cfg.heightScale);

    return result;
}

}
