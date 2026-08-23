#include "procedural/world/WorldPlacer.h"
#include <algorithm>
#include <cmath>

namespace procengine {

WorldPlacer::WorldPlacer(Seed worldSeed, float terrainSize, float spacing)
    : worldSeed_(worldSeed)
    , terrainSize_(terrainSize)
    , spacing_(spacing)
    , halfSize_(terrainSize * spacing * 0.5f) {}

void WorldPlacer::setTerrainSurface(const TerrainSurface* surface) {
    terrainSurface_ = surface;
}

void WorldPlacer::reserveAABB(const glm::vec3& center, float halfW, float halfD) {
    exclusions_.push_back({center, halfW, halfD});
}

void WorldPlacer::reserveCircle(const glm::vec3& center, float radius) {
    exclusions_.push_back({center, radius, radius});
}

float WorldPlacer::sampleTerrainHeight(float x, float z) const {
    if (terrainSurface_) {
        return terrainSurface_->getHeight(x, z);
    }
    return 0.0f;
}

float WorldPlacer::sampleTerrainHeightFootprint(float x, float z, float halfW, float halfD) const {
    if (terrainSurface_) {
        return terrainSurface_->getHeightAtFootprint(x, z, halfW, halfD);
    }
    return 0.0f;
}

bool WorldPlacer::isExcluded(float x, float z) const {
    for (const auto& zone : exclusions_) {
        float dx = std::abs(x - zone.center.x);
        float dz = std::abs(z - zone.center.z);
        if (dx < zone.halfW && dz < zone.halfD)
            return true;
    }
    return false;
}

bool WorldPlacer::tooCloseToExisting(float x, float z, float minSpacing) const {
    float minDist2 = minSpacing * minSpacing;
    for (const auto& obj : placed_) {
        float dx = x - obj.position.x;
        float dz = z - obj.position.z;
        if (dx * dx + dz * dz < minDist2)
            return true;
    }
    return false;
}

std::vector<WorldObject> WorldPlacer::placeTrees(int count, float minSpacing) {
    std::vector<WorldObject> result;
    Rng rng(worldSeed_ + 100);
    float edgeMargin = 3.0f;
    int maxAttempts = count * 20;

    for (int i = 0; i < maxAttempts && static_cast<int>(result.size()) < count; i++) {
        float x = rng.range(-halfSize_ + edgeMargin, halfSize_ - edgeMargin);
        float z = rng.range(-halfSize_ + edgeMargin, halfSize_ - edgeMargin);

        if (isExcluded(x, z)) continue;
        if (tooCloseToExisting(x, z, minSpacing)) continue;

        float y = terrainSurface_ ? terrainSurface_->getHeight(x, z) : 0.0f;

        WorldObject obj;
        obj.position = glm::vec3(x, y, z);
        obj.seed = rng.next();
        obj.type = ObjectType::Tree;
        result.push_back(obj);
        placed_.push_back(obj);
    }
    return result;
}

std::vector<WorldObject> WorldPlacer::placeRocks(int count, float minSpacing) {
    std::vector<WorldObject> result;
    Rng rng(worldSeed_ + 300);
    float edgeMargin = 3.0f;
    int maxAttempts = count * 20;

    for (int i = 0; i < maxAttempts && static_cast<int>(result.size()) < count; i++) {
        float x = rng.range(-halfSize_ + edgeMargin, halfSize_ - edgeMargin);
        float z = rng.range(-halfSize_ + edgeMargin, halfSize_ - edgeMargin);

        if (isExcluded(x, z)) continue;
        if (tooCloseToExisting(x, z, minSpacing)) continue;

        float y = terrainSurface_ ? terrainSurface_->getHeight(x, z) : 0.0f;

        WorldObject obj;
        obj.position = glm::vec3(x, y, z);
        obj.seed = rng.next();
        obj.type = ObjectType::Rock;
        result.push_back(obj);
        placed_.push_back(obj);
    }
    return result;
}

}