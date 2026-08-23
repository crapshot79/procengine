#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace procengine {

class Buffer {
public:
    Buffer() = default;
    Buffer(VkDevice device, VkPhysicalDevice physicalDevice,
           VkDeviceSize size, VkBufferUsageFlags usage,
           VkMemoryPropertyFlags properties);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    VkBuffer getHandle() const { return buffer_; }
    VkDeviceSize getSize() const { return size_; }

    void upload(const void* data, VkDeviceSize dataSize);

private:
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
};

}