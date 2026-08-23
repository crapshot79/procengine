#pragma once

#include "engine/renderer/Mesh.h"
#include "procedural/core/Seed.h"
#include <glm/glm.hpp>
#include <vector>

namespace procengine {

class HouseGenerator {
public:
    MeshData generate(Seed seed, float width, float height, float depth);

private:
    void addQuad(MeshData& mesh,
        const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
        const glm::vec3& normal, const glm::vec3& color, float materialType);

    struct Opening {
        float center;
        float width;
        float bottom;
        float top;
    };

    void addBox(MeshData& mesh, const glm::vec3& min, const glm::vec3& max,
        const glm::vec3& color, float materialType);

    void addWallZ(MeshData& mesh, float zMin, float zMax, float xMin, float xMax,
        float yMin, float yMax, const std::vector<Opening>& openings,
        const glm::vec3& color, float materialType);

    void addWallX(MeshData& mesh, float xMin, float xMax, float zMin, float zMax,
        float yMin, float yMax, const std::vector<Opening>& openings,
        const glm::vec3& color, float materialType);

    void addGableWallZ(MeshData& mesh, float zMin, float zMax, float xMin, float xMax,
        float baseY, float peakY, const glm::vec3& color, float materialType);

    void addRoofSlab(MeshData& mesh,
        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
        float thickness, const glm::vec3& topNormal, const glm::vec3& color, float materialType);
};

}
