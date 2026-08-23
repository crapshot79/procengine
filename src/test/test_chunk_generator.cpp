#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/WorldSeed.h"
#include "engine/renderer/Mesh.h"
#include "test/TestUtil.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <set>
#include <tuple>

namespace procengine {

static float meshMaxHeight(const MeshData& m) {
    float h = -1e30f;
    for (auto& v : m.vertices) if (v.pos.y > h) h = v.pos.y;
    return h;
}

static float meshMinHeight(const MeshData& m) {
    float h = 1e30f;
    for (auto& v : m.vertices) if (v.pos.y < h) h = v.pos.y;
    return h;
}

static bool equalMesh(const MeshData& a, const MeshData& b) {
    if (a.vertices.size() != b.vertices.size()) return false;
    if (a.indices.size() != b.indices.size()) return false;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].pos != b.vertices[i].pos) return false;
        if (a.vertices[i].color != b.vertices[i].color) return false;
    }
    for (size_t i = 0; i < a.indices.size(); ++i) {
        if (a.indices[i] != b.indices[i]) return false;
    }
    return true;
}

static float vertexHeightAtRow(const MeshData& m, int stride, int row, int col) {
    return m.vertices[row * stride + col].pos.y;
}

static float normalDiffAtRow(const MeshData& a, const MeshData& b, int stride, int row, int colA, int colB) {
    glm::vec3 d = a.vertices[row * stride + colA].normal - b.vertices[row * stride + colB].normal;
    return glm::length(d);
}

int runTestChunkGenerator() {
    std::printf("Stage 1.2 — ChunkGenerator test\n");

    constexpr float chunkSize = ChunkGenerator::DEFAULT_CHUNK_SIZE;
    constexpr int   gridSize  = ChunkGenerator::DEFAULT_GRID_SIZE;

    ChunkGenerator gen;
    WorldSeed w = 42ULL;

    MeshData a = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 6.0f);
    MeshData b = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 6.0f);
    testCheck(equalMesh(a, b), "Determinism: same (world, cc) -> identical mesh");

    MeshData a2 = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 6.0f);
    testCheck(equalMesh(a, a2), "Determinism: triple-check identical mesh");

    MeshData otherChunk = gen.generate(w, ChunkCoord{1, 0}, chunkSize, gridSize, 6.0f);
    testCheck(!equalMesh(a, otherChunk), "Variation: different cc -> different mesh");

    float minX = a.vertices[0].pos.x;
    float maxX = a.vertices[0].pos.x;
    float minZ = a.vertices[0].pos.z;
    float maxZ = a.vertices[0].pos.z;
    for (auto& v : a.vertices) {
        if (v.pos.x < minX) minX = v.pos.x;
        if (v.pos.x > maxX) maxX = v.pos.x;
        if (v.pos.z < minZ) minZ = v.pos.z;
        if (v.pos.z > maxZ) maxZ = v.pos.z;
    }
    testCheck(minX == 0.0f && std::fabs(maxX - (chunkSize - 1e-4f)) < 1e-3f,
        "World coords: chunk (0,0) x-range [0, chunkSize]");
    testCheck(minZ == 0.0f && std::fabs(maxZ - (chunkSize - 1e-4f)) < 1e-3f,
        "World coords: chunk (0,0) z-range [0, chunkSize]");

    MeshData c12 = gen.generate(w, ChunkCoord{1, 2}, chunkSize, gridSize, 6.0f);
    bool xRangeOK = (c12.vertices[0].pos.x >= chunkSize - 1e-4f) &&
                    (c12.vertices.back().pos.x <= chunkSize * 2.0f + 1e-4f);
    bool zRangeOK = (c12.vertices[0].pos.z >= chunkSize * 2.0f - 1e-4f) &&
                    (c12.vertices.back().pos.z <= chunkSize * 3.0f + 1e-4f);
    testCheck(xRangeOK && zRangeOK, "World coords: chunk (1,2) at correct world offset");

    constexpr float kTol = 1e-4f;

    {
        MeshData left  = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 6.0f);
        MeshData right = gen.generate(w, ChunkCoord{1, 0}, chunkSize, gridSize, 6.0f);

        int stride2 = gridSize + 1;
        int sharedCol = stride2 - 1;
        float maxDiff = 0.0f;
        float maxNormalDiff = 0.0f;
        for (int row = 0; row <= gridSize; ++row) {
            float lh = vertexHeightAtRow(left,  stride2, row, sharedCol);
            float rh = vertexHeightAtRow(right, stride2, row, 0);
            maxDiff = std::max(maxDiff, std::fabs(lh - rh));
            maxNormalDiff = std::max(maxNormalDiff, normalDiffAtRow(left, right, stride2, row, sharedCol, 0));
        }
        testCheck(maxDiff < kTol,
            "Seamlessness: right edge of (0,0) matches left edge of (1,0)");
        testCheck(maxNormalDiff < kTol,
            "Seamlessness: right/left edge normals match across chunk boundary");
    }

    {
        MeshData bottom = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 6.0f);
        MeshData top    = gen.generate(w, ChunkCoord{0, 1}, chunkSize, gridSize, 6.0f);

        int stride2 = gridSize + 1;
        int sharedRow = stride2 - 1;
        float maxDiff = 0.0f;
        float maxNormalDiff = 0.0f;
        for (int col = 0; col <= gridSize; ++col) {
            float bh = bottom.vertices[sharedRow * stride2 + col].pos.y;
            float th = top.vertices[col].pos.y;
            maxDiff = std::max(maxDiff, std::fabs(bh - th));
            glm::vec3 dn = bottom.vertices[sharedRow * stride2 + col].normal - top.vertices[col].normal;
            maxNormalDiff = std::max(maxNormalDiff, glm::length(dn));
        }
        testCheck(maxDiff < kTol,
            "Seamlessness: top edge of (0,0) matches bottom edge of (0,1)");
        testCheck(maxNormalDiff < kTol,
            "Seamlessness: top/bottom edge normals match across chunk boundary");
    }

    {
        MeshData neg = gen.generate(w, ChunkCoord{-1, -1}, chunkSize, gridSize, 6.0f);
        bool negOK = (neg.vertices[0].pos.x >= -chunkSize - 1e-4f) &&
                     (neg.vertices.back().pos.z <= 0.0f + 1e-4f);
        testCheck(negOK, "World coords: negative chunk coords map to correct world range");
    }

    {
        WorldSeed wA = 1ULL;
        WorldSeed wB = 2ULL;
        MeshData ma = gen.generate(wA, ChunkCoord{3, -2}, chunkSize, gridSize, 6.0f);
        MeshData mb = gen.generate(wB, ChunkCoord{3, -2}, chunkSize, gridSize, 6.0f);
        testCheck(!equalMesh(ma, mb), "World seed variation: same cc, different world -> different terrain");
    }

    {
        MeshData a5 = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 5.0f);
        MeshData a6 = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 6.0f);
        bool scaledOK = std::fabs(meshMaxHeight(a6) - meshMaxHeight(a5) * (6.0f / 5.0f)) < 1e-2f;
        testCheck(scaledOK, "Height scale: doubling heightScale ~ doubles max height");
    }

    {
        std::set<std::tuple<int, int, int, int>> coordsSeen;
        for (int zi = -2; zi <= 2; ++zi) {
            for (int xi = -2; xi <= 2; ++xi) {
                MeshData m = gen.generate(w, ChunkCoord{xi, zi}, chunkSize, gridSize, 6.0f);
                coordsSeen.insert({xi, zi, (int)m.vertices.size(), (int)m.indices.size()});
            }
        }
        testCheck(coordsSeen.size() == 25u,
            "Each chunk (5x5 ring) produces an independent mesh");
    }

    int stride = gridSize + 1;
    testCheck(a.vertices.size() == static_cast<size_t>(stride * stride),
        "Vertex count matches stride*stride");
    testCheck(a.indices.size() == static_cast<size_t>(6 * gridSize * gridSize),
        "Index count matches 6 * gridSize^2 (two triangles per cell)");

    testCheck(meshMinHeight(a) >= -1e-4f && meshMaxHeight(a) <= 6.0f + 1e-4f,
        "All terrain heights within [0, heightScale] range");

    return gFailures;
}

}
