#include "procedural/rock/RockGenerator.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace procengine {

MeshData RockGenerator::generate(Seed seed) {
    Rng rng(seed);
    float radius = rng.range(0.5f, 1.5f);
    float scaleX = rng.range(0.7f, 1.5f);
    float scaleY = rng.range(0.4f, 0.9f);
    float scaleZ = rng.range(0.7f, 1.4f);
    float rotY = rng.range(0.0f, 6.283f);
    return generate(seed, radius, scaleX, scaleY, scaleZ, rotY);
}

MeshData RockGenerator::generate(Seed seed, float radius, float scaleX, float scaleY, float scaleZ, float rotationY) {
    MeshData mesh;
    Rng rng(seed);

    glm::vec3 baseColor(
        rng.range(0.35f, 0.55f),
        rng.range(0.32f, 0.48f),
        rng.range(0.28f, 0.42f)
    );

    int segments = 8;
    int rings = 6;

    struct Vert {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
    };

    std::vector<Vert> points;
    points.reserve(static_cast<size_t>((rings - 1) * segments + 2));

    auto makePoint = [&](float nx, float ny, float nz, bool pole) -> Vert {
        float noiseScale = pole ? 0.04f : 0.15f + 0.15f * (1.0f - std::abs(ny));
        float noise = rng.range(-noiseScale, noiseScale);
        float squish = 1.0f + rng.range(-0.1f, 0.1f);

        float px = nx * (1.0f + noise) * squish;
        float py = ny * (1.0f + noise * 0.5f);
        float pz = nz * (1.0f + noise) * squish;

        float cosR = std::cos(rotationY);
        float sinR = std::sin(rotationY);
        float rx = px * cosR - pz * sinR;
        float rz = px * sinR + pz * cosR;

        glm::vec3 pos(rx * radius * scaleX, py * radius * scaleY, rz * radius * scaleZ);
        glm::vec3 normal = glm::length(pos) > 0.0001f ? glm::normalize(pos) : glm::vec3(0, 1, 0);

        float colorVar = rng.range(-0.04f, 0.04f);
        glm::vec3 color = glm::clamp(baseColor + glm::vec3(colorVar, colorVar * 0.8f, colorVar * 0.6f), 0.0f, 1.0f);
        return {pos, normal, color};
    };

    Vert top = makePoint(0.0f, 1.0f, 0.0f, true);
    Vert bottom = makePoint(0.0f, -1.0f, 0.0f, true);

    for (int ring = 1; ring < rings; ring++) {
        float phi = 3.14159265f * static_cast<float>(ring) / rings;
        for (int seg = 0; seg < segments; seg++) {
            float theta = 2.0f * 3.14159265f * static_cast<float>(seg) / segments;

            float nx = std::sin(phi) * std::cos(theta);
            float ny = std::cos(phi);
            float nz = std::sin(phi) * std::sin(theta);

            points.push_back(makePoint(nx, ny, nz, false));
        }
    }

    uint32_t topIndex = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({top.pos, top.normal, top.color, MATERIAL_ROCK});

    uint32_t firstRingBase = static_cast<uint32_t>(mesh.vertices.size());
    for (const auto& point : points) {
        mesh.vertices.push_back({point.pos, point.normal, point.color, MATERIAL_ROCK});
    }

    uint32_t bottomIndex = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({bottom.pos, bottom.normal, bottom.color, MATERIAL_ROCK});

    for (int seg = 0; seg < segments; seg++) {
        uint32_t i0 = firstRingBase + static_cast<uint32_t>(seg);
        uint32_t i1 = firstRingBase + static_cast<uint32_t>((seg + 1) % segments);
        mesh.indices.push_back(topIndex);
        mesh.indices.push_back(i1);
        mesh.indices.push_back(i0);
    }

    for (int ring = 0; ring < rings - 2; ring++) {
        for (int seg = 0; seg < segments; seg++) {
            uint32_t i00 = firstRingBase + static_cast<uint32_t>(ring * segments + seg);
            uint32_t i10 = firstRingBase + static_cast<uint32_t>(ring * segments + ((seg + 1) % segments));
            uint32_t i01 = firstRingBase + static_cast<uint32_t>((ring + 1) * segments + seg);
            uint32_t i11 = firstRingBase + static_cast<uint32_t>((ring + 1) * segments + ((seg + 1) % segments));

            mesh.indices.push_back(i00);
            mesh.indices.push_back(i10);
            mesh.indices.push_back(i01);

            mesh.indices.push_back(i10);
            mesh.indices.push_back(i11);
            mesh.indices.push_back(i01);
        }
    }

    uint32_t lastRingBase = firstRingBase + static_cast<uint32_t>((rings - 2) * segments);
    for (int seg = 0; seg < segments; seg++) {
        uint32_t i0 = lastRingBase + static_cast<uint32_t>(seg);
        uint32_t i1 = lastRingBase + static_cast<uint32_t>((seg + 1) % segments);
        mesh.indices.push_back(bottomIndex);
        mesh.indices.push_back(i0);
        mesh.indices.push_back(i1);
    }

    return mesh;
}

}
