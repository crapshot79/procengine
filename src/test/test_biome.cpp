#include "TestUtil.h"
#include "procedural/world/Biome.h"
#include "procedural/world/BiomeGenerator.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/ChunkPlacer.h"
#include "procedural/world/WorldSeed.h"
#include <cmath>
#include <algorithm>

namespace {

using namespace procengine;

int testBiomeDominant() {
    std::printf("Stage 2.1 — BiomeSample dominant test\n");

    {
        BiomeSample bs{1.0f, 0.0f, 0.0f};
        testCheck(bs.dominant() == BiomeType::Grassland, "Pure grassland -> Grassland");
    }
    {
        BiomeSample bs{0.0f, 1.0f, 0.0f};
        testCheck(bs.dominant() == BiomeType::Forest, "Pure forest -> Forest");
    }
    {
        BiomeSample bs{0.0f, 0.0f, 1.0f};
        testCheck(bs.dominant() == BiomeType::Highland, "Pure highland -> Highland");
    }
    {
        BiomeSample bs{0.4f, 0.3f, 0.3f};
        testCheck(bs.dominant() == BiomeType::Grassland, "Grassland wins tie");
    }

    return gFailures;
}

int testBiomeDeterminism() {
    std::printf("Stage 2.1 — Biome determinism test\n");

    {
        WorldSeed w = 42;
        BiomeSample a = BiomeGenerator::sampleBiome(w, 10.0f, 20.0f);
        BiomeSample b = BiomeGenerator::sampleBiome(w, 10.0f, 20.0f);
        testCheck(std::abs(a.grasslandWeight - b.grasslandWeight) < 0.0001f,
            "Same seed + position -> identical grassland");
        testCheck(std::abs(a.forestWeight - b.forestWeight) < 0.0001f,
            "Same seed + position -> identical forest");
        testCheck(std::abs(a.highlandWeight - b.highlandWeight) < 0.0001f,
            "Same seed + position -> identical highland");
    }

    return gFailures;
}

int testBiomeVariation() {
    std::printf("Stage 2.1 — Biome variation test\n");

    {
        WorldSeed w = 42;
        bool anyDiff = false;
        for (int i = 0; i < 10; ++i) {
            float x1 = static_cast<float>(i) * 50.0f;
            float z1 = static_cast<float>(i) * 37.0f;
            float x2 = x1 + 1200.0f;
            float z2 = z1 + 1200.0f;
            BiomeSample a = BiomeGenerator::sampleBiome(w, x1, z1);
            BiomeSample b = BiomeGenerator::sampleBiome(w, x2, z2);
            float diff = std::abs(a.grasslandWeight - b.grasslandWeight) +
                         std::abs(a.forestWeight - b.forestWeight);
            if (diff > 0.05f) anyDiff = true;
        }
        testCheck(anyDiff, "Some distant positions produce different biome weights");
    }

    return gFailures;
}

int testBiomeSeedVariation() {
    std::printf("Stage 2.1 — Biome seed variation test\n");

    {
        bool anyDiff = false;
        for (int i = 0; i < 10; ++i) {
            float x = static_cast<float>(i) * 240.0f + 10.0f;
            float z = static_cast<float>(i) * 180.0f + 10.0f;
            BiomeSample a = BiomeGenerator::sampleBiome(42, x, z);
            BiomeSample b = BiomeGenerator::sampleBiome(99, x, z);
            float diff = std::abs(a.grasslandWeight - b.grasslandWeight) +
                         std::abs(a.forestWeight - b.forestWeight);
            if (diff > 0.05f) anyDiff = true;
        }
        testCheck(anyDiff, "Different seeds produce different biome layouts at some positions");
    }

    return gFailures;
}

int testBiomeNormalization() {
    std::printf("Stage 2.1 — Biome normalization test\n");

    {
        WorldSeed w = 42;
        for (int i = 0; i < 20; ++i) {
            float x = static_cast<float>(i) * 17.3f;
            float z = static_cast<float>(i) * 23.7f;
            BiomeSample bs = BiomeGenerator::sampleBiome(w, x, z);
            float total = bs.grasslandWeight + bs.forestWeight + bs.highlandWeight;
            testCheck(std::abs(total - 1.0f) < 0.01f, "Weights sum to ~1.0");
            testCheck(bs.grasslandWeight >= -0.001f, "Grassland weight non-negative");
            testCheck(bs.forestWeight >= -0.001f, "Forest weight non-negative");
            testCheck(bs.highlandWeight >= -0.001f, "Highland weight non-negative");
        }
    }

    return gFailures;
}

int testBiomeNoFallbackSnap() {
    std::printf("Stage 2.1 — Biome no-fallback snap test\n");

    {
        WorldSeed w = 42;
        bool allNormalized = true;
        bool allGrasslandPositive = true;
        bool allForestPositive = true;
        bool allHighlandPositive = true;
        for (int zi = -8; zi <= 8; ++zi) {
            for (int xi = -8; xi <= 8; ++xi) {
                float x = static_cast<float>(xi) * 19.0f + 3.25f;
                float z = static_cast<float>(zi) * 23.0f + 6.75f;
                BiomeSample bs = BiomeGenerator::sampleBiome(w, x, z);
                float total = bs.grasslandWeight + bs.forestWeight + bs.highlandWeight;
                allNormalized = allNormalized && std::abs(total - 1.0f) < 0.01f;
                allGrasslandPositive = allGrasslandPositive && bs.grasslandWeight > 0.0f;
                allForestPositive = allForestPositive && bs.forestWeight > 0.0f;
                allHighlandPositive = allHighlandPositive && bs.highlandWeight > 0.0f;
            }
        }
        testCheck(allNormalized, "Soft biome weights remain normalized");
        testCheck(allGrasslandPositive, "Grassland soft weight remains meaningful");
        testCheck(allForestPositive, "Forest soft weight remains meaningful");
        testCheck(allHighlandPositive, "Highland soft weight remains meaningful");
    }

    return gFailures;
}

int testBiomeLocalContinuity() {
    std::printf("Stage 2.1 — Biome local continuity test\n");

    {
        WorldSeed w = 42;
        for (int i = 0; i < 32; ++i) {
            float x = static_cast<float>(i) * 11.3f - 80.0f;
            float z = static_cast<float>(i) * -7.7f + 45.0f;
            BiomeSample a = BiomeGenerator::sampleBiome(w, x, z);
            BiomeSample b = BiomeGenerator::sampleBiome(w, x + 0.25f, z + 0.25f);

            float maxDiff = 0.0f;
            maxDiff = std::max(maxDiff, std::abs(a.grasslandWeight - b.grasslandWeight));
            maxDiff = std::max(maxDiff, std::abs(a.forestWeight - b.forestWeight));
            maxDiff = std::max(maxDiff, std::abs(a.highlandWeight - b.highlandWeight));
            testCheck(maxDiff < 0.05f, "Tiny movement does not snap biome weights");
        }
    }

    return gFailures;
}

int testBiomeBorderContinuity() {
    std::printf("Stage 2.1 — Biome border continuity test\n");

    {
        WorldSeed w = 42;
        float chunkSize = 32.0f;
        float x = 31.5f;
        float z = 10.0f;

        BiomeSample left = BiomeGenerator::sampleBiome(w, x, z);
        BiomeSample right = BiomeGenerator::sampleBiome(w, x + 1.0f, z);

        float maxDiff = 0.0f;
        maxDiff = std::max(maxDiff, std::abs(left.grasslandWeight - right.grasslandWeight));
        maxDiff = std::max(maxDiff, std::abs(left.forestWeight - right.forestWeight));
        maxDiff = std::max(maxDiff, std::abs(left.highlandWeight - right.highlandWeight));
        testCheck(maxDiff < 0.05f, "Biome weights continuous across x=32 border");
    }

    {
        WorldSeed w = 42;
        float z = 63.5f;
        float x = 10.0f;

        BiomeSample top = BiomeGenerator::sampleBiome(w, x, z);
        BiomeSample bottom = BiomeGenerator::sampleBiome(w, x, z + 1.0f);

        float maxDiff = 0.0f;
        maxDiff = std::max(maxDiff, std::abs(top.grasslandWeight - bottom.grasslandWeight));
        maxDiff = std::max(maxDiff, std::abs(top.forestWeight - bottom.forestWeight));
        maxDiff = std::max(maxDiff, std::abs(top.highlandWeight - bottom.highlandWeight));
        testCheck(maxDiff < 0.05f, "Biome weights continuous across z=64 border");
    }

    return gFailures;
}

int testBiomeNegativeCoords() {
    std::printf("Stage 2.1 — Biome negative coords test\n");

    {
        WorldSeed w = 42;
        BiomeSample bs = BiomeGenerator::sampleBiome(w, -50.0f, -80.0f);
        float total = bs.grasslandWeight + bs.forestWeight + bs.highlandWeight;
        testCheck(std::abs(total - 1.0f) < 0.01f, "Negative coords: weights sum to 1.0");
        testCheck(bs.grasslandWeight >= -0.001f, "Negative coords: grassland non-negative");
    }

    return gFailures;
}

int testTreeDensity() {
    std::printf("Stage 2.1 — Tree density test\n");

    {
        WorldSeed w = 42;
        bool anyDiff = false;
        for (int i = 0; i < 10; ++i) {
            float x1 = static_cast<float>(i) * 50.0f;
            float z1 = static_cast<float>(i) * 37.0f;
            float d1 = BiomeGenerator::treeDensity(w, x1, z1);
            float d2 = BiomeGenerator::treeDensity(w, x1 + 1200.0f, z1 + 1200.0f);
            if (std::abs(d1 - d2) > 1.0f) anyDiff = true;
        }
        testCheck(anyDiff, "Tree density varies across world");
    }

    {
        WorldSeed w = 42;
        float d1 = BiomeGenerator::treeDensity(w, 50.0f, 50.0f);
        float d2 = BiomeGenerator::treeDensity(w, 50.0f, 50.0f);
        testCheck(std::abs(d1 - d2) < 0.001f, "Tree density deterministic");
    }

    return gFailures;
}

int testRockDensity() {
    std::printf("Stage 2.1 — Rock density test\n");

    {
        WorldSeed w = 42;
        bool anyDiff = false;
        for (int i = 0; i < 10; ++i) {
            float x1 = static_cast<float>(i) * 50.0f;
            float z1 = static_cast<float>(i) * 37.0f;
            float d1 = BiomeGenerator::rockDensity(w, x1, z1);
            float d2 = BiomeGenerator::rockDensity(w, x1 + 1200.0f, z1 + 1200.0f);
            if (std::abs(d1 - d2) > 1.0f) anyDiff = true;
        }
        testCheck(anyDiff, "Rock density varies across world");
    }

    {
        WorldSeed w = 42;
        float d1 = BiomeGenerator::rockDensity(w, 50.0f, 50.0f);
        float d2 = BiomeGenerator::rockDensity(w, 50.0f, 50.0f);
        testCheck(std::abs(d1 - d2) < 0.001f, "Rock density deterministic");
    }

    return gFailures;
}

int testBiomeChunkStreamerIntegration() {
    std::printf("Stage 2.1 — Biome chunk streamer integration test\n");

    {
        WorldSeed w = 42;
        float chunkSize = 32.0f;
        float heightScale = 6.0f;

        bool terrainColorDiffers = false;
        ChunkGenerator gen;
        MeshData meshA = gen.generate(w, {0, 0}, chunkSize);
        MeshData meshB = gen.generate(w, {12, 0}, chunkSize);

        float avgR_A = 0, avgR_B = 0;
        for (auto& v : meshA.vertices) avgR_A += v.color.r;
        for (auto& v : meshB.vertices) avgR_B += v.color.r;
        avgR_A /= static_cast<float>(meshA.vertices.size());
        avgR_B /= static_cast<float>(meshB.vertices.size());
        terrainColorDiffers = std::abs(avgR_A - avgR_B) > 0.001f;
        testCheck(terrainColorDiffers, "Different chunks have different average terrain color");
    }

    return gFailures;
}

int testBiomeSharedScale() {
    std::printf("Stage 2.1 — Biome shared scale test\n");

    {
        WorldSeed w = 42;
        constexpr float chunkSize = 32.0f;
        constexpr float heightScale = 6.0f;
        ChunkGenerator gen;
        MeshData mesh = gen.generate(w, {0, 0}, chunkSize,
                                     ChunkGenerator::DEFAULT_GRID_SIZE,
                                     heightScale);

        int stride = ChunkGenerator::DEFAULT_GRID_SIZE + 1;
        int idx = 16 * stride + 16;
        float wx = mesh.vertices[idx].pos.x;
        float wz = mesh.vertices[idx].pos.z;
        float h = mesh.vertices[idx].pos.y;

        BiomeSample bs = BiomeGenerator::sampleBiome(w, wx, wz);
        glm::vec3 expected = ChunkGenerator::biomeColor(h, heightScale, bs);
        glm::vec3 actual = mesh.vertices[idx].color;

        float diff = std::abs(expected.r - actual.r) +
                     std::abs(expected.g - actual.g) +
                     std::abs(expected.b - actual.b);
        testCheck(diff < 0.0001f, "Terrain color uses shared BiomeGenerator scale");

        int expectedTrees = static_cast<int>(BiomeGenerator::treeDensity(w, wx, wz) + 0.5f);
        int expectedRocks = static_cast<int>(BiomeGenerator::rockDensity(w, wx, wz) + 0.5f);
        testCheck(ChunkPlacer::biomeTreeCount(w, wx, wz) == expectedTrees,
                  "Tree density uses shared BiomeGenerator scale");
        testCheck(ChunkPlacer::biomeRockCount(w, wx, wz) == expectedRocks,
                  "Rock density uses shared BiomeGenerator scale");
    }

    return gFailures;
}

int testBiomeDensityDeterminism() {
    std::printf("Stage 2.1 — Biome density determinism test\n");

    {
        WorldSeed w = 42;
        int t1 = ChunkPlacer::biomeTreeCount(w, 16.0f, 16.0f);
        int t2 = ChunkPlacer::biomeTreeCount(w, 16.0f, 16.0f);
        testCheck(t1 == t2, "Tree count deterministic");

        int r1 = ChunkPlacer::biomeRockCount(w, 16.0f, 16.0f);
        int r2 = ChunkPlacer::biomeRockCount(w, 16.0f, 16.0f);
        testCheck(r1 == r2, "Rock count deterministic");
    }

    return gFailures;
}

}

namespace procengine {

int runBiomeTests() {
    testBiomeDominant();
    testBiomeDeterminism();
    testBiomeVariation();
    testBiomeSeedVariation();
    testBiomeNormalization();
    testBiomeNoFallbackSnap();
    testBiomeLocalContinuity();
    testBiomeBorderContinuity();
    testBiomeNegativeCoords();
    testTreeDensity();
    testRockDensity();
    testBiomeChunkStreamerIntegration();
    testBiomeSharedScale();
    testBiomeDensityDeterminism();

    return gFailures;
}

}
