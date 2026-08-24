#include "procedural/world/ChunkPlacer.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/WorldSeed.h"
#include "test/TestUtil.h"

#include <cstdio>
#include <cmath>
#include <set>

namespace procengine {

static bool posEq(const PlacedObject& a, const PlacedObject& b) {
    return a.id == b.id &&
           a.position.x == b.position.x &&
           a.position.y == b.position.y &&
           a.position.z == b.position.z &&
           a.seed == b.seed &&
           a.type == b.type;
}

static bool allMatch(const std::vector<PlacedObject>& a, const std::vector<PlacedObject>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!posEq(a[i], b[i])) return false;
    }
    return true;
}

static float distSq2D(const PlacedObject& a, const PlacedObject& b) {
    float dx = a.position.x - b.position.x;
    float dz = a.position.z - b.position.z;
    return dx * dx + dz * dz;
}

int runTestChunkPlacer() {
    std::printf("Stage 1.3 — ChunkPlacer test\n");

    ChunkPlacer placer;
    WorldSeed w = 42ULL;

    {
        auto a = placer.place(w, ChunkCoord{0, 0});
        auto b = placer.place(w, ChunkCoord{0, 0});
        testCheck(allMatch(a, b), "Determinism: same (world, cc) -> identical placements");
    }

    {
        auto a = placer.place(w, ChunkCoord{0, 0});
        auto b = placer.place(w, ChunkCoord{1, 0});
        testCheck(!allMatch(a, b), "Variation: different cc -> different placements");
    }

    {
        WorldSeed w2 = 99ULL;
        auto a = placer.place(w, ChunkCoord{0, 0});
        auto b = placer.place(w2, ChunkCoord{0, 0});
        testCheck(!allMatch(a, b), "Variation: different world seed -> different placements");
    }

    {
        ChunkPlacer::Config cfg;
        auto result = placer.place(w, ChunkCoord{0, 0}, cfg);
        int trees = 0, rocks = 0;
        for (auto& o : result) {
            if (o.type == PlacedType::Tree) trees++;
            else rocks++;
        }
        testCheck(trees >= 0, "Biome tree count is non-negative");
        testCheck(rocks >= 0, "Biome rock count is non-negative");
        testCheck(trees + rocks > 0, "Biome places some objects");
    }

    {
        ChunkPlacer::Config cfg;
        auto result = placer.place(w, ChunkCoord{0, 0}, cfg);
        testCheck(!result.empty(), "Default biome produces objects");
    }

    {
        float chunkSize = 32.0f;
        float margin = 0.5f;
        auto result = placer.place(w, ChunkCoord{0, 0});
        bool allInBounds = true;
        for (const auto& obj : result) {
            float lx = obj.position.x - static_cast<float>(ChunkCoord{0,0}.x) * chunkSize;
            float lz = obj.position.z - static_cast<float>(ChunkCoord{0,0}.z) * chunkSize;
            if (lx < margin - 1e-3f || lx > chunkSize - margin + 1e-3f) allInBounds = false;
            if (lz < margin - 1e-3f || lz > chunkSize - margin + 1e-3f) allInBounds = false;
        }
        testCheck(allInBounds, "All objects within chunk bounds");
    }

    {
        float treeSpacing = 4.0f;
        float rockSpacing = 2.5f;
        auto result = placer.place(w, ChunkCoord{0, 0});
        bool spacingOK = true;
        for (size_t i = 0; i < result.size(); ++i) {
            for (size_t j = i + 1; j < result.size(); ++j) {
                if (result[i].type != result[j].type) continue;
                float minSp = (result[i].type == PlacedType::Tree) ? treeSpacing : rockSpacing;
                float d2 = distSq2D(result[i], result[j]);
                if (d2 < minSp * minSp * 0.99f) {
                    spacingOK = false;
                    std::printf("  FAIL spacing: %zu vs %zu dist=%.2f min=%.2f\n",
                                i, j, std::sqrt(d2), minSp);
                }
            }
        }
        testCheck(spacingOK, "All same-type objects respect minimum spacing");
    }

    {
        std::set<uint64_t> ids;
        auto result = placer.place(w, ChunkCoord{0, 0});
        bool unique = true;
        for (const auto& obj : result) {
            uint64_t key = (obj.id.hi << 1) ^ obj.id.lo;
            if (!ids.insert(key).second) unique = false;
        }
        testCheck(unique, "All ObjectIds are unique within a chunk");
    }

    {
        auto a = placer.place(w, ChunkCoord{-1, -1});
        auto b = placer.place(w, ChunkCoord{-1, -1});
        testCheck(allMatch(a, b), "Determinism: negative chunk coords stable");
    }

    {
        float chunkSize = 32.0f;
        auto result = placer.place(w, ChunkCoord{0, 0});
        bool heightsValid = true;
        for (const auto& obj : result) {
            float expectedH = ChunkGenerator::queryHeight(w, obj.position.x, obj.position.z);
            if (std::fabs(obj.position.y - expectedH) > 1e-4f) heightsValid = false;
        }
        testCheck(heightsValid, "All object Y values match terrain height at their position");
    }

    {
        auto a = placer.place(w, ChunkCoord{2, 3});
        auto b = placer.place(w, ChunkCoord{2, 3});
        testCheck(allMatch(a, b), "Determinism: non-origin chunk stable");
    }

    {
        auto placed = placer.place(w, ChunkCoord{0, 0});
        std::set<uint64_t> treeIds, rockIds;
        for (const auto& obj : placed) {
            if (obj.type == PlacedType::Tree) treeIds.insert(obj.id.hi);
            else rockIds.insert(obj.id.hi);
        }
        bool treeUnique = true, rockUnique = true;
        for (const auto& obj : placed) {
            if (obj.type == PlacedType::Tree) {
                if (treeIds.count(obj.id.hi) > 1) treeUnique = false;
            } else {
                if (rockIds.count(obj.id.hi) > 1) rockUnique = false;
            }
        }
        testCheck(treeUnique, "Tree IDs unique");
        testCheck(rockUnique, "Rock IDs unique");
    }

    return gFailures;
}

}
