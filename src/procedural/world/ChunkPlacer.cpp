#include "procedural/world/ChunkPlacer.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/WorldSeed.h"
#include "procedural/world/BiomeGenerator.h"

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

    float chunkX = static_cast<float>(cc.x) * cfg.chunkSize + cfg.chunkSize * 0.5f;
    float chunkZ = static_cast<float>(cc.z) * cfg.chunkSize + cfg.chunkSize * 0.5f;

    int treeCount = biomeTreeCount(world, chunkX, chunkZ);
    int rockCount = biomeRockCount(world, chunkX, chunkZ);

    result.reserve(treeCount + rockCount);

    placeType(result, world, cc, PlacedType::Tree, DOMAIN_TREE,
              treeCount, cfg.treeSpacing, cfg.chunkSize, cfg.heightScale);
    placeType(result, world, cc, PlacedType::Rock, DOMAIN_ROCK,
              rockCount, cfg.rockSpacing, cfg.chunkSize, cfg.heightScale);

    return result;
}

int ChunkPlacer::biomeTreeCount(WorldSeed world, float chunkX, float chunkZ) {
    float density = BiomeGenerator::treeDensity(world, chunkX, chunkZ);
    return std::max(0, static_cast<int>(density + 0.5f));
}

int ChunkPlacer::biomeRockCount(WorldSeed world, float chunkX, float chunkZ) {
    float density = BiomeGenerator::rockDensity(world, chunkX, chunkZ);
    return std::max(0, static_cast<int>(density + 0.5f));
}

}
