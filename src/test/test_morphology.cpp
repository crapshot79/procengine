#include "TestUtil.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/ChunkCoord.h"
#include "procedural/world/BiomeGenerator.h"
#include "procedural/world/Biome.h"
#include "procedural/world/WorldSeed.h"
#include "procedural/world/TerrainDelta.h"
#include "procedural/world/TerrainSurface.h"
#include "engine/renderer/Mesh.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>

namespace {

using namespace procengine;

int testMorphologyDeterminism() {
    std::printf("Stage 2.2 — Morphology determinism test\n");

    {
        WorldSeed w = 42;
        ChunkGenerator gen;
        float cs = 32.0f, hs = 6.0f;
        MeshData a = gen.generate(w, {0, 0}, cs);
        MeshData b = gen.generate(w, {0, 0}, cs);
        bool identical = true;
        for (size_t i = 0; i < a.vertices.size(); ++i) {
            if (std::abs(a.vertices[i].pos.y - b.vertices[i].pos.y) > 0.0001f) {
                identical = false; break;
            }
        }
        testCheck(identical, "Same seed + chunk -> identical height");
    }

    return gFailures;
}

int testSeamlessBorders() {
    std::printf("Stage 2.2 — Seamless border test\n");

    {
        WorldSeed w = 42;
        ChunkGenerator gen;
        float cs = 32.0f, hs = 6.0f;
        MeshData a = gen.generate(w, {0, 0}, cs);
        MeshData b = gen.generate(w, {1, 0}, cs);

        int stride = ChunkGenerator::DEFAULT_GRID_SIZE + 1;
        bool match = true;
        for (int zi = 0; zi <= ChunkGenerator::DEFAULT_GRID_SIZE; ++zi) {
            float hA = a.vertices[zi * stride + ChunkGenerator::DEFAULT_GRID_SIZE].pos.y;
            float hB = b.vertices[zi * stride + 0].pos.y;
            if (std::abs(hA - hB) > 0.001f) { match = false; break; }
        }
        testCheck(match, "Right edge of (0,0) matches left edge of (1,0)");
    }

    {
        WorldSeed w = 42;
        ChunkGenerator gen;
        float cs = 32.0f;
        MeshData a = gen.generate(w, {0, 0}, cs);
        MeshData b = gen.generate(w, {0, 1}, cs);

        int stride = ChunkGenerator::DEFAULT_GRID_SIZE + 1;
        bool match = true;
        for (int xi = 0; xi <= ChunkGenerator::DEFAULT_GRID_SIZE; ++xi) {
            float hA = a.vertices[ChunkGenerator::DEFAULT_GRID_SIZE * stride + xi].pos.y;
            float hB = b.vertices[0 * stride + xi].pos.y;
            if (std::abs(hA - hB) > 0.001f) { match = false; break; }
        }
        testCheck(match, "Bottom edge of (0,0) matches top edge of (0,1)");
    }

    return gFailures;
}

int testNegativeCoords() {
    std::printf("Stage 2.2 — Negative coords test\n");

    {
        WorldSeed w = 42;
        ChunkGenerator gen;
        float cs = 32.0f;
        MeshData neg = gen.generate(w, {-1, -1}, cs);
        MeshData pos = gen.generate(w, {0, 0}, cs);

        bool negValid = true;
        for (auto& v : neg.vertices) {
            if (std::isnan(v.pos.y) || std::isinf(v.pos.y)) { negValid = false; break; }
        }
        testCheck(negValid, "Negative chunk produces valid heights");
        testCheck(neg.vertices.size() == pos.vertices.size(), "Negative chunk has correct vertex count");
    }

    return gFailures;
}

int testUnloadReload() {
    std::printf("Stage 2.2 — Unload/reload test\n");

    {
        WorldSeed w = 42;
        ChunkGenerator gen;
        float cs = 32.0f;
        MeshData original = gen.generate(w, {2, 3}, cs);
        MeshData reloaded = gen.generate(w, {2, 3}, cs);

        bool identical = true;
        for (size_t i = 0; i < original.vertices.size(); ++i) {
            if (std::abs(original.vertices[i].pos.y - reloaded.vertices[i].pos.y) > 0.0001f) {
                identical = false; break;
            }
        }
        testCheck(identical, "Unload/reload reproduces identical terrain");
    }

    return gFailures;
}

int testDeltasStillApply() {
    std::printf("Stage 2.2 — Deltas still apply test\n");

    {
        WorldSeed w = 42;
        ChunkGenerator gen;
        float cs = 32.0f, hs = 6.0f;
        int gs = ChunkGenerator::DEFAULT_GRID_SIZE;
        MeshData mesh = gen.generate(w, {0, 0}, cs);

        float origH = mesh.vertices[16 * (gs + 1) + 16].pos.y;

        std::vector<TerrainDelta> deltas = {{16, 16, 20.0f}};
        ChunkGenerator::applyDeltas(mesh, deltas, gs, cs, hs);

        float modH = mesh.vertices[16 * (gs + 1) + 16].pos.y;
        testCheck(std::abs(modH - 20.0f) < 0.01f, "Delta overrides biome terrain height");

        ChunkGenerator::recomputeNormalsAndColors(mesh, gs, cs, hs, w);
        bool normalsOk = true;
        for (auto& v : mesh.vertices) {
            float len = glm::length(v.normal);
            if (std::abs(len - 1.0f) > 0.01f) { normalsOk = false; break; }
        }
        testCheck(normalsOk, "Normals valid after delta + recompute");
    }

    return gFailures;
}

int testBiomeTransitionContinuous() {
    std::printf("Stage 2.2 — Biome transition continuous test\n");

    {
        WorldSeed w = 42;
        float step = 1.0f;
        float maxJump = 0.0f;
        for (float x = 0.0f; x < 200.0f; x += step) {
            float h1 = ChunkGenerator::queryHeight(w, x, 50.0f, 6.0f);
            float h2 = ChunkGenerator::queryHeight(w, x + step, 50.0f, 6.0f);
            maxJump = std::max(maxJump, std::abs(h2 - h1));
        }
        testCheck(maxJump < 2.0f, "No abrupt height jumps across biome transitions");
    }

    return gFailures;
}

int testHighlandMoreRelief() {
    std::printf("Stage 2.2 — Highland more relief test\n");

    {
        WorldSeed w = 42;

        struct ReliefStats {
            int count = 0;
            float minH = std::numeric_limits<float>::max();
            float maxH = -std::numeric_limits<float>::max();
            float sumH = 0.0f;
            float sumSqH = 0.0f;

            void add(float h) {
                ++count;
                minH = std::min(minH, h);
                maxH = std::max(maxH, h);
                sumH += h;
                sumSqH += h * h;
            }

            float relief() const { return maxH - minH; }
            float average() const { return count > 0 ? sumH / static_cast<float>(count) : 0.0f; }
            float variation() const {
                if (count == 0) {
                    return 0.0f;
                }
                float mean = average();
                float variance = sumSqH / static_cast<float>(count) - mean * mean;
                return std::sqrt(std::max(variance, 0.0f));
            }
        };

        ReliefStats grass;
        ReliefStats forest;
        ReliefStats high;

        for (int zi = -96; zi <= 96; ++zi) {
            for (int xi = -96; xi <= 96; ++xi) {
                float x = static_cast<float>(xi) * 16.0f;
                float z = static_cast<float>(zi) * 16.0f;
                BiomeSample b = BiomeGenerator::sampleBiome(w, x, z);
                float h = ChunkGenerator::queryHeight(w, x, z, 6.0f);

                if (b.grasslandWeight > 0.70f) {
                    grass.add(h);
                } else if (b.forestWeight > 0.70f) {
                    forest.add(h);
                } else if (b.highlandWeight > 0.70f) {
                    high.add(h);
                }
            }
        }

        testCheck(grass.count > 32, "Sample includes dominant grassland terrain");
        testCheck(forest.count > 32, "Sample includes dominant forest terrain");
        testCheck(high.count > 32, "Sample includes dominant highland terrain");
        testCheck(grass.relief() > 0.0f, "Grassland has some relief");
        testCheck(forest.variation() > grass.variation() * 1.15f,
                  "Forest has more variation than grassland");
        testCheck(high.variation() > forest.variation() * 1.15f,
                  "Highland has substantially greater variation than forest");
        testCheck(high.average() > grass.average(),
                  "Highland is generally more elevated than grassland");
    }

    return gFailures;
}

int testTerrainSurfaceMatch() {
    std::printf("Stage 2.2 — TerrainSurface match test\n");

    {
        WorldSeed w = 42;
        ChunkGenerator gen;
        float cs = 32.0f, hs = 6.0f;
        int gs = ChunkGenerator::DEFAULT_GRID_SIZE;
        MeshData mesh = gen.generate(w, {0, 0}, cs);

        TerrainSurface surface;
        surface.build(gs, cs / gs, mesh.vertices.data(), static_cast<int>(mesh.vertices.size()));

        float localX = 0.0f;
        float localZ = 0.0f;
        float surfaceH = surface.getHeight(localX, localZ);

        float wx = 0.0f * cs + localX + cs * 0.5f;
        float wz = 0.0f * cs + localZ + cs * 0.5f;
        float queryH = ChunkGenerator::queryHeight(w, wx, wz, hs);

        testCheck(std::abs(surfaceH - queryH) < 0.5f,
            "TerrainSurface query matches generated height");
    }

    return gFailures;
}

}

namespace procengine {

int runMorphologyTests() {
    testMorphologyDeterminism();
    testSeamlessBorders();
    testNegativeCoords();
    testUnloadReload();
    testDeltasStillApply();
    testBiomeTransitionContinuous();
    testHighlandMoreRelief();
    testTerrainSurfaceMatch();

    return gFailures;
}

}
