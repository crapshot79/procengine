#include "procedural/building/HouseGenerator.h"

#include <algorithm>

namespace procengine {

void HouseGenerator::addQuad(MeshData& mesh,
    const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
    const glm::vec3& normal, const glm::vec3& color, float materialType) {
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

    mesh.vertices.push_back({p0, normal, color, materialType});
    mesh.vertices.push_back({p1, normal, color, materialType});
    mesh.vertices.push_back({p2, normal, color, materialType});
    mesh.vertices.push_back({p3, normal, color, materialType});

    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);

    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}

void HouseGenerator::addBox(MeshData& mesh, const glm::vec3& min, const glm::vec3& max,
    const glm::vec3& color, float materialType) {
    if (min.x >= max.x || min.y >= max.y || min.z >= max.z) return;

    addQuad(mesh,
        glm::vec3(min.x, min.y, max.z), glm::vec3(max.x, min.y, max.z),
        glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z),
        glm::vec3(0, 0, 1), color, materialType);

    addQuad(mesh,
        glm::vec3(max.x, min.y, min.z), glm::vec3(min.x, min.y, min.z),
        glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z),
        glm::vec3(0, 0, -1), color, materialType);

    addQuad(mesh,
        glm::vec3(min.x, min.y, min.z), glm::vec3(min.x, min.y, max.z),
        glm::vec3(min.x, max.y, max.z), glm::vec3(min.x, max.y, min.z),
        glm::vec3(-1, 0, 0), color, materialType);

    addQuad(mesh,
        glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, min.y, min.z),
        glm::vec3(max.x, max.y, min.z), glm::vec3(max.x, max.y, max.z),
        glm::vec3(1, 0, 0), color, materialType);

    addQuad(mesh,
        glm::vec3(min.x, max.y, max.z), glm::vec3(max.x, max.y, max.z),
        glm::vec3(max.x, max.y, min.z), glm::vec3(min.x, max.y, min.z),
        glm::vec3(0, 1, 0), color, materialType);

    addQuad(mesh,
        glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z),
        glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z),
        glm::vec3(0, -1, 0), color, materialType);
}

void HouseGenerator::addWallZ(MeshData& mesh, float zMin, float zMax, float xMin, float xMax,
    float yMin, float yMax, const std::vector<Opening>& openings,
    const glm::vec3& color, float materialType) {
    std::vector<float> xs = {xMin, xMax};
    std::vector<float> ys = {yMin, yMax};

    for (const auto& opening : openings) {
        float left = std::clamp(opening.center - opening.width * 0.5f, xMin, xMax);
        float right = std::clamp(opening.center + opening.width * 0.5f, xMin, xMax);
        float bottom = std::clamp(opening.bottom, yMin, yMax);
        float top = std::clamp(opening.top, yMin, yMax);
        if (left < right && bottom < top) {
            xs.push_back(left);
            xs.push_back(right);
            ys.push_back(bottom);
            ys.push_back(top);
        }
    }

    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

    for (size_t xi = 0; xi + 1 < xs.size(); xi++) {
        for (size_t yi = 0; yi + 1 < ys.size(); yi++) {
            float cx = (xs[xi] + xs[xi + 1]) * 0.5f;
            float cy = (ys[yi] + ys[yi + 1]) * 0.5f;
            bool insideOpening = false;
            for (const auto& opening : openings) {
                float left = opening.center - opening.width * 0.5f;
                float right = opening.center + opening.width * 0.5f;
                if (cx > left && cx < right && cy > opening.bottom && cy < opening.top) {
                    insideOpening = true;
                    break;
                }
            }
            if (!insideOpening) {
                addBox(mesh,
                    glm::vec3(xs[xi], ys[yi], zMin),
                    glm::vec3(xs[xi + 1], ys[yi + 1], zMax),
                    color, materialType);
            }
        }
    }
}

void HouseGenerator::addWallX(MeshData& mesh, float xMin, float xMax, float zMin, float zMax,
    float yMin, float yMax, const std::vector<Opening>& openings,
    const glm::vec3& color, float materialType) {
    std::vector<float> zs = {zMin, zMax};
    std::vector<float> ys = {yMin, yMax};

    for (const auto& opening : openings) {
        float left = std::clamp(opening.center - opening.width * 0.5f, zMin, zMax);
        float right = std::clamp(opening.center + opening.width * 0.5f, zMin, zMax);
        float bottom = std::clamp(opening.bottom, yMin, yMax);
        float top = std::clamp(opening.top, yMin, yMax);
        if (left < right && bottom < top) {
            zs.push_back(left);
            zs.push_back(right);
            ys.push_back(bottom);
            ys.push_back(top);
        }
    }

    std::sort(zs.begin(), zs.end());
    zs.erase(std::unique(zs.begin(), zs.end()), zs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

    for (size_t zi = 0; zi + 1 < zs.size(); zi++) {
        for (size_t yi = 0; yi + 1 < ys.size(); yi++) {
            float cz = (zs[zi] + zs[zi + 1]) * 0.5f;
            float cy = (ys[yi] + ys[yi + 1]) * 0.5f;
            bool insideOpening = false;
            for (const auto& opening : openings) {
                float left = opening.center - opening.width * 0.5f;
                float right = opening.center + opening.width * 0.5f;
                if (cz > left && cz < right && cy > opening.bottom && cy < opening.top) {
                    insideOpening = true;
                    break;
                }
            }
            if (!insideOpening) {
                addBox(mesh,
                    glm::vec3(xMin, ys[yi], zs[zi]),
                    glm::vec3(xMax, ys[yi + 1], zs[zi + 1]),
                    color, materialType);
            }
        }
    }
}

void HouseGenerator::addGableWallZ(MeshData& mesh, float zMin, float zMax, float xMin, float xMax,
    float baseY, float peakY, const glm::vec3& color, float materialType) {
    float zMid = (zMin + zMax) * 0.5f;
    bool facesPositiveZ = zMid > 0.0f;
    float zOuter = facesPositiveZ ? zMax : zMin;
    float zInner = facesPositiveZ ? zMin : zMax;

    glm::vec3 a0(xMin, baseY, zOuter);
    glm::vec3 b0(xMax, baseY, zOuter);
    glm::vec3 c0(0.0f, peakY, zOuter);
    glm::vec3 a1(xMin, baseY, zInner);
    glm::vec3 b1(xMax, baseY, zInner);
    glm::vec3 c1(0.0f, peakY, zInner);

    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    if (facesPositiveZ) {
        mesh.vertices.push_back({a0, glm::vec3(0, 0, 1), color, materialType});
        mesh.vertices.push_back({b0, glm::vec3(0, 0, 1), color, materialType});
        mesh.vertices.push_back({c0, glm::vec3(0, 0, 1), color, materialType});
        mesh.vertices.push_back({b1, glm::vec3(0, 0, -1), color * 0.82f, materialType});
        mesh.vertices.push_back({a1, glm::vec3(0, 0, -1), color * 0.82f, materialType});
        mesh.vertices.push_back({c1, glm::vec3(0, 0, -1), color * 0.82f, materialType});
    } else {
        mesh.vertices.push_back({b0, glm::vec3(0, 0, -1), color, materialType});
        mesh.vertices.push_back({a0, glm::vec3(0, 0, -1), color, materialType});
        mesh.vertices.push_back({c0, glm::vec3(0, 0, -1), color, materialType});
        mesh.vertices.push_back({a1, glm::vec3(0, 0, 1), color * 0.82f, materialType});
        mesh.vertices.push_back({b1, glm::vec3(0, 0, 1), color * 0.82f, materialType});
        mesh.vertices.push_back({c1, glm::vec3(0, 0, 1), color * 0.82f, materialType});
    }
    mesh.indices.insert(mesh.indices.end(), {base + 0, base + 1, base + 2, base + 3, base + 4, base + 5});

    addQuad(mesh, a1, a0, c0, c1, glm::vec3(-1, 0.35f, 0), color, materialType);
    addQuad(mesh, b0, b1, c1, c0, glm::vec3(1, 0.35f, 0), color, materialType);
    addQuad(mesh, a0, a1, b1, b0, glm::vec3(0, -1, 0), color * 0.85f, materialType);
}

void HouseGenerator::addRoofSlab(MeshData& mesh,
    const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
    float thickness, const glm::vec3& topNormal, const glm::vec3& color, float materialType) {
    glm::vec3 drop(0.0f, thickness, 0.0f);
    glm::vec3 ai = a - drop;
    glm::vec3 bi = b - drop;
    glm::vec3 ci = c - drop;
    glm::vec3 di = d - drop;

    addQuad(mesh, a, b, c, d, topNormal, color, materialType);
    addQuad(mesh, di, ci, bi, ai, -topNormal, color * 0.75f, materialType);
    addQuad(mesh, ai, a, d, di, glm::vec3(-1, 0, 0), color * 0.8f, materialType);
    addQuad(mesh, b, bi, ci, c, glm::vec3(1, 0, 0), color * 0.8f, materialType);
    addQuad(mesh, ai, bi, b, a, glm::vec3(0, 0, -1), color * 0.78f, materialType);
    addQuad(mesh, d, c, ci, di, glm::vec3(0, 0, 1), color * 0.78f, materialType);
}

MeshData HouseGenerator::generate(Seed seed, float width, float height, float depth) {
    MeshData mesh;
    Rng rng(seed);

    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    float wallT = 0.16f;

    glm::vec3 wallColor(rng.range(0.82f, 0.88f), rng.range(0.76f, 0.82f), rng.range(0.62f, 0.72f));
    glm::vec3 roofColor(rng.range(0.55f, 0.65f), rng.range(0.18f, 0.25f), rng.range(0.08f, 0.14f));
    glm::vec3 chimneyColor(rng.range(0.45f, 0.55f), rng.range(0.42f, 0.48f), rng.range(0.38f, 0.44f));
    glm::vec3 floorColor(0.4f, 0.3f, 0.2f);

    float doorW = width * 0.32f;
    float doorH = height * 0.64f;
    float winW = width * 0.24f;
    float winH = height * 0.28f;
    float winY = height * 0.5f;
    float sideWinW = depth * 0.22f;

    std::vector<Opening> frontOpenings = {
        {0.0f, doorW, 0.0f, doorH},
        {-hw * 0.58f, winW, winY, winY + winH},
        {hw * 0.58f, winW, winY, winY + winH}
    };
    std::vector<Opening> sideOpenings = {
        {0.0f, sideWinW, height * 0.45f, height * 0.45f + winH}
    };

    addWallZ(mesh, hd - wallT, hd, -hw, hw, 0.0f, height, frontOpenings, wallColor, MATERIAL_WALL);
    addWallZ(mesh, -hd, -hd + wallT, -hw, hw, 0.0f, height, {}, wallColor, MATERIAL_WALL);
    addWallX(mesh, -hw, -hw + wallT, -hd + wallT, hd - wallT, 0.0f, height, sideOpenings, wallColor, MATERIAL_WALL);
    addWallX(mesh, hw - wallT, hw, -hd + wallT, hd - wallT, 0.0f, height, sideOpenings, wallColor, MATERIAL_WALL);

    addBox(mesh, glm::vec3(-hw, -0.12f, -hd), glm::vec3(hw, 0.0f, hd), floorColor, MATERIAL_FLOOR);
    addBox(mesh, glm::vec3(-hw + wallT, height - 0.12f, -hd + wallT),
        glm::vec3(hw - wallT, height, hd - wallT), floorColor * 0.85f, MATERIAL_WALL);

    float roofPeak = height + width * 0.55f;
    float roofOverhang = 0.35f;
    float roofT = 0.14f;
    glm::vec3 roofNormLeft = glm::normalize(glm::vec3(-1, 1.2f, 0));
    glm::vec3 roofNormRight = glm::normalize(glm::vec3(1, 1.2f, 0));

    addRoofSlab(mesh,
        glm::vec3(-hw - roofOverhang, height, -hd - roofOverhang),
        glm::vec3(-hw - roofOverhang, height, hd + roofOverhang),
        glm::vec3(0, roofPeak, hd + roofOverhang),
        glm::vec3(0, roofPeak, -hd - roofOverhang),
        roofT, roofNormLeft, roofColor, MATERIAL_ROOF);

    addRoofSlab(mesh,
        glm::vec3(hw + roofOverhang, height, hd + roofOverhang),
        glm::vec3(hw + roofOverhang, height, -hd - roofOverhang),
        glm::vec3(0, roofPeak, -hd - roofOverhang),
        glm::vec3(0, roofPeak, hd + roofOverhang),
        roofT, roofNormRight, roofColor, MATERIAL_ROOF);

    addGableWallZ(mesh, hd - wallT, hd, -hw, hw, height, roofPeak - roofT, wallColor * 0.95f, MATERIAL_WALL);
    addGableWallZ(mesh, -hd, -hd + wallT, -hw, hw, height, roofPeak - roofT, wallColor * 0.95f, MATERIAL_WALL);

    float chimW = 0.35f;
    float chimD = 0.35f;
    float chimH = roofPeak * 0.35f;
    float chimX = hw * 0.4f;
    float chimZ = 0.0f;
    float chimBaseY = height + (roofPeak - height) * (1.0f - chimX / hw);
    addBox(mesh,
        glm::vec3(chimX - chimW, chimBaseY, chimZ - chimD),
        glm::vec3(chimX + chimW, chimBaseY + chimH, chimZ + chimD),
        chimneyColor, MATERIAL_CHIMNEY);

    return mesh;
}

}
