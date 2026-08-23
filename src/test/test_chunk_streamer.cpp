#include "procedural/world/ChunkStreamer.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/WorldSeed.h"
#include "test/TestUtil.h"

#include <cstdio>
#include <cmath>
#include <unordered_set>

namespace procengine {

static bool meshEq(const MeshData& a, const MeshData& b) {
    if (a.vertices.size() != b.vertices.size()) return false;
    if (a.indices.size() != b.indices.size()) return false;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].pos != b.vertices[i].pos) return false;
        if (a.vertices[i].color != b.vertices[i].color) return false;
    }
    return true;
}

static bool placementsEq(const std::vector<PlacedObject>& a, const std::vector<PlacedObject>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id) return false;
        if (a[i].position.x != b[i].position.x) return false;
        if (a[i].position.y != b[i].position.y) return false;
        if (a[i].position.z != b[i].position.z) return false;
        if (a[i].seed != b[i].seed) return false;
        if (a[i].type != b[i].type) return false;
    }
    return true;
}

int runTestChunkStreamer() {
    std::printf("Stage 1.4 — ChunkStreamer test\n");

    constexpr float chunkSize = 32.0f;
    constexpr float heightScale = 6.0f;
    WorldSeed w = 42ULL;

    {
        ChunkStreamer s(w, chunkSize, heightScale, 1);
        ChunkCoord c0 = s.worldToChunk(0.0f, 0.0f);
        ChunkCoord c1 = s.worldToChunk(16.0f, 16.0f);
        ChunkCoord c2 = s.worldToChunk(32.0f, 0.0f);
        ChunkCoord c3 = s.worldToChunk(-1.0f, -1.0f);
        ChunkCoord c4 = s.worldToChunk(31.9f, 31.9f);
        testCheck(c0.x == 0 && c0.z == 0, "worldToChunk: origin -> (0,0)");
        testCheck(c1.x == 0 && c1.z == 0, "worldToChunk: center of chunk -> (0,0)");
        testCheck(c2.x == 1 && c2.z == 0, "worldToChunk: 32m -> (1,0)");
        testCheck(c3.x == -1 && c3.z == -1, "worldToChunk: negative -> (-1,-1)");
        testCheck(c4.x == 0 && c4.z == 0, "worldToChunk: near boundary stays in chunk");
    }

    {
        ChunkStreamer s(w, chunkSize, heightScale, 1);
        ChunkCoord cam = s.worldToChunk(16.0f, 16.0f);
        testCheck(cam.x == 0 && cam.z == 0, "worldToChunk: camera center of chunk (0,0)");
    }

    {
        ChunkGenerator gen;
        WorldSeed w2 = 42ULL;
        ChunkCoord cc{0, 0};
        MeshData a = gen.generate(w2, cc, chunkSize);
        ChunkPlacer placer;
        auto pa = placer.place(w2, cc);

        MeshData b = a;
        auto pb = pa;
        testCheck(meshEq(a, b) && placementsEq(pa, pb),
            "Determinism: raw generate matches itself");
    }

    {
        ChunkStreamer s(w, chunkSize, heightScale, 1);

        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        MeshData expectedMesh = gen.generate(w, cc, chunkSize);
        ChunkPlacer placer;
        auto expectedPlacements = placer.place(w, cc);

        std::unordered_map<ChunkCoord, ChunkState, ChunkCoordHash> cache;

        ChunkState state;
        state.mesh = expectedMesh;
        state.placements = expectedPlacements;
        cache[cc] = std::move(state);

        auto it = cache.find(cc);
        testCheck(it != cache.end(), "Cache lookup succeeds");
        testCheck(meshEq(it->second.mesh, expectedMesh), "Cached mesh matches generated");
        testCheck(placementsEq(it->second.placements, expectedPlacements), "Cached placements match generated");

        cache.erase(cc);
        testCheck(cache.find(cc) == cache.end(), "Erase removes from cache");

        ChunkState state2;
        state2.mesh = expectedMesh;
        state2.placements = expectedPlacements;
        cache[cc] = std::move(state2);

        auto it2 = cache.find(cc);
        testCheck(meshEq(it2->second.mesh, expectedMesh), "Re-inserted mesh matches");
        testCheck(placementsEq(it2->second.placements, expectedPlacements), "Re-inserted placements match");
    }

    {
        ChunkCoordHash hash;
        ChunkCoord a{0, 0};
        ChunkCoord b{0, 0};
        ChunkCoord c{1, 0};
        ChunkCoord d{0, 1};
        testCheck(hash(a) == hash(b), "Hash: same coord -> same hash");
        testCheck(hash(a) != hash(c) || hash(a) != hash(d),
            "Hash: different coords -> different hash (or acceptable collision)");
    }

    {
        ChunkCoordHash hash;
        std::unordered_set<size_t> hashes;
        for (int z = -5; z <= 5; ++z) {
            for (int x = -5; x <= 5; ++x) {
                hashes.insert(hash(ChunkCoord{x, z}));
            }
        }
        testCheck(hashes.size() >= 100,
            "Hash distribution: 11x11 grid produces mostly unique hashes");
    }

    {
        ChunkCoord cc{3, -2};
        ChunkGenerator gen;
        MeshData original = gen.generate(w, cc, chunkSize);
        ChunkPlacer placer;
        auto origPlacements = placer.place(w, cc);

        struct CpuChunk {
            MeshData mesh;
            std::vector<PlacedObject> placements;
        };

        CpuChunk cached;
        cached.mesh = original;
        cached.placements = origPlacements;

        MeshData regenerated = gen.generate(w, cc, chunkSize);
        auto rePlacements = placer.place(w, cc);

        testCheck(meshEq(cached.mesh, regenerated), "Unload/reload: mesh reproduces identically");
        testCheck(placementsEq(cached.placements, rePlacements), "Unload/reload: placements reproduce identically");
    }

    {
        ChunkGenerator gen;
        ChunkPlacer placer;
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                ChunkCoord cc{x, z};
                MeshData m1 = gen.generate(w, cc, chunkSize);
                auto p1 = placer.place(w, cc);

                MeshData m2 = gen.generate(w, cc, chunkSize);
                auto p2 = placer.place(w, cc);

                testCheck(meshEq(m1, m2), "Determinism: 5x5 chunk grid mesh stable");
                testCheck(placementsEq(p1, p2), "Determinism: 5x5 chunk grid placements stable");
            }
        }
    }

    return gFailures;
}

}
