#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace procengine {

struct PipelineConfig {
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineRasterizationStateCreateInfo rasterizationState;
    VkPipelineMultisampleStateCreateInfo multisampleState;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendState;
    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
    uint32_t subpass = 0;
};

class Pipeline {
public:
    Pipeline() = default;
    Pipeline(VkDevice device, const PipelineConfig& config,
             const std::string& vertSpvPath, const std::string& fragSpvPath);

    VkPipeline getHandle() const { return pipeline_; }
    VkPipelineLayout getLayout() const { return layout_; }

private:
    static std::vector<char> readFile(const std::string& path);
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);

    VkDevice device_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
};

}