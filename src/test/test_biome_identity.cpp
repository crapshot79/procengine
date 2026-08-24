#include "TestUtil.h"
#include "procedural/vegetation/TreeGenerator.h"
#include "procedural/rock/RockGenerator.h"
#include "procedural/world/Biome.h"
#include "procedural/core/Seed.h"
#include <cmath>
#include <algorithm>

namespace {

using namespace procengine;

float meshHeight(const MeshData& m) {
    float mn = 1e9f, mx = -1e9f;
    for (auto& v : m.vertices) {
        mn = std::min(mn, v.pos.y);
        mx = std::max(mx, v.pos.y);
    }
    return mx - mn;
}

float meshWidth(const MeshData& m) {
    float mn = 1e9f, mx = -1e9f;
    for (auto& v : m.vertices) {
        mn = std::min(mn, v.pos.x);
        mx = std::max(mx, v.pos.x);
    }
    return mx - mn;
}

int testTreeDeterminism() {
    std::printf("Stage 2.3 — Tree determinism test\n");

    {
        BiomeSample grass{1.0f, 0.0f, 0.0f};
        TreeGenerator gen;
        MeshData a = gen.generate(42, grass);
        MeshData b = gen.generate(42, grass);
        bool same = (a.vertices.size() == b.vertices.size());
        if (same) {
            for (size_t i = 0; i < a.vertices.size(); ++i) {
                if (std::abs(a.vertices[i].pos.y - b.vertices[i].pos.y) > 0.0001f) { same = false; break; }
            }
        }
        testCheck(same, "Same seed + same biome -> identical tree");
    }

    return gFailures;
}

int testRockDeterminism() {
    std::printf("Stage 2.3 — Rock determinism test\n");

    {
        BiomeSample high{0.0f, 0.0f, 1.0f};
        RockGenerator gen;
        MeshData a = gen.generate(99, high);
        MeshData b = gen.generate(99, high);
        bool same = (a.vertices.size() == b.vertices.size());
        if (same) {
            for (size_t i = 0; i < a.vertices.size(); ++i) {
                if (std::abs(a.vertices[i].pos.y - b.vertices[i].pos.y) > 0.0001f) { same = false; break; }
            }
        }
        testCheck(same, "Same seed + same biome -> identical rock");
    }

    return gFailures;
}

int testTreeGrasslandSmaller() {
    std::printf("Stage 2.3 — Tree grassland smaller test\n");

    {
        BiomeSample grass{1.0f, 0.0f, 0.0f};
        BiomeSample forest{0.0f, 1.0f, 0.0f};
        TreeGenerator gen;

        float grassH = 0, forestH = 0;
        for (int i = 0; i < 30; ++i) {
            grassH += meshHeight(gen.generate(100 + i, grass));
            forestH += meshHeight(gen.generate(100 + i, forest));
        }
        grassH /= 30.0f;
        forestH /= 30.0f;

        testCheck(grassH < forestH, "Grassland trees shorter than forest trees");
    }

    return gFailures;
}

int testTreeHighlandStunted() {
    std::printf("Stage 2.3 — Tree highland stunted test\n");

    {
        BiomeSample forest{0.0f, 1.0f, 0.0f};
        BiomeSample high{0.0f, 0.0f, 1.0f};
        TreeGenerator gen;

        float forestH = 0, highH = 0;
        for (int i = 0; i < 30; ++i) {
            forestH += meshHeight(gen.generate(200 + i, forest));
            highH += meshHeight(gen.generate(200 + i, high));
        }
        forestH /= 30.0f;
        highH /= 30.0f;

        testCheck(highH < forestH, "Highland trees shorter than forest trees");
    }

    return gFailures;
}

int testRockHighlandLarger() {
    std::printf("Stage 2.3 — Rock highland larger test\n");

    {
        BiomeSample grass{1.0f, 0.0f, 0.0f};
        BiomeSample high{0.0f, 0.0f, 1.0f};
        RockGenerator gen;

        float grassW = 0, highW = 0;
        for (int i = 0; i < 30; ++i) {
            grassW += meshWidth(gen.generate(300 + i, grass));
            highW += meshWidth(gen.generate(300 + i, high));
        }
        grassW /= 30.0f;
        highW /= 30.0f;

        testCheck(highW > grassW, "Highland rocks wider than grassland rocks");
    }

    return gFailures;
}

int testTreeWidth() {
    std::printf("Stage 2.3 — Tree crown width test\n");

    {
        BiomeSample grass{1.0f, 0.0f, 0.0f};
        BiomeSample forest{0.0f, 1.0f, 0.0f};
        TreeGenerator gen;

        float grassW = 0, forestW = 0;
        for (int i = 0; i < 30; ++i) {
            grassW += meshWidth(gen.generate(400 + i, grass));
            forestW += meshWidth(gen.generate(400 + i, forest));
        }
        grassW /= 30.0f;
        forestW /= 30.0f;

        testCheck(forestW > grassW, "Forest trees wider (fuller crowns) than grassland");
    }

    return gFailures;
}

int testTreeColorVariation() {
    std::printf("Stage 2.3 — Tree color variation by biome\n");

    {
        BiomeSample forest{0.0f, 1.0f, 0.0f};
        BiomeSample high{0.0f, 0.0f, 1.0f};
        TreeGenerator gen;

        auto avgLeafColor = [&](const BiomeSample& b) {
            glm::vec3 sum(0);
            int count = 0;
            MeshData m = gen.generate(500, b);
            for (auto& v : m.vertices) {
                if (v.materialType == MATERIAL_LEAVES) { sum += v.color; count++; }
            }
            return count > 0 ? sum / static_cast<float>(count) : glm::vec3(0);
        };

        glm::vec3 fc = avgLeafColor(forest);
        glm::vec3 hc = avgLeafColor(high);

        float forestGreen = fc.y;
        float highGreen = hc.y;
        testCheck(highGreen < forestGreen, "Highland leaves less green/greyer than forest");
    }

    return gFailures;
}

}

namespace procengine {

int runBiomeIdentityTests() {
    testTreeDeterminism();
    testRockDeterminism();
    testTreeGrasslandSmaller();
    testTreeHighlandStunted();
    testRockHighlandLarger();
    testTreeWidth();
    testTreeColorVariation();

    return gFailures;
}

}
