#include "engine/renderer/Renderer.h"
#include "engine/renderer/Mesh.h"
#include "engine/renderer/Uniforms.h"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace procengine {

static const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = true;
#endif

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();
    return buffer;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*) {
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

Renderer::Renderer(SDL_Window* window) : window_(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDepthResources();
    createFramebuffers();
    createShadowMapResources();
    createDescriptorSetLayout();
    createPipeline();
    createShadowPipeline();
    createCommandPool();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(device_);

    vkDestroySampler(device_, shadowSampler_, nullptr);
    vkDestroyFramebuffer(device_, shadowFramebuffer_, nullptr);
    vkDestroyRenderPass(device_, shadowRenderPass_, nullptr);
    vkDestroyPipeline(device_, shadowPipeline_, nullptr);
    vkDestroyPipelineLayout(device_, shadowPipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_, shadowDescriptorSetLayout_, nullptr);
    vkDestroyDescriptorPool(device_, shadowDescriptorPool_, nullptr);
    vkDestroyBuffer(device_, shadowUniformBuffer_, nullptr);
    vkFreeMemory(device_, shadowUniformBufferMemory_, nullptr);
    vkDestroyImageView(device_, shadowImageView_, nullptr);
    vkDestroyImage(device_, shadowImage_, nullptr);
    vkFreeMemory(device_, shadowImageMemory_, nullptr);

    vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
    vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    vkDestroyRenderPass(device_, renderPass_, nullptr);

    cleanupSwapchain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device_, uniformBuffers_[i], nullptr);
        vkFreeMemory(device_, uniformBuffersMemory_[i], nullptr);
    }

    vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
        vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
        vkDestroyFence(device_, inFlightFences_[i], nullptr);
    }

    vkDestroyCommandPool(device_, commandPool_, nullptr);
    vkDestroyDevice(device_, nullptr);
    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
    }
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}

void Renderer::cleanupSwapchain() {
    vkDestroyImageView(device_, depthImageView_, nullptr);
    vkDestroyImage(device_, depthImage_, nullptr);
    vkFreeMemory(device_, depthImageMemory_, nullptr);

    for (auto fb : swapchainFramebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    for (auto iv : swapchainImageViews_) vkDestroyImageView(device_, iv, nullptr);
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
}

void Renderer::recreateSwapchain() {
    vkDeviceWaitIdle(device_);
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
}

void Renderer::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ProcEngine Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "ProcEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

bool Renderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : validationLayers) {
        bool found = false;
        for (const auto& layer : availableLayers) {
            if (strcmp(layerName, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

void Renderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    CreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_);
}

void Renderer::createSurface() {
    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        throw std::runtime_error("Failed to create window surface");
    }
}

void Renderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan GPUs found");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable GPU found");
    }
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapchainAdequate = false;
    if (extensionsSupported) {
        uint32_t formatCount, presentCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentCount, nullptr);
        swapchainAdequate = (formatCount > 0) && (presentCount > 0);
    }
    return indices.graphicsFamily != UINT32_MAX && indices.presentFamily != UINT32_MAX
        && extensionsSupported && swapchainAdequate;
}

Renderer::QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto& qf : queueFamilies) {
        if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
        if (presentSupport) indices.presentFamily = i;
        if (indices.graphicsFamily != UINT32_MAX && indices.presentFamily != UINT32_MAX) break;
        i++;
    }
    return indices;
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : availableExtensions) {
        required.erase(ext.extensionName);
    }
    return required.empty();
}

void Renderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

    float queuePriority = 1.0f;
    for (uint32_t qf : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = qf;
        qci.queueCount = 1;
        qci.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(qci);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w, h;
    SDL_GetWindowSize(window_, &w, &h);
    VkExtent2D actual = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    actual.width = std::clamp(actual.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    actual.height = std::clamp(actual.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return actual;
}

void Renderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    uint32_t presentCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    VkExtent2D extent = chooseSwapExtent(caps);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void Renderer::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); i++) {
        swapchainImageViews_[i] = createImageView(swapchainImages_[i], swapchainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void Renderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device_, &rpInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void Renderer::createDepthResources() {
    depthFormat_ = findDepthFormat();
    createImage(swapchainExtent_.width, swapchainExtent_.height, depthFormat_,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage_, depthImageMemory_);
    depthImageView_ = createImageView(depthImage_, depthFormat_, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::createShadowMapResources() {
    createImage(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, depthFormat_,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shadowImage_, shadowImageMemory_);

    shadowImageView_ = createImageView(shadowImage_, depthFormat_, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.anisotropyEnable = VK_FALSE;
    samplerCI.compareEnable = VK_TRUE;
    samplerCI.compareOp = VK_COMPARE_OP_LESS;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCI.unnormalizedCoordinates = VK_FALSE;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.mipLodBias = 0.0f;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 0.0f;

    if (vkCreateSampler(device_, &samplerCI, nullptr, &shadowSampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow sampler");
    }

    VkAttachmentDescription shadowAttachment{};
    shadowAttachment.format = depthFormat_;
    shadowAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    shadowAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    shadowAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    shadowAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    shadowAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    shadowAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadowAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference shadowDepthRef{};
    shadowDepthRef.attachment = 0;
    shadowDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription shadowSubpass{};
    shadowSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    shadowSubpass.pDepthStencilAttachment = &shadowDepthRef;

    VkSubpassDependency shadowDependency{};
    shadowDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    shadowDependency.dstSubpass = 0;
    shadowDependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    shadowDependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    shadowDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    shadowDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo shadowRPCI{};
    shadowRPCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    shadowRPCI.attachmentCount = 1;
    shadowRPCI.pAttachments = &shadowAttachment;
    shadowRPCI.subpassCount = 1;
    shadowRPCI.pSubpasses = &shadowSubpass;
    shadowRPCI.dependencyCount = 1;
    shadowRPCI.pDependencies = &shadowDependency;

    if (vkCreateRenderPass(device_, &shadowRPCI, nullptr, &shadowRenderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow render pass");
    }

    VkFramebufferCreateInfo shadowFBCI{};
    shadowFBCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    shadowFBCI.renderPass = shadowRenderPass_;
    shadowFBCI.attachmentCount = 1;
    shadowFBCI.pAttachments = &shadowImageView_;
    shadowFBCI.width = SHADOW_MAP_SIZE;
    shadowFBCI.height = SHADOW_MAP_SIZE;
    shadowFBCI.layers = 1;

    if (vkCreateFramebuffer(device_, &shadowFBCI, nullptr, &shadowFramebuffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow framebuffer");
    }

    VkDescriptorSetLayoutBinding lightSpaceBinding{};
    lightSpaceBinding.binding = 0;
    lightSpaceBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightSpaceBinding.descriptorCount = 1;
    lightSpaceBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    lightSpaceBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo shadowLayoutCI{};
    shadowLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    shadowLayoutCI.bindingCount = 1;
    shadowLayoutCI.pBindings = &lightSpaceBinding;

    if (vkCreateDescriptorSetLayout(device_, &shadowLayoutCI, nullptr, &shadowDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow descriptor set layout");
    }

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo shadowPipelineLayoutCI{};
    shadowPipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    shadowPipelineLayoutCI.setLayoutCount = 1;
    shadowPipelineLayoutCI.pSetLayouts = &shadowDescriptorSetLayout_;
    shadowPipelineLayoutCI.pushConstantRangeCount = 1;
    shadowPipelineLayoutCI.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device_, &shadowPipelineLayoutCI, nullptr, &shadowPipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline layout");
    }

    VkBufferCreateInfo shadowBufCI{};
    shadowBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    shadowBufCI.size = sizeof(LightSpaceUbo);
    shadowBufCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    shadowBufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device_, &shadowBufCI, nullptr, &shadowUniformBuffer_);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, shadowUniformBuffer_, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &shadowUniformBufferMemory_);
    vkBindBufferMemory(device_, shadowUniformBuffer_, shadowUniformBufferMemory_, 0);
    vkMapMemory(device_, shadowUniformBufferMemory_, 0, sizeof(LightSpaceUbo), 0, &shadowUniformBufferMapped_);

    VkDescriptorPoolSize shadowPoolSize{};
    shadowPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadowPoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo shadowPoolCI{};
    shadowPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    shadowPoolCI.poolSizeCount = 1;
    shadowPoolCI.pPoolSizes = &shadowPoolSize;
    shadowPoolCI.maxSets = 1;

    if (vkCreateDescriptorPool(device_, &shadowPoolCI, nullptr, &shadowDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow descriptor pool");
    }

    VkDescriptorSetLayout shadowLayouts[] = { shadowDescriptorSetLayout_ };
    VkDescriptorSetAllocateInfo shadowAllocInfo{};
    shadowAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    shadowAllocInfo.descriptorPool = shadowDescriptorPool_;
    shadowAllocInfo.descriptorSetCount = 1;
    shadowAllocInfo.pSetLayouts = shadowLayouts;

    if (vkAllocateDescriptorSets(device_, &shadowAllocInfo, &shadowDescriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate shadow descriptor set");
    }

    VkDescriptorBufferInfo shadowBufInfo{};
    shadowBufInfo.buffer = shadowUniformBuffer_;
    shadowBufInfo.offset = 0;
    shadowBufInfo.range = sizeof(LightSpaceUbo);

    VkWriteDescriptorSet shadowWrite{};
    shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadowWrite.dstSet = shadowDescriptorSet_;
    shadowWrite.dstBinding = 0;
    shadowWrite.dstArrayElement = 0;
    shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadowWrite.descriptorCount = 1;
    shadowWrite.pBufferInfo = &shadowBufInfo;

    vkUpdateDescriptorSets(device_, 1, &shadowWrite, 0, nullptr);
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
    }
    throw std::runtime_error("Failed to find supported format");
}

VkFormat Renderer::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void Renderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.extent.width = width;
    imageCI.extent.height = height;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.format = format;
    imageCI.tiling = tiling;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.usage = usage;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &imageCI, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(device_, image, imageMemory, 0);
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = format;
    ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.subresourceRange.aspectMask = aspectFlags;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device_, &ci, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }
    return imageView;
}

void Renderer::createFramebuffers() {
    swapchainFramebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); i++) {
        std::array<VkImageView, 2> attachments = { swapchainImageViews_[i], depthImageView_ };
        VkFramebufferCreateInfo fbCI{};
        fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCI.renderPass = renderPass_;
        fbCI.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbCI.pAttachments = attachments.data();
        fbCI.width = swapchainExtent_.width;
        fbCI.height = swapchainExtent_.height;
        fbCI.layers = 1;

        if (vkCreateFramebuffer(device_, &fbCI, nullptr, &swapchainFramebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void Renderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding shadowBinding{};
    shadowBinding.binding = 1;
    shadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowBinding.descriptorCount = 1;
    shadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    shadowBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboBinding, shadowBinding };

    VkDescriptorSetLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutCI.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layoutCI, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void Renderer::createPipeline() {
    auto vertCode = readFile("shaders/scene.vert.spv");
    auto fragCode = readFile("shaders/scene.frag.spv");

    VkShaderModuleCreateInfo vertModCI{};
    vertModCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModCI.codeSize = vertCode.size();
    vertModCI.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    VkShaderModule vertModule;
    vkCreateShaderModule(device_, &vertModCI, nullptr, &vertModule);

    VkShaderModuleCreateInfo fragModCI{};
    fragModCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModCI.codeSize = fragCode.size();
    fragModCI.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    VkShaderModule fragModule;
    vkCreateShaderModule(device_, &fragModCI, nullptr, &fragModule);

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

    auto bindingDesc = Vertex::getBindingDescription();
    auto attributeDescs = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputCI{};
    vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCI.vertexBindingDescriptionCount = 1;
    vertexInputCI.pVertexBindingDescriptions = &bindingDesc;
    vertexInputCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
    vertexInputCI.pVertexAttributeDescriptions = attributeDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicStateCI{};
    dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCI.pDynamicStates = dynamicStates.data();

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts = &descriptorSetLayout_;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device_, &layoutCI, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInputCI;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &rasterizer;
    pipelineCI.pMultisampleState = &multisampling;
    pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.pColorBlendState = &colorBlending;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.layout = pipelineLayout_;
    pipelineCI.renderPass = renderPass_;
    pipelineCI.subpass = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

void Renderer::createShadowPipeline() {
    auto vertCode = readFile("shaders/shadow.vert.spv");
    auto fragCode = readFile("shaders/shadow.frag.spv");

    VkShaderModuleCreateInfo vertModCI{};
    vertModCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModCI.codeSize = vertCode.size();
    vertModCI.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    VkShaderModule vertModule;
    vkCreateShaderModule(device_, &vertModCI, nullptr, &vertModule);

    VkShaderModuleCreateInfo fragModCI{};
    fragModCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModCI.codeSize = fragCode.size();
    fragModCI.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    VkShaderModule fragModule;
    vkCreateShaderModule(device_, &fragModCI, nullptr, &fragModule);

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

    auto bindingDesc = Vertex::getBindingDescription();
    auto attributeDescs = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputCI{};
    vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCI.vertexBindingDescriptionCount = 1;
    vertexInputCI.pVertexBindingDescriptions = &bindingDesc;
    vertexInputCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
    vertexInputCI.pVertexAttributeDescriptions = attributeDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport shadowViewport{};
    shadowViewport.x = 0.0f;
    shadowViewport.y = 0.0f;
    shadowViewport.width = static_cast<float>(SHADOW_MAP_SIZE);
    shadowViewport.height = static_cast<float>(SHADOW_MAP_SIZE);
    shadowViewport.minDepth = 0.0f;
    shadowViewport.maxDepth = 1.0f;

    VkRect2D shadowScissor{};
    shadowScissor.offset = {0, 0};
    shadowScissor.extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &shadowViewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &shadowScissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 4.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 1.5f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = 0;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInputCI;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &rasterizer;
    pipelineCI.pMultisampleState = &multisampling;
    pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.pColorBlendState = &colorBlending;
    pipelineCI.layout = shadowPipelineLayout_;
    pipelineCI.renderPass = shadowRenderPass_;
    pipelineCI.subpass = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &shadowPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline");
    }

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

void Renderer::createCommandPool() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    VkCommandPoolCreateInfo poolCI{};
    poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCI.queueFamilyIndex = indices.graphicsFamily;

    if (vkCreateCommandPool(device_, &poolCI, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void Renderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    uniformBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped_.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferCI{};
        bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size = bufferSize;
        bufferCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device_, &bufferCI, nullptr, &uniformBuffers_[i]);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device_, uniformBuffers_[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(device_, &allocInfo, nullptr, &uniformBuffersMemory_[i]);
        vkBindBufferMemory(device_, uniformBuffers_[i], uniformBuffersMemory_[i], 0);
        vkMapMemory(device_, uniformBuffersMemory_[i], 0, bufferSize, 0, &uniformBuffersMapped_[i]);
    }
}

void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCI.pPoolSizes = poolSizes.data();
    poolCI.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device_, &poolCI, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void Renderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo shadowImageInfo{};
        shadowImageInfo.sampler = shadowSampler_;
        shadowImageInfo.imageView = shadowImageView_;
        shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptorWrites[2]{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets_[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets_[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &shadowImageInfo;

        vkUpdateDescriptorSets(device_, 2, descriptorWrites, 0, nullptr);
    }
}

void Renderer::createCommandBuffers() {
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semCI{};
    semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(device_, &semCI, nullptr, &imageAvailableSemaphores_[i]);
        vkCreateSemaphore(device_, &semCI, nullptr, &renderFinishedSemaphores_[i]);
        vkCreateFence(device_, &fenceCI, nullptr, &inFlightFences_[i]);
    }
}

void Renderer::beginShadowPass(const glm::mat4& lightSpaceMatrix) {
    LightSpaceUbo lsUbo{};
    lsUbo.lightSpace = lightSpaceMatrix;
    memcpy(shadowUniformBufferMapped_, &lsUbo, sizeof(lsUbo));

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo shadowRPBegin{};
    shadowRPBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    shadowRPBegin.renderPass = shadowRenderPass_;
    shadowRPBegin.framebuffer = shadowFramebuffer_;
    shadowRPBegin.renderArea.offset = {0, 0};
    shadowRPBegin.renderArea.extent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
    shadowRPBegin.clearValueCount = 1;
    shadowRPBegin.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffers_[currentFrame_], &shadowRPBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);

    vkCmdBindDescriptorSets(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS,
        shadowPipelineLayout_, 0, 1, &shadowDescriptorSet_, 0, nullptr);
}

void Renderer::shadowDrawMesh(const GpuMesh& mesh, const glm::mat4& modelMatrix) {
    if (!frameStarted_) return;

    vkCmdPushConstants(commandBuffers_[currentFrame_], shadowPipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMatrix);

    VkBuffer vertexBuffers[] = { mesh.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffers_[currentFrame_], 0, 1, vertexBuffers, offsets);

    if (mesh.indexBuffer != VK_NULL_HANDLE && mesh.indexCount > 0) {
        vkCmdBindIndexBuffer(commandBuffers_[currentFrame_], mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffers_[currentFrame_], mesh.indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffers_[currentFrame_], mesh.vertexCount, 1, 0, 0);
    }
}

void Renderer::beginScenePass() {
    if (!frameStarted_) return;

    vkCmdEndRenderPass(commandBuffers_[currentFrame_]);

    VkImageMemoryBarrier shadowBarrier{};
    shadowBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    shadowBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    shadowBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    shadowBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadowBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadowBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowBarrier.image = shadowImage_;
    shadowBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    shadowBarrier.subresourceRange.baseMipLevel = 0;
    shadowBarrier.subresourceRange.levelCount = 1;
    shadowBarrier.subresourceRange.baseArrayLayer = 0;
    shadowBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffers_[currentFrame_],
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &shadowBarrier);

    VkClearValue clearColors[2];
    clearColors[0].color = {{0.58f, 0.76f, 0.88f, 1.0f}};
    clearColors[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = renderPass_;
    rpBegin.framebuffer = swapchainFramebuffers_[currentImageIndex_];
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = swapchainExtent_;
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = clearColors;

    vkCmdBeginRenderPass(commandBuffers_[currentFrame_], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    vkCmdBindDescriptorSets(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers_[currentFrame_], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent_;
    vkCmdSetScissor(commandBuffers_[currentFrame_], 0, 1, &scissor);
}

void Renderer::drawMesh(const GpuMesh& mesh, const glm::mat4& modelMatrix) {
    if (!frameStarted_) return;

    vkCmdPushConstants(commandBuffers_[currentFrame_], pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMatrix);

    VkBuffer vertexBuffers[] = { mesh.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffers_[currentFrame_], 0, 1, vertexBuffers, offsets);

    if (mesh.indexBuffer != VK_NULL_HANDLE && mesh.indexCount > 0) {
        vkCmdBindIndexBuffer(commandBuffers_[currentFrame_], mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffers_[currentFrame_], mesh.indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffers_[currentFrame_], mesh.vertexCount, 1, 0, 0);
    }
}

void Renderer::beginFrame() {
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &currentImageIndex_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffers_[currentFrame_], &beginInfo);

    frameStarted_ = true;
}

void Renderer::endFrame() {
    if (!frameStarted_) return;
    frameStarted_ = false;

    vkCmdEndRenderPass(commandBuffers_[currentFrame_]);

    if (vkEndCommandBuffer(commandBuffers_[currentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores_[currentFrame_] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores_[currentFrame_] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapchains[] = { swapchain_ };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &currentImageIndex_;

    VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::updateUniformBuffer(const UniformBufferObject& ubo) {
    memcpy(uniformBuffersMapped_[currentFrame_], &ubo, sizeof(ubo));
}

void Renderer::waitIdle() {
    vkDeviceWaitIdle(device_);
}

GpuMesh Renderer::uploadMesh(const MeshData& data) {
    GpuMesh gpu{};
    gpu.vertexCount = static_cast<uint32_t>(data.vertices.size());
    gpu.indexCount = static_cast<uint32_t>(data.indices.size());

    VkDeviceSize vertexBufSize = sizeof(Vertex) * data.vertices.size();

    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;

    VkBufferCreateInfo vbc{};
    vbc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbc.size = vertexBufSize;
    vbc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vbc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device_, &vbc, nullptr, &stagingBuf);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, stagingBuf, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &stagingMem);
    vkBindBufferMemory(device_, stagingBuf, stagingMem, 0);

    void* mapped;
    vkMapMemory(device_, stagingMem, 0, vertexBufSize, 0, &mapped);
    memcpy(mapped, data.vertices.data(), static_cast<size_t>(vertexBufSize));
    vkUnmapMemory(device_, stagingMem);

    VkBufferCreateInfo vbcGpu{};
    vbcGpu.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbcGpu.size = vertexBufSize;
    vbcGpu.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbcGpu.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device_, &vbcGpu, nullptr, &gpu.vertexBuffer);

    vkGetBufferMemoryRequirements(device_, gpu.vertexBuffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &allocInfo, nullptr, &gpu.vertexBufferMemory);
    vkBindBufferMemory(device_, gpu.vertexBuffer, gpu.vertexBufferMemory, 0);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandPool = commandPool_;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer cmdBuf;
    vkAllocateCommandBuffers(device_, &cmdAlloc, &cmdBuf);

    VkCommandBufferBeginInfo cmdBegin{};
    cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &cmdBegin);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = vertexBufSize;
    vkCmdCopyBuffer(cmdBuf, stagingBuf, gpu.vertexBuffer, 1, &copyRegion);
    vkEndCommandBuffer(cmdBuf);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmdBuf;
    vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &cmdBuf);
    vkDestroyBuffer(device_, stagingBuf, nullptr);
    vkFreeMemory(device_, stagingMem, nullptr);

    if (!data.indices.empty()) {
        VkDeviceSize indexBufSize = sizeof(uint32_t) * data.indices.size();

        VkBuffer stagingIBuf;
        VkDeviceMemory stagingIMem;

        VkBufferCreateInfo ibc{};
        ibc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ibc.size = indexBufSize;
        ibc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        ibc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &ibc, nullptr, &stagingIBuf);

        vkGetBufferMemoryRequirements(device_, stagingIBuf, &memReqs);
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &allocInfo, nullptr, &stagingIMem);
        vkBindBufferMemory(device_, stagingIBuf, stagingIMem, 0);

        vkMapMemory(device_, stagingIMem, 0, indexBufSize, 0, &mapped);
        memcpy(mapped, data.indices.data(), static_cast<size_t>(indexBufSize));
        vkUnmapMemory(device_, stagingIMem);

        VkBufferCreateInfo ibcGpu{};
        ibcGpu.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ibcGpu.size = indexBufSize;
        ibcGpu.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        ibcGpu.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &ibcGpu, nullptr, &gpu.indexBuffer);

        vkGetBufferMemoryRequirements(device_, gpu.indexBuffer, &memReqs);
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device_, &allocInfo, nullptr, &gpu.indexBufferMemory);
        vkBindBufferMemory(device_, gpu.indexBuffer, gpu.indexBufferMemory, 0);

        vkAllocateCommandBuffers(device_, &cmdAlloc, &cmdBuf);
        vkBeginCommandBuffer(cmdBuf, &cmdBegin);

        VkBufferCopy indexCopy{};
        indexCopy.srcOffset = 0;
        indexCopy.dstOffset = 0;
        indexCopy.size = indexBufSize;
        vkCmdCopyBuffer(cmdBuf, stagingIBuf, gpu.indexBuffer, 1, &indexCopy);
        vkEndCommandBuffer(cmdBuf);

        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmdBuf;
        vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(device_, commandPool_, 1, &cmdBuf);
        vkDestroyBuffer(device_, stagingIBuf, nullptr);
        vkFreeMemory(device_, stagingIMem, nullptr);
    }

    return gpu;
}

void Renderer::destroyMesh(GpuMesh& mesh) {
    if (mesh.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, mesh.vertexBuffer, nullptr);
        vkFreeMemory(device_, mesh.vertexBufferMemory, nullptr);
        mesh.vertexBuffer = VK_NULL_HANDLE;
    }
    if (mesh.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, mesh.indexBuffer, nullptr);
        vkFreeMemory(device_, mesh.indexBufferMemory, nullptr);
        mesh.indexBuffer = VK_NULL_HANDLE;
    }
}

}
