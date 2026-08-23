#include "procedural/world/TerrainSurface.h"
#include "engine/renderer/Mesh.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace procengine {

void TerrainSurface::build(int gridSize, float spacing, const Vertex* vertices, int vertexCount) {
    gridSize_ = gridSize;
    spacing_ = spacing;
    halfSize_ = gridSize * spacing * 0.5f;

    int stride = gridSize + 1;
    grid_.resize(vertexCount);
    for (int i = 0; i < vertexCount; i++) {
        grid_[i].pos = vertices[i].pos;
        grid_[i].normal = vertices[i].normal;
    }
}

void TerrainSurface::sampleCell(float x, float z, glm::vec3& v00, glm::vec3& v10, glm::vec3& v01, glm::vec3& v11, float& tx, float& tz) const {
    float u = (x + halfSize_) / spacing_;
    float v = (z + halfSize_) / spacing_;

    int ix = static_cast<int>(std::floor(u));
    int iz = static_cast<int>(std::floor(v));

    ix = std::clamp(ix, 0, gridSize_ - 1);
    iz = std::clamp(iz, 0, gridSize_ - 1);

    int stride = gridSize_ + 1;

    v00 = grid_[iz * stride + ix].pos;
    v10 = grid_[iz * stride + ix + 1].pos;
    v01 = grid_[(iz + 1) * stride + ix].pos;
    v11 = grid_[(iz + 1) * stride + ix + 1].pos;

    tx = std::clamp(u - ix, 0.0f, 1.0f);
    tz = std::clamp(v - iz, 0.0f, 1.0f);
}

float TerrainSurface::getHeight(float x, float z) const {
    if (grid_.empty()) return 0.0f;

    float u = (x + halfSize_) / spacing_;
    float v = (z + halfSize_) / spacing_;

    if (u < 0.0f || u >= gridSize_ || v < 0.0f || v >= gridSize_) {
        float cx = std::clamp(u, 0.0f, static_cast<float>(gridSize_));
        float cz = std::clamp(v, 0.0f, static_cast<float>(gridSize_));
        int gx = static_cast<int>(cx);
        int gz = static_cast<int>(cz);
        gx = std::clamp(gx, 0, gridSize_);
        gz = std::clamp(gz, 0, gridSize_);
        int stride = gridSize_ + 1;
        return grid_[gz * stride + gx].pos.y;
    }

    glm::vec3 v00, v10, v01, v11;
    float tx, tz;
    sampleCell(x, z, v00, v10, v01, v11, tx, tz);

    float h00 = v00.y;
    float h10 = v10.y;
    float h01 = v01.y;
    float h11 = v11.y;

    if (tx + tz < 1.0f) {
        return h00 * (1.0f - tx - tz) + h01 * tz + h10 * tx;
    } else {
        return h11 * (tx + tz - 1.0f) + h01 * (1.0f - tx) + h10 * (1.0f - tz);
    }
}

glm::vec3 TerrainSurface::getNormal(float x, float z) const {
    if (grid_.empty()) return glm::vec3(0.0f, 1.0f, 0.0f);

    float u = (x + halfSize_) / spacing_;
    float v = (z + halfSize_) / spacing_;

    if (u < 0.0f || u >= gridSize_ || v < 0.0f || v >= gridSize_) {
        int gx = std::clamp(static_cast<int>(u), 0, gridSize_);
        int gz = std::clamp(static_cast<int>(v), 0, gridSize_);
        int stride = gridSize_ + 1;
        return grid_[gz * stride + gx].normal;
    }

    int ix = static_cast<int>(std::floor(u));
    int iz = static_cast<int>(std::floor(v));
    ix = std::clamp(ix, 0, gridSize_ - 1);
    iz = std::clamp(iz, 0, gridSize_ - 1);
    int stride = gridSize_ + 1;

    float tx = std::clamp(u - ix, 0.0f, 1.0f);
    float tz = std::clamp(v - iz, 0.0f, 1.0f);

    glm::vec3 n00 = grid_[iz * stride + ix].normal;
    glm::vec3 n10 = grid_[iz * stride + ix + 1].normal;
    glm::vec3 n01 = grid_[(iz + 1) * stride + ix].normal;
    glm::vec3 n11 = grid_[(iz + 1) * stride + ix + 1].normal;

    glm::vec3 result;
    if (tx + tz < 1.0f) {
        result = n00 * (1.0f - tx - tz) + n01 * tz + n10 * tx;
    } else {
        result = n11 * (tx + tz - 1.0f) + n01 * (1.0f - tx) + n10 * (1.0f - tz);
    }

    float len = glm::length(result);
    return len > 0.0f ? result / len : glm::vec3(0.0f, 1.0f, 0.0f);
}

float TerrainSurface::getHeightAtFootprint(float x, float z, float halfW, float halfD) const {
    if (halfW <= 0.0f && halfD <= 0.0f) return getHeight(x, z);

    float step = spacing_ * 0.5f;
    float minY = std::numeric_limits<float>::max();

    for (float dx = -halfW; dx <= halfW + 0.001f; dx += step) {
        for (float dz = -halfD; dz <= halfD + 0.001f; dz += step) {
            float h = getHeight(x + dx, z + dz);
            if (h < minY) minY = h;
        }
    }

    return minY;
}

}