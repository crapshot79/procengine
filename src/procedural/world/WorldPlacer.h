#pragma once

#include "procedural/core/Seed.h"
#include "procedural/world/TerrainSurface.h"
#include <glm/glm.hpp>
#include <vector>

namespace procengine {

enum class ObjectType {
    House,
    Tree,
    Rock
};

struct WorldObject {
    glm::vec3 position;
    Seed seed;
    ObjectType type;
};

class WorldPlacer {
public:
    WorldPlacer(Seed worldSeed, float terrainSize, float spacing);

    void setTerrainSurface(const TerrainSurface* surface);

    void reserveAABB(const glm::vec3& center, float halfW, float halfD);
    void reserveCircle(const glm::vec3& center, float radius);

    std::vector<WorldObject> placeTrees(int count, float minSpacing);
    std::vector<WorldObject> placeRocks(int count, float minSpacing);

    float sampleTerrainHeight(float x, float z) const;
    float sampleTerrainHeightFootprint(float x, float z, float halfW, float halfD) const;

private:
    bool isExcluded(float x, float z) const;
    bool tooCloseToExisting(float x, float z, float minSpacing) const;

    Seed worldSeed_;
    float terrainSize_;
    float spacing_;
    float halfSize_;

    const TerrainSurface* terrainSurface_ = nullptr;

    struct ExclusionZone {
        glm::vec3 center;
        float halfW;
        float halfD;
    };

    std::vector<ExclusionZone> exclusions_;
    std::vector<WorldObject> placed_;
};

}