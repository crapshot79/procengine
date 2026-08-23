#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <limits>

namespace procengine {

constexpr float MATERIAL_TERRAIN = 0.0f;
constexpr float MATERIAL_BARK = 1.0f;
constexpr float MATERIAL_LEAVES = 2.0f;
constexpr float MATERIAL_WALL = 3.0f;
constexpr float MATERIAL_ROOF = 4.0f;
constexpr float MATERIAL_DOOR = 5.0f;
constexpr float MATERIAL_WINDOW = 6.0f;
constexpr float MATERIAL_CHIMNEY = 7.0f;
constexpr float MATERIAL_FLOOR = 8.0f;
constexpr float MATERIAL_ROCK = 9.0f;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
    float materialType;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription desc{};
        desc.binding = 0;
        desc.stride = sizeof(Vertex);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> descs(4);
        descs[0].binding = 0;
        descs[0].location = 0;
        descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        descs[0].offset = offsetof(Vertex, pos);

        descs[1].binding = 0;
        descs[1].location = 1;
        descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        descs[1].offset = offsetof(Vertex, normal);

        descs[2].binding = 0;
        descs[2].location = 2;
        descs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        descs[2].offset = offsetof(Vertex, color);

        descs[3].binding = 0;
        descs[3].location = 3;
        descs[3].format = VK_FORMAT_R32_SFLOAT;
        descs[3].offset = offsetof(Vertex, materialType);
        return descs;
    }
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

inline float computeMeshMinY(const MeshData& mesh) {
    float minY = std::numeric_limits<float>::max();
    for (const auto& v : mesh.vertices) {
        if (v.pos.y < minY) minY = v.pos.y;
    }
    return minY;
}

inline float computeMeshMaxY(const MeshData& mesh) {
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& v : mesh.vertices) {
        if (v.pos.y > maxY) maxY = v.pos.y;
    }
    return maxY;
}

struct GpuMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t vertexCount = 0;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
};

}