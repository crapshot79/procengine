#pragma once

#include "engine/renderer/Mesh.h"
#include "procedural/core/Seed.h"
#include <vector>
#include <glm/glm.hpp>

namespace procengine {

struct BiomeSample;

class TreeGenerator {
public:
    MeshData generate(Seed seed);
    MeshData generate(Seed seed, float trunkHeight, float trunkRadius, float crownRadius);
    MeshData generate(Seed seed, const BiomeSample& biome);

private:
    void addCylinder(MeshData& mesh, float baseRadius, float topRadius, float height, int segments, const glm::vec3& offset, const glm::vec3& color, float colorVar, float materialType);
    void addCylinderBetween(MeshData& mesh, const glm::vec3& start, const glm::vec3& end, float baseRadius, float topRadius, int segments, const glm::vec3& color, float colorVar, float materialType);
    void addSphere(MeshData& mesh, float radius, int slices, int stacks, const glm::vec3& center, const glm::vec3& color, float colorVar, float materialType);
};

}
