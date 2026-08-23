#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace procengine {

struct Vertex;

class TerrainSurface {
public:
    TerrainSurface() = default;

    void build(int gridSize, float spacing, const Vertex* vertices, int vertexCount);

    float getHeight(float x, float z) const;
    glm::vec3 getNormal(float x, float z) const;

    float getHeightAtFootprint(float x, float z, float halfW, float halfD) const;

    glm::vec3 getWorldSize() const { return glm::vec3(halfSize_ * 2.0f, 0.0f, halfSize_ * 2.0f); }
    float getHalfSize() const { return halfSize_; }
    int getGridSize() const { return gridSize_; }
    float getSpacing() const { return spacing_; }

private:
    struct GridVertex {
        glm::vec3 pos;
        glm::vec3 normal;
    };

    void sampleCell(float x, float z, glm::vec3& v00, glm::vec3& v10, glm::vec3& v01, glm::vec3& v11, float& tx, float& tz) const;

    int gridSize_ = 0;
    float spacing_ = 1.0f;
    float halfSize_ = 0.0f;
    std::vector<GridVertex> grid_;
};

}