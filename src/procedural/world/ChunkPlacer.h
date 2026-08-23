#pragma once

#include "engine/renderer/Mesh.h"
#include "procedural/core/Seed.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/WorldSeed.h"
#include <glm/glm.hpp>
#include <vector>

namespace procengine {

enum class PlacedType { Tree, Rock };

struct PlacedObject {
    ObjectId id;
    glm::vec3 position;
    Seed seed;
    PlacedType type;
};

class ChunkPlacer {
public:
    static constexpr uint64_t DOMAIN_TREE = 0x54524545ULL;
    static constexpr uint64_t DOMAIN_ROCK = 0x524F434BULL;

    struct Config {
        float chunkSize    = 32.0f;
        float heightScale  = 6.0f;
        int   treeCount    = 16;
        int   rockCount    = 12;
        float treeSpacing  = 4.0f;
        float rockSpacing  = 2.5f;
    };

    std::vector<PlacedObject> place(WorldSeed world, ChunkCoord cc,
                                     const Config& cfg = Config{});
};

}
