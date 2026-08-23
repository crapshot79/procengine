#include "procedural/world/ChunkCoord.h"
#include "procedural/world/ChunkGenerator.h"
#include "procedural/world/WorldSeed.h"
#include "engine/renderer/Mesh.h"
#include "test/TestUtil.h"

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>

namespace procengine {

static void writeChunkOBJ(const MeshData& m, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::printf("  Could not open %s for writing\n", path.c_str());
        return;
    }
    out << "# chunk terrain mesh\n";
    out << "# vertices: " << m.vertices.size() << "\n";
    out << "# triangles: " << (m.indices.size() / 3) << "\n";
    for (const auto& v : m.vertices) {
        out << "v " << v.pos.x << ' ' << v.pos.y << ' ' << v.pos.z << "\n";
    }
    for (size_t i = 0; i < m.indices.size(); i += 3) {
        out << "f "
            << (m.indices[i]   + 1) << ' '
            << (m.indices[i+1] + 1) << ' '
            << (m.indices[i+2] + 1) << "\n";
    }
}

int runTestChunkDump() {
    std::printf("Stage 1.2 — ChunkGenerator one-off dump\n");

    ChunkGenerator gen;
    WorldSeed w = 42ULL;
    constexpr float chunkSize = ChunkGenerator::DEFAULT_CHUNK_SIZE;
    constexpr int   gridSize  = ChunkGenerator::DEFAULT_GRID_SIZE;

    MeshData a = gen.generate(w, ChunkCoord{0, 0}, chunkSize, gridSize, 6.0f);
    MeshData b = gen.generate(w, ChunkCoord{1, 0}, chunkSize, gridSize, 6.0f);
    MeshData c = gen.generate(w, ChunkCoord{0, 1}, chunkSize, gridSize, 6.0f);

    int stride = gridSize + 1;

    int leftCol  = stride - 1;
    int topRow   = stride - 1;

    std::printf("  Chunk (0,0):\n");
    std::printf("    vert[0   ]  world=(%.3f, %.3f, %.3f)\n", a.vertices[0].pos.x, a.vertices[0].pos.y, a.vertices[0].pos.z);
    std::printf("    vert[NW  ]  world=(%.3f, %.3f, %.3f)\n", a.vertices[leftCol].pos.x, a.vertices[leftCol].pos.y, a.vertices[leftCol].pos.z);
    std::printf("    vert[SE  ]  world=(%.3f, %.3f, %.3f)\n", a.vertices[topRow * stride + stride - 1].pos.x, a.vertices[topRow * stride + stride - 1].pos.y, a.vertices[topRow * stride + stride - 1].pos.z);

    std::printf("  Border match (right edge of (0,0) vs left edge of (1,0)):\n");
    int shared = 5;
    int indices[] = {0, stride/4, stride/2, (3*stride)/4, stride - 1};
    for (int idx : indices) {
        float lh = a.vertices[idx * stride + leftCol].pos.y;
        float rh = b.vertices[idx * stride + 0].pos.y;
        std::printf("    row %2d: chunkA.y=%.6f  chunkB.y=%.6f  diff=%.2e\n",
                    idx, lh, rh, std::fabs(lh - rh));
    }

    std::printf("  Border match (top edge of (0,0) vs bottom edge of (0,1)):\n");
    int ixs2[] = {0, stride/4, stride/2, (3*stride)/4, stride - 1};
    for (int idx : ixs2) {
        float bh = a.vertices[topRow * stride + idx].pos.y;
        float th = c.vertices[0 * stride + idx].pos.y;
        std::printf("    col %2d: chunkA.y=%.6f  chunkC.y=%.6f  diff=%.2e\n",
                    idx, bh, th, std::fabs(bh - th));
    }

    float hMinA = 1e30f, hMaxA = -1e30f;
    float hMinB = 1e30f, hMaxB = -1e30f;
    for (auto& v : a.vertices) { if (v.pos.y < hMinA) hMinA = v.pos.y; if (v.pos.y > hMaxA) hMaxA = v.pos.y; }
    for (auto& v : b.vertices) { if (v.pos.y < hMinB) hMinB = v.pos.y; if (v.pos.y > hMaxB) hMaxB = v.pos.y; }
    std::printf("  Height range (0,0): [%.3f, %.3f]\n", hMinA, hMaxA);
    std::printf("  Height range (1,0): [%.3f, %.3f]\n", hMinB, hMaxB);
    std::printf("  These differ because the chunks sample different noise regions.\n");

    std::string outPath = "D:/opencodeprojects/procengine/build/Debug/chunk_0_0.obj";
    writeChunkOBJ(a, outPath);
    std::printf("  Wrote %s (%zu verts, %zu tris)\n", outPath.c_str(), a.vertices.size(), a.indices.size() / 3);

    std::string outPath2 = "D:/opencodeprojects/procengine/build/Debug/chunk_1_0.obj";
    writeChunkOBJ(b, outPath2);
    std::printf("  Wrote %s\n", outPath2.c_str());

    std::string outPath3 = "D:/opencodeprojects/procengine/build/Debug/chunk_0_1.obj";
    writeChunkOBJ(c, outPath3);
    std::printf("  Wrote %s\n", outPath3.c_str());

    return gFailures;
}

}