#pragma once

#include "engine/renderer/Mesh.h"
#include "engine/renderer/Uniforms.h"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <array>

namespace procengine {

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
static constexpr int SHADOW_MAP_SIZE = 2048;

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void beginShadowPass(const glm::mat4& lightSpaceMatrix);
    void shadowDrawMesh(const GpuMesh& mesh, const glm::mat4& modelMatrix);
    void beginScenePass();
    void drawMesh(const GpuMesh& mesh, const glm::mat4& modelMatrix);
    void endFrame();

    void beginFrame();

    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    uint32_t getWidth() const { return swapchainExtent_.width; }
    uint32_t getHeight() const { return swapchainExtent_.height; }

    GpuMesh uploadMesh(const MeshData& data);
    void destroyMesh(GpuMesh& mesh);

    void updateUniformBuffer(const UniformBufferObject& ubo);
    void waitIdle();

private:
    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSurface();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();
    void createShadowMapResources();
    void createDescriptorSetLayout();
    void createPipeline();
    void createShadowPipeline();
    void createCommandPool();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();
    void recreateSwapchain();
    void cleanupSwapchain();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    bool frameStarted_ = false;

    SDL_Window* window_;

    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_;

    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBuffersMemory_;
    std::vector<void*> uniformBuffersMapped_;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets_;

    VkImage shadowImage_ = VK_NULL_HANDLE;
    VkDeviceMemory shadowImageMemory_ = VK_NULL_HANDLE;
    VkImageView shadowImageView_ = VK_NULL_HANDLE;
    VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout shadowPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout shadowDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkBuffer shadowUniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory shadowUniformBufferMemory_ = VK_NULL_HANDLE;
    void* shadowUniformBufferMapped_ = nullptr;
    VkDescriptorPool shadowDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet shadowDescriptorSet_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;

    struct QueueFamilyIndices {
        uint32_t graphicsFamily = UINT32_MAX;
        uint32_t presentFamily = UINT32_MAX;
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkValidationLayerSupport();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps);

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT*,
        void*);
};

}
