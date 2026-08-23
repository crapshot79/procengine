#pragma once

#include "engine/renderer/Mesh.h"
#include "procedural/core/Seed.h"
#include <glm/glm.hpp>

namespace procengine {

class RockGenerator {
public:
    MeshData generate(Seed seed);
    MeshData generate(Seed seed, float radius, float scaleX, float scaleY, float scaleZ, float rotationY);

private:
    void addFace(MeshData& mesh,
        const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
        const glm::vec3& normal, const glm::vec3& color, float materialType);
};

}