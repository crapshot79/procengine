#include "procedural/vegetation/TreeGenerator.h"
#include <cmath>
#include <algorithm>

namespace procengine {

void TreeGenerator::addCylinder(MeshData& mesh, float baseRadius, float topRadius, float height, int segments, const glm::vec3& offset, const glm::vec3& color, float colorVar, float materialType) {
    Rng rng(static_cast<Seed>(std::hash<float>{}(offset.x * 1000.0f + offset.z)));
    int base = static_cast<int>(mesh.vertices.size());

    for (int ring = 0; ring <= 1; ring++) {
        float y = offset.y + height * ring;
        float r = baseRadius + (topRadius - baseRadius) * ring;
        float angleOffset = rng.nextFloat() * 0.15f;

        for (int seg = 0; seg < segments; seg++) {
            float angle = (static_cast<float>(seg) / segments) * 2.0f * 3.14159265f + angleOffset;
            float wobble = 1.0f + rng.range(-0.04f, 0.04f);
            float actualR = r * wobble;

            Vertex v;
            v.pos = glm::vec3(offset.x + actualR * std::cos(angle), y, offset.z + actualR * std::sin(angle));
            v.normal = glm::normalize(glm::vec3(std::cos(angle), 0.2f, std::sin(angle)));
            float cv = rng.range(-colorVar, colorVar);
            v.color = glm::clamp(color + glm::vec3(cv, cv * 0.5f, cv * 0.3f), 0.0f, 1.0f);
            v.materialType = materialType;
            mesh.vertices.push_back(v);
        }
    }

    for (int seg = 0; seg < segments; seg++) {
        int i0 = base + seg;
        int i1 = base + ((seg + 1) % segments);
        int i2 = base + segments + ((seg + 1) % segments);
        int i3 = base + segments + seg;

        mesh.indices.push_back(i0);
        mesh.indices.push_back(i2);
        mesh.indices.push_back(i1);

        mesh.indices.push_back(i0);
        mesh.indices.push_back(i3);
        mesh.indices.push_back(i2);
    }

    int bottomCenter = static_cast<int>(mesh.vertices.size());
    mesh.vertices.push_back({glm::vec3(offset.x, offset.y, offset.z), glm::vec3(0, -1, 0), color * 0.9f, materialType});
    for (int seg = 0; seg < segments; seg++) {
        int i0 = base + seg;
        int i1 = base + ((seg + 1) % segments);
        mesh.indices.push_back(bottomCenter);
        mesh.indices.push_back(i1);
        mesh.indices.push_back(i0);
    }

    int topCenter = static_cast<int>(mesh.vertices.size());
    mesh.vertices.push_back({glm::vec3(offset.x, offset.y + height, offset.z), glm::vec3(0, 1, 0), color * 0.9f, materialType});
    for (int seg = 0; seg < segments; seg++) {
        int i0 = base + segments + seg;
        int i1 = base + segments + ((seg + 1) % segments);
        mesh.indices.push_back(topCenter);
        mesh.indices.push_back(i0);
        mesh.indices.push_back(i1);
    }
}

void TreeGenerator::addCylinderBetween(MeshData& mesh, const glm::vec3& start, const glm::vec3& end, float baseRadius, float topRadius, int segments, const glm::vec3& color, float colorVar, float materialType) {
    glm::vec3 axis = end - start;
    float length = glm::length(axis);
    if (length <= 0.0001f) return;

    glm::vec3 tangent = axis / length;
    glm::vec3 helper = std::abs(tangent.y) > 0.92f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 side = glm::normalize(glm::cross(helper, tangent));
    glm::vec3 up = glm::normalize(glm::cross(tangent, side));

    Rng rng(static_cast<Seed>(std::hash<float>{}(start.x * 917.0f + start.y * 613.0f + start.z * 379.0f)));
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    float angleOffset = rng.nextFloat() * 0.2f;

    for (int ring = 0; ring <= 1; ring++) {
        float t = static_cast<float>(ring);
        float r = baseRadius + (topRadius - baseRadius) * t;
        glm::vec3 center = ring == 0 ? start : end;

        for (int seg = 0; seg < segments; seg++) {
            float angle = (static_cast<float>(seg) / segments) * 2.0f * 3.14159265f + angleOffset;
            glm::vec3 radial = side * std::cos(angle) + up * std::sin(angle);
            float wobble = 1.0f + rng.range(-0.03f, 0.03f);

            Vertex v;
            v.pos = center + radial * (r * wobble);
            v.normal = glm::normalize(radial * 0.95f + tangent * 0.12f);
            float cv = rng.range(-colorVar, colorVar);
            v.color = glm::clamp(color + glm::vec3(cv, cv * 0.5f, cv * 0.25f), 0.0f, 1.0f);
            v.materialType = materialType;
            mesh.vertices.push_back(v);
        }
    }

    for (int seg = 0; seg < segments; seg++) {
        uint32_t i0 = base + static_cast<uint32_t>(seg);
        uint32_t i1 = base + static_cast<uint32_t>((seg + 1) % segments);
        uint32_t i2 = base + static_cast<uint32_t>(segments + ((seg + 1) % segments));
        uint32_t i3 = base + static_cast<uint32_t>(segments + seg);

        mesh.indices.push_back(i0);
        mesh.indices.push_back(i2);
        mesh.indices.push_back(i1);

        mesh.indices.push_back(i0);
        mesh.indices.push_back(i3);
        mesh.indices.push_back(i2);
    }

    uint32_t bottomCenter = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({start, -tangent, color * 0.85f, materialType});
    for (int seg = 0; seg < segments; seg++) {
        uint32_t i0 = base + static_cast<uint32_t>(seg);
        uint32_t i1 = base + static_cast<uint32_t>((seg + 1) % segments);
        mesh.indices.push_back(bottomCenter);
        mesh.indices.push_back(i1);
        mesh.indices.push_back(i0);
    }

    uint32_t topCenter = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({end, tangent, color * 0.9f, materialType});
    for (int seg = 0; seg < segments; seg++) {
        uint32_t i0 = base + static_cast<uint32_t>(segments + seg);
        uint32_t i1 = base + static_cast<uint32_t>(segments + ((seg + 1) % segments));
        mesh.indices.push_back(topCenter);
        mesh.indices.push_back(i0);
        mesh.indices.push_back(i1);
    }
}

void TreeGenerator::addSphere(MeshData& mesh, float radius, int slices, int stacks, const glm::vec3& center, const glm::vec3& color, float colorVar, float materialType) {
    Rng rng(static_cast<Seed>(std::hash<float>{}(center.x * 3717.0f + center.z * 9199.0f)));
    int base = static_cast<int>(mesh.vertices.size());

    for (int stack = 0; stack <= stacks; stack++) {
        float phi = 3.14159265f * static_cast<float>(stack) / stacks;
        for (int slice = 0; slice <= slices; slice++) {
            float theta = 2.0f * 3.14159265f * static_cast<float>(slice) / slices;

            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);

            float stretch = 1.0f;
            if (y > 0.3f) stretch = 1.15f;
            if (y > 0.7f) stretch = 1.25f;
            float squash = 1.0f - 0.2f * (1.0f - y) * (1.0f - y);

            float rx = radius * squash;
            float ry = radius * stretch * 0.85f;
            float rz = radius * squash;

            float wobble = 1.0f + rng.range(-0.06f, 0.06f);

            Vertex v;
            v.pos = center + glm::vec3(x * rx * wobble, y * ry, z * rz * wobble);

            v.normal = glm::normalize(glm::vec3(x * squash, y / (stretch * 0.85f), z * squash));

            float cv = rng.range(-colorVar, colorVar);
            float darkening = 0.85f + 0.15f * (1.0f - y);
            v.color = glm::clamp(color * darkening + glm::vec3(cv * 0.3f, cv, cv * 0.2f), 0.0f, 1.0f);
            v.materialType = materialType;
            mesh.vertices.push_back(v);
        }
    }

    for (int stack = 0; stack < stacks; stack++) {
        for (int slice = 0; slice < slices; slice++) {
            int i00 = base + stack * (slices + 1) + slice;
            int i10 = i00 + 1;
            int i01 = base + (stack + 1) * (slices + 1) + slice;
            int i11 = i01 + 1;

            mesh.indices.push_back(i00);
            mesh.indices.push_back(i01);
            mesh.indices.push_back(i10);

            mesh.indices.push_back(i10);
            mesh.indices.push_back(i01);
            mesh.indices.push_back(i11);
        }
    }
}

MeshData TreeGenerator::generate(Seed seed) {
    Rng rng(seed);
    float trunkH = rng.range(2.0f, 3.5f);
    float trunkR = rng.range(0.08f, 0.2f);
    float crownR = rng.range(0.8f, 1.8f);
    return generate(seed, trunkH, trunkR, crownR);
}

MeshData TreeGenerator::generate(Seed seed, float trunkHeight, float trunkRadius, float crownRadius) {
    MeshData mesh;
    Rng rng(seed);

    glm::vec3 barkColor(rng.range(0.3f, 0.45f), rng.range(0.2f, 0.3f), rng.range(0.08f, 0.15f));
    glm::vec3 leafColor(rng.range(0.1f, 0.25f), rng.range(0.35f, 0.55f), rng.range(0.05f, 0.15f));

    int segments = 8;

    float trunkH = trunkHeight;
    float baseR = trunkRadius;
    float topR = trunkRadius * rng.range(0.5f, 0.75f);

    addCylinder(mesh, baseR, topR, trunkH, segments, glm::vec3(0, 0, 0), barkColor, 0.04f, MATERIAL_BARK);

    int branchCount = rng.rangeInt(2, 4);
    for (int b = 0; b < branchCount; b++) {
        float branchHeight = trunkH * rng.range(0.5f, 0.85f);
        float branchAngle = rng.nextFloat() * 2.0f * 3.14159265f;
        float branchLen = rng.range(0.45f, 0.85f) * crownRadius;
        float branchUp = rng.range(0.22f, 0.48f);

        glm::vec3 branchStart(0.0f, branchHeight, 0.0f);
        glm::vec3 branchEnd(
            std::cos(branchAngle) * branchLen,
            branchHeight + branchUp * branchLen,
            std::sin(branchAngle) * branchLen);

        addCylinderBetween(mesh, branchStart, branchEnd, topR * 0.58f, topR * 0.16f, 6,
            barkColor * 0.9f, 0.03f, MATERIAL_BARK);

        addSphere(mesh, crownRadius * rng.range(0.18f, 0.28f), 7, 4, branchEnd,
            leafColor * glm::vec3(rng.range(0.82f, 0.98f), rng.range(0.92f, 1.08f), rng.range(0.78f, 0.95f)),
            0.04f, MATERIAL_LEAVES);
    }

    float crownBase = trunkH * 0.65f;
    float crownTop = trunkH + crownRadius * 1.8f;

    float crownCenterY = (crownBase + crownTop) * 0.5f;
    addSphere(mesh, crownRadius, 10, 7,
        glm::vec3(0, crownCenterY, 0), leafColor, 0.06f, MATERIAL_LEAVES);
    addSphere(mesh, crownRadius * 0.62f, 8, 5,
        glm::vec3(0, crownBase + crownRadius * 0.22f, 0), leafColor * glm::vec3(0.82f, 0.95f, 0.78f), 0.05f, MATERIAL_LEAVES);

    int puffCount = rng.rangeInt(3, 5);
    for (int p = 0; p < puffCount; p++) {
        float angle = rng.nextFloat() * 2.0f * 3.14159265f;
        float dist = crownRadius * rng.range(0.4f, 0.8f);
        float yOffset = rng.range(-0.3f, 0.5f) * crownRadius;
        float puffR = crownRadius * rng.range(0.5f, 0.85f);

        glm::vec3 puffCenter(std::cos(angle) * dist, crownCenterY + yOffset, std::sin(angle) * dist);
        addSphere(mesh, puffR, 8, 5, puffCenter, leafColor * glm::vec3(rng.range(0.9f, 1.1f), rng.range(0.9f, 1.1f), rng.range(0.85f, 1.0f)), 0.05f, MATERIAL_LEAVES);
    }

    return mesh;
}

}
