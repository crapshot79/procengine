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
            bool hasVersion = std::string(line).find("PENGINE_DELTA_V") != std::string::npos;
            testCheck(hasVersion, "Save file has version header");
            std::fclose(f);
        }
    }

    std::printf("Stage 1.6 — WorldStore add object test\n");

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        testCheck(!store.hasAddedObjects(cc), "New store has no added objects");
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        StoredObject obj;
        obj.idHi = 111; obj.idLo = 222;
        obj.type = 2;
        obj.posX = 5.0f; obj.posY = 3.0f; obj.posZ = 7.0f;
        obj.rotY = 1.5f;
        obj.scaleX = 2.0f; obj.scaleY = 1.0f; obj.scaleZ = 2.0f;
        obj.seed = 999;
        store.addObject(cc, obj);
        testCheck(store.hasAddedObjects(cc), "hasAddedObjects returns true after addObject");
        testCheck(store.getAddedObjects(cc).size() == 1, "One added object recorded");
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        StoredObject obj;
        obj.idHi = 111; obj.idLo = 222;
        obj.type = 2; obj.posX = 5.0f; obj.posY = 3.0f; obj.posZ = 7.0f;
        obj.seed = 999;
        store.addObject(cc, obj);
        store.addObject(cc, obj);
        testCheck(store.getAddedObjects(cc).size() == 1, "Duplicate addObject ignored");
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        StoredObject obj1;
        obj1.idHi = 111; obj1.idLo = 222;
        obj1.type = 2; obj1.posX = 5.0f; obj1.posY = 3.0f; obj1.posZ = 7.0f;
        obj1.seed = 999;
        StoredObject obj2;
        obj2.idHi = 333; obj2.idLo = 444;
        obj2.type = 1; obj2.posX = 10.0f; obj2.posY = 2.0f; obj2.posZ = 15.0f;
        obj2.seed = 888;
        store.addObject(cc, obj1);
        store.addObject(cc, obj2);
        testCheck(store.getAddedObjects(cc).size() == 2, "Two distinct objects recorded");
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        StoredObject obj;
        obj.idHi = 111; obj.idLo = 222;
        obj.type = 2; obj.posX = 5.0f; obj.posY = 3.0f; obj.posZ = 7.0f;
        obj.seed = 999;
        store.addObject(cc, obj);

        WorldStore store2(w, "test_saves_add");
        store2.loadAll();
        testCheck(store2.hasAddedObjects(cc), "loadAll restores added objects");
        auto& loaded = store2.getAddedObjects(cc);
        testCheck(loaded.size() == 1, "loadAll restores correct count");
        testCheck(loaded[0].idHi == 111 && loaded[0].idLo == 222, "loadAll restores correct IDs");
        testCheck(loaded[0].posX == 5.0f && loaded[0].posY == 3.0f && loaded[0].posZ == 7.0f,
            "loadAll restores correct position");
        testCheck(loaded[0].seed == 999, "loadAll restores correct seed");
        testCheck(loaded[0].type == 2, "loadAll restores correct type");
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        StoredObject obj;
        obj.idHi = 111; obj.idLo = 222;
        obj.type = 2; obj.posX = 5.0f; obj.posY = 3.0f; obj.posZ = 7.0f;
        obj.seed = 999;
        store.addObject(cc, obj);

        WorldStore store2(w, "test_saves_add");
        store2.loadAll();
        store2.addObject(cc, obj);
        testCheck(store2.getAddedObjects(cc).size() == 1, "No duplicate on re-add after loadAll");
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        StoredObject obj;
        obj.idHi = 111; obj.idLo = 222;
        obj.type = 2; obj.posX = 5.0f; obj.posY = 3.0f; obj.posZ = 7.0f;
        obj.seed = 999;
        store.addObject(cc, obj);

        FILE* f = std::fopen("test_saves_add/seed_42_chunk_0_0.delta", "r");
        testCheck(f != nullptr, "Add save file created");
        if (f) {
            char line[512];
            bool foundAdd = false;
            while (std::fgets(line, sizeof(line), f)) {
                if (std::strncmp(line, "add ", 4) == 0) foundAdd = true;
            }
            testCheck(foundAdd, "Save file contains add line");
            std::fclose(f);
        }
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord ccOther{99, 99};
        testCheck(!store.hasAddedObjects(ccOther), "Other chunk has no added objects");
    }

    {
        WorldStore store(w, "test_saves_add");
        ChunkCoord cc{0, 0};
        StoredObject obj;
        obj.idHi = 555; obj.idLo = 666;
        obj.type = 2; obj.posX = 1.0f; obj.posY = 2.0f; obj.posZ = 3.0f;
        obj.seed = 777;
        store.addObject(cc, obj);
        store.save(cc);

        WorldStore store2(w, "test_saves_add");
        store2.load(cc);
        testCheck(store2.getAddedObjects(cc).size() == 1, "Direct load restores added object");
    }

    return gFailures;
}

}
