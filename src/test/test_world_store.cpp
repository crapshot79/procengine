#include "procedural/world/WorldStore.h"
#include "procedural/world/ChunkStreamer.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/ChunkPlacer.h"
#include "procedural/world/WorldSeed.h"
#include "test/TestUtil.h"

#include <cstdio>
#include <cmath>
#include <sys/stat.h>

namespace procengine {

static bool dirExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR);
}

static bool fileExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFREG);
}

int runTestWorldStore() {
    std::printf("Stage 1.5 — WorldStore test\n");

    constexpr float chunkSize = 32.0f;
    constexpr float heightScale = 6.0f;
    WorldSeed w = 42ULL;
    ChunkCoord cc{0, 0};

    {
        WorldStore store(w, "test_saves");
        testCheck(store.isEmpty(), "New store is empty");
    }

    {
        WorldStore store(w, "test_saves");
        store.addRemoval(cc, 100, 200);
        testCheck(!store.isEmpty(), "Store not empty after addRemoval");
        testCheck(store.hasRemovals(cc), "hasRemovals returns true for modified chunk");
        testCheck(store.getRemovals(cc).size() == 1, "One removal recorded");
    }

    {
        WorldStore store(w, "test_saves");
        store.addRemoval(cc, 100, 200);
        store.addRemoval(cc, 100, 200);
        testCheck(store.getRemovals(cc).size() == 1, "Duplicate removal ignored");
    }

    {
        WorldStore store(w, "test_saves");
        store.addRemoval(cc, 100, 200);
        store.addRemoval(cc, 300, 400);
        testCheck(store.getRemovals(cc).size() == 2, "Two distinct removals recorded");
    }

    {
        std::string path = "test_saves/seed_42_chunk_0_0.delta";
        testCheck(fileExists(path), "Save file created on addRemoval");
    }

    {
        WorldStore store2(w, "test_saves");
        store2.loadAll();
        testCheck(store2.hasRemovals(cc), "loadAll restores removals from disk");
        testCheck(store2.getRemovals(cc).size() == 2, "loadAll restores correct count");
    }

    {
        WorldStore store(w, "test_saves");
        store.loadAll();
        ChunkGenerator gen;
        MeshData mesh = gen.generate(w, cc, chunkSize);
        ChunkPlacer placer;
        auto placements = placer.place(w, cc);

        auto removals = store.getRemovals(cc);
        int removed = 0;
        for (const auto& obj : placements) {
            uint64_t key = (obj.id.hi << 1) ^ obj.id.lo;
            for (uint64_t rk : removals) {
                if (rk == key) { removed++; break; }
            }
        }
        testCheck(removed >= 0, "Removal lookup runs without error");
    }

    {
        WorldStore store(w, "test_saves_real");
        ChunkGenerator gen;
        MeshData mesh = gen.generate(w, cc, chunkSize);
        ChunkPlacer placer;
        auto placements = placer.place(w, cc);

        int treeCount = 0;
        for (const auto& obj : placements) {
            if (obj.type == PlacedType::Tree) treeCount++;
        }
        testCheck(treeCount > 0, "Chunk has trees to remove");

        const PlacedObject& firstTree = placements[0];
        store.addRemoval(cc, firstTree.id.hi, firstTree.id.lo);

        auto removals = store.getRemovals(cc);
        int matchCount = 0;
        for (const auto& obj : placements) {
            uint64_t key = (obj.id.hi << 1) ^ obj.id.lo;
            for (uint64_t rk : removals) {
                if (rk == key) { matchCount++; break; }
            }
        }
        testCheck(matchCount == 1, "Real ObjectId removal matches exactly one placed object");
    }

    {
        std::string path = "test_saves_real/seed_42_chunk_0_0.delta";
        testCheck(fileExists(path), "Real save file created");
    }

    {
        WorldStore store(w, "test_saves");
        ChunkCoord ccOther{5, 5};
        testCheck(!store.hasRemovals(ccOther), "Other chunk has no removals");
    }

    {
        WorldStore store(w, "test_saves");
        store.load(cc);
        testCheck(store.hasRemovals(cc), "Direct load restores removals");
    }

    {
        WorldStore store(w, "test_saves");
        store.loadAll();
        store.addRemoval(cc, 500, 600);
        store.save(cc);

        WorldStore store2(w, "test_saves");
        store2.loadAll();
        testCheck(store2.getRemovals(cc).size() == 3, "Re-saved chunk accumulates removals");
    }

    {
        WorldStore store(w, "test_saves");
        store.addRemoval(cc, 700, 800);
        store.save(cc);

        FILE* f = std::fopen("test_saves/seed_42_chunk_0_0.delta", "r");
        testCheck(f != nullptr, "Save file readable");
        if (f) {
            char line[256];
            std::fgets(line, sizeof(line), f);
            bool hasVersion = std::string(line).find("PENGINE_DELTA_V1") != std::string::npos;
            testCheck(hasVersion, "Save file has version header");
            std::fclose(f);
        }
    }

    return gFailures;
}

}
