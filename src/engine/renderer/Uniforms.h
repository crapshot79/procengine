#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace procengine {

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 lightSpace;
    glm::vec3 lightDir;
    float shadowMapSize;
    glm::vec3 lightColor;
    float fogDensity;
    glm::vec3 cameraPos;
    float pad1;
};

struct LightSpaceUbo {
    glm::mat4 lightSpace;
};

}
