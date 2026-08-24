#include "TestUtil.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/TerrainDelta.h"
#include "procedural/world/WorldSeed.h"
#include "procedural/world/WorldStore.h"
#include "procedural/world/TerrainSurface.h"
#include "engine/renderer/Mesh.h"
#include <cstdio>
#include <cmath>

#ifdef _WIN32
#include <direct.h>
#endif

namespace {

using namespace procengine;

int testTerrainDeltaBasic() {
    std::printf("Stage 1.7 — TerrainDelta basic test\n");

    {
        TerrainDelta td;
        testCheck(td.localX == 0, "Default localX is 0");
        testCheck(td.localZ == 0, "Default localZ is 0");
        testCheck(td.newHeight == 0.0f, "Default newHeight is 0");
    }

    {
        TerrainDelta td{5, 10, 3.5f};
        testCheck(td.localX == 5, "Constructor sets localX");
        testCheck(td.localZ == 10, "Constructor sets localZ");
        testCheck(td.newHeight == 3.5f, "Constructor sets newHeight");
    }

    return gFailures;
}

int testTerrainDeltaSerialization() {
    std::printf("Stage 1.7 — TerrainDelta serialization test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_saves");

        store.addTerrainDelta(cc, 16, 16, 5.5f, 32);

        FILE* f = std::fopen("test_terrain_saves/seed_42_chunk_0_0.delta", "r");
        testCheck(f != nullptr, "Save file created");
        if (f) {
            char line[512];
            bool hasV3 = false;
            bool hasTerrain = false;
            while (std::fgets(line, sizeof(line), f)) {
                if (std::strstr(line, "PENGINE_DELTA_V3")) hasV3 = true;
                if (std::strncmp(line, "terrain ", 8) == 0) hasTerrain = true;
            }
            testCheck(hasV3, "Save file has V3 header");
            testCheck(hasTerrain, "Save file contains terrain line");
            std::fclose(f);
        }
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_saves");
        store.load(cc);

        testCheck(store.hasTerrainDeltas(cc), "load restores terrain deltas");
        auto& deltas = store.getTerrainDeltas(cc);
        testCheck(deltas.size() == 1, "load restores correct count");
        testCheck(deltas[0].localX == 16, "load restores localX");
        testCheck(deltas[0].localZ == 16, "load restores localZ");
        testCheck(std::abs(deltas[0].newHeight - 5.5f) < 0.01f, "load restores newHeight");
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_saves");
        store.addTerrainDelta(cc, 10, 20, 3.0f, 32);
        store.addTerrainDelta(cc, 16, 16, 7.0f, 32);

        WorldStore store2(w, "test_terrain_saves");
        store2.loadAll();
        auto& deltas = store2.getTerrainDeltas(cc);
        testCheck(deltas.size() == 2, "Multiple terrain deltas serialized");
    }

    return gFailures;
}

int testTerrainDeltaDedup() {
    std::printf("Stage 1.7 — TerrainDelta dedup test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_dedup");

        store.addTerrainDelta(cc, 16, 16, 5.0f, 32);
        store.addTerrainDelta(cc, 16, 16, 8.0f, 32);

        auto& deltas = store.getTerrainDeltas(cc);
        testCheck(deltas.size() == 1, "Duplicate delta at same position not added");
        testCheck(std::abs(deltas[0].newHeight - 8.0f) < 0.01f, "Duplicate updated to new height");
    }

    return gFailures;
}

int testApplyDeltas() {
    std::printf("Stage 1.7 — Apply deltas test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData base = gen.generate(w, cc, chunkSize, gridSize, heightScale);
        float origH = base.vertices[16 * (gridSize + 1) + 16].pos.y;

        std::vector<TerrainDelta> deltas = {{16, 16, 10.0f}};
        ChunkGenerator::applyDeltas(base, deltas, gridSize, chunkSize, heightScale);

        float modH = base.vertices[16 * (gridSize + 1) + 16].pos.y;
        testCheck(std::abs(modH - 10.0f) < 0.01f, "Delta applied to correct vertex");
        testCheck(std::abs(origH - modH) > 0.1f, "Height actually changed");
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData mesh = gen.generate(w, cc, chunkSize, gridSize, heightScale);

        std::vector<TerrainDelta> deltas = {{5, 10, 99.0f}};
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);

        float h = mesh.vertices[10 * (gridSize + 1) + 5].pos.y;
        testCheck(std::abs(h - 99.0f) < 0.01f, "Delta at different position applied");
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData mesh = gen.generate(w, cc, chunkSize, gridSize, heightScale);
        float origH5_5 = mesh.vertices[5 * (gridSize + 1) + 5].pos.y;

        std::vector<TerrainDelta> deltas = {{16, 16, 10.0f}};
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);

        float h5_5 = mesh.vertices[5 * (gridSize + 1) + 5].pos.y;
        testCheck(std::abs(origH5_5 - h5_5) < 0.001f, "Unmodified vertex unchanged");
    }

    return gFailures;
}

int testNoDuplicateApply() {
    std::printf("Stage 1.7 — No duplicate apply test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData mesh = gen.generate(w, cc, chunkSize, gridSize, heightScale);
        float origH = mesh.vertices[16 * (gridSize + 1) + 16].pos.y;

        std::vector<TerrainDelta> deltas = {{16, 16, 10.0f}};
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);

        float h = mesh.vertices[16 * (gridSize + 1) + 16].pos.y;
        testCheck(std::abs(h - 10.0f) < 0.01f, "Applying same deltas twice yields same result");
    }

    return gFailures;
}

int testRecomputeNormalsAndColors() {
    std::printf("Stage 1.7 — Recompute normals and colors test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData mesh = gen.generate(w, cc, chunkSize, gridSize, heightScale);
        float origColorG = mesh.vertices[16 * (gridSize + 1) + 16].color.g;

        std::vector<TerrainDelta> deltas = {{16, 16, 20.0f}};
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);
        ChunkGenerator::recomputeNormalsAndColors(mesh, gridSize, chunkSize, heightScale, w);

        float newColorG = mesh.vertices[16 * (gridSize + 1) + 16].color.g;
        testCheck(newColorG != origColorG, "Color recomputed after delta");

        bool allNormalsValid = true;
        for (int i = 0; i < static_cast<int>(mesh.vertices.size()); ++i) {
            float len = glm::length(mesh.vertices[i].normal);
            if (std::abs(len - 1.0f) > 0.01f) { allNormalsValid = false; break; }
        }
        testCheck(allNormalsValid, "All normals unit-length after recompute");
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData mesh = gen.generate(w, cc, chunkSize, gridSize, heightScale);
        glm::vec3 origNormal = mesh.vertices[5 * (gridSize + 1) + 5].normal;

        std::vector<TerrainDelta> deltas = {{16, 16, 20.0f}};
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);
        ChunkGenerator::recomputeNormalsAndColors(mesh, gridSize, chunkSize, heightScale, w);

        glm::vec3 farNormal = mesh.vertices[5 * (gridSize + 1) + 5].normal;
        testCheck(glm::length(farNormal - origNormal) < 0.05f,
            "Normal far from delta approximately unchanged");
    }

    return gFailures;
}

int testTerrainSurfaceQuery() {
    std::printf("Stage 1.7 — TerrainSurface query test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData mesh = gen.generate(w, cc, chunkSize, gridSize, heightScale);
        float origH = ChunkGenerator::queryHeight(w, 16.0f, 16.0f, heightScale);

        std::vector<TerrainDelta> deltas = {{16, 16, 15.0f}};
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);
        ChunkGenerator::recomputeNormalsAndColors(mesh, gridSize, chunkSize, heightScale, w);

        TerrainSurface surface;
        surface.build(gridSize, chunkSize / gridSize, mesh.vertices.data(),
                      static_cast<int>(mesh.vertices.size()));
        float queriedH = surface.getHeight(16.0f - chunkSize * 0.5f,
                                           16.0f - chunkSize * 0.5f);

        testCheck(std::abs(queriedH - 15.0f) < 0.5f,
            "TerrainSurface returns modified height");
        testCheck(std::abs(origH - queriedH) > 0.5f,
            "Surface height differs from base procedural height");
    }

    return gFailures;
}

int testBorderPropagation() {
    std::printf("Stage 1.7 — Border propagation test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_border");

        store.addTerrainDelta(cc, 32, 16, 9.0f, 32);

        testCheck(store.hasTerrainDeltas(cc), "Delta recorded in source chunk");

        ChunkCoord neighbor{1, 0};
        testCheck(store.hasTerrainDeltas(neighbor),
            "Border delta propagated to right neighbor");
        auto& nbDeltas = store.getTerrainDeltas(neighbor);
        bool found = false;
        for (auto& d : nbDeltas) {
            if (d.localX == 0 && d.localZ == 16) {
                testCheck(std::abs(d.newHeight - 9.0f) < 0.01f,
                    "Propagated delta has correct height");
                found = true;
            }
        }
        testCheck(found, "Neighbor has delta at localX=0");
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_border2");

        store.addTerrainDelta(cc, 0, 16, 7.0f, 32);

        ChunkCoord neighbor{-1, 0};
        testCheck(store.hasTerrainDeltas(neighbor),
            "Border delta propagated to left neighbor");
        auto& nbDeltas = store.getTerrainDeltas(neighbor);
        bool found = false;
        for (auto& d : nbDeltas) {
            if (d.localX == 32 && d.localZ == 16) {
                found = true;
            }
        }
        testCheck(found, "Left neighbor has delta at localX=32");
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_border3");

        store.addTerrainDelta(cc, 16, 0, 6.0f, 32);

        ChunkCoord neighbor{0, -1};
        testCheck(store.hasTerrainDeltas(neighbor),
            "Border delta propagated to top neighbor");
    }

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        WorldStore store(w, "test_terrain_border4");

        store.addTerrainDelta(cc, 16, 32, 8.0f, 32);

        ChunkCoord neighbor{0, 1};
        testCheck(store.hasTerrainDeltas(neighbor),
            "Border delta propagated to bottom neighbor");
    }

    return gFailures;
}

int testBorderSeamless() {
    std::printf("Stage 1.7 — Border seamless test\n");

    {
        WorldSeed w = 42;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;
        ChunkGenerator gen;
        float spacing = chunkSize / gridSize;

        MeshData meshA = gen.generate(w, {0, 0}, chunkSize, gridSize, heightScale);
        MeshData meshB = gen.generate(w, {1, 0}, chunkSize, gridSize, heightScale);

        float hA_right = meshA.vertices[16 * (gridSize + 1) + gridSize].pos.y;
        float hB_left = meshB.vertices[16 * (gridSize + 1) + 0].pos.y;
        testCheck(std::abs(hA_right - hB_left) < 0.001f,
            "Base terrain: border vertices match");

        std::vector<TerrainDelta> deltasA = {{gridSize, 16, 20.0f}};
        ChunkGenerator::applyDeltas(meshA, deltasA, gridSize, chunkSize, heightScale);
        ChunkGenerator::recomputeNormalsAndColors(meshA, gridSize, chunkSize, heightScale, w);

        std::vector<TerrainDelta> deltasB = {{0, 16, 20.0f}};
        ChunkGenerator::applyDeltas(meshB, deltasB, gridSize, chunkSize, heightScale);
        ChunkGenerator::recomputeNormalsAndColors(meshB, gridSize, chunkSize, heightScale, w);

        float hA_mod = meshA.vertices[16 * (gridSize + 1) + gridSize].pos.y;
        float hB_mod = meshB.vertices[16 * (gridSize + 1) + 0].pos.y;
        testCheck(std::abs(hA_mod - hB_mod) < 0.001f,
            "Modified border vertices match after propagation");
        testCheck(std::abs(hA_mod - 20.0f) < 0.01f,
            "Modified border has expected height");
    }

    return gFailures;
}

int testBackwardCompatibility() {
    std::printf("Stage 1.7 — Backward compatibility test\n");

    {
#ifdef _WIN32
        _mkdir("test_terrain_compat");
#else
        mkdir("test_terrain_compat", 0755);
#endif
        FILE* f = std::fopen("test_terrain_compat/seed_99_chunk_1_1.delta", "w");
        if (f) {
            std::fprintf(f, "PENGINE_DELTA_V1\n");
            std::fprintf(f, "seed 99\n");
            std::fprintf(f, "chunk 1 1\n");
            std::fprintf(f, "remove 12345\n");
            std::fclose(f);
        }

        WorldStore store(99, "test_terrain_compat");
        store.load({1, 1});
        testCheck(store.hasRemovals({1, 1}), "V1 removals loaded");
        testCheck(!store.hasTerrainDeltas({1, 1}), "V1 file has no terrain deltas");
    }

    {
        FILE* f = std::fopen("test_terrain_compat/seed_99_chunk_2_2.delta", "w");
        if (f) {
            std::fprintf(f, "PENGINE_DELTA_V2\n");
            std::fprintf(f, "seed 99\n");
            std::fprintf(f, "chunk 2 2\n");
            std::fprintf(f, "remove 67890\n");
            std::fclose(f);
        }

        WorldStore store(99, "test_terrain_compat");
        store.load({2, 2});
        testCheck(store.hasRemovals({2, 2}), "V2 removals loaded");
        testCheck(!store.hasTerrainDeltas({2, 2}), "V2 file has no terrain deltas");
    }

    return gFailures;
}

int testNegativeChunkDelta() {
    std::printf("Stage 1.7 — Negative chunk delta test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{-1, -1};
        WorldStore store(w, "test_terrain_neg");

        store.addTerrainDelta(cc, 10, 10, 5.0f, 32);
        testCheck(store.hasTerrainDeltas(cc), "Delta stored for negative chunk");

        WorldStore store2(w, "test_terrain_neg");
        store2.load(cc);
        testCheck(store2.hasTerrainDeltas(cc), "Delta loaded for negative chunk");
        auto& deltas = store2.getTerrainDeltas(cc);
        testCheck(deltas.size() == 1, "Correct delta count for negative chunk");
    }

    return gFailures;
}

int testMultipleDeltaApply() {
    std::printf("Stage 1.7 — Multiple delta apply test\n");

    {
        WorldSeed w = 42;
        ChunkCoord cc{0, 0};
        ChunkGenerator gen;
        float chunkSize = 32.0f;
        int gridSize = 32;
        float heightScale = 6.0f;

        MeshData mesh = gen.generate(w, cc, chunkSize, gridSize, heightScale);

        std::vector<TerrainDelta> deltas = {
            {5, 5, 10.0f},
            {10, 10, 15.0f},
            {20, 20, 8.0f}
        };
        ChunkGenerator::applyDeltas(mesh, deltas, gridSize, chunkSize, heightScale);

        float h5_5 = mesh.vertices[5 * (gridSize + 1) + 5].pos.y;
        float h10_10 = mesh.vertices[10 * (gridSize + 1) + 10].pos.y;
        float h20_20 = mesh.vertices[20 * (gridSize + 1) + 20].pos.y;

        testCheck(std::abs(h5_5 - 10.0f) < 0.01f, "First delta applied");
        testCheck(std::abs(h10_10 - 15.0f) < 0.01f, "Second delta applied");
        testCheck(std::abs(h20_20 - 8.0f) < 0.01f, "Third delta applied");
    }

    return gFailures;
}

}

namespace procengine {

int runTerrainDeltaTests() {
    testTerrainDeltaBasic();
    testTerrainDeltaSerialization();
    testTerrainDeltaDedup();
    testApplyDeltas();
    testNoDuplicateApply();
    testRecomputeNormalsAndColors();
    testTerrainSurfaceQuery();
    testBorderPropagation();
    testBorderSeamless();
    testBackwardCompatibility();
    testNegativeChunkDelta();
    testMultipleDeltaApply();

    return gFailures;
}

}
