#include "engine/renderer/Pipeline.h"
#include <fstream>
#include <stdexcept>

namespace procengine {

std::vector<char> Pipeline::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule Pipeline::createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return module;
}

Pipeline::Pipeline(VkDevice device, const PipelineConfig& config,
                   const std::string& vertSpvPath, const std::string& fragSpvPath)
    : device_(device) {
    auto vertCode = readFile(vertSpvPath);
    auto fragCode = readFile(fragSpvPath);
    VkShaderModule vertModule = createShaderModule(device_, vertCode);
    VkShaderModule fragModule = createShaderModule(device_, fragCode);

    VkPipelineShaderStageCreateInfo stages[2];
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[0].pNext = nullptr;
    stages[0].flags = 0;
    stages[0].pSpecializationInfo = nullptr;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";
    stages[1].pNext = nullptr;
    stages[1].flags = 0;
    stages[1].pSpecializationInfo = nullptr;

    layout_ = config.pipelineLayout;

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pViewportState = &config.viewportState;
    pipelineCI.pRasterizationState = &config.rasterizationState;
    pipelineCI.pMultisampleState = &config.multisampleState;
    pipelineCI.pColorBlendState = &config.colorBlendState;
    pipelineCI.pDepthStencilState = &config.depthStencilState;
    pipelineCI.layout = layout_;
    pipelineCI.renderPass = config.renderPass;
    pipelineCI.subpass = config.subpass;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

}