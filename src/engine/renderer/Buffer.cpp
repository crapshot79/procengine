#include "engine/renderer/Buffer.h"
#include <stdexcept>

namespace procengine {

Buffer::Buffer(VkDevice device, VkPhysicalDevice physicalDevice,
               VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties)
    : device_(device), physicalDevice_(physicalDevice), size_(size) {
    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = size;
    bufferCI.usage = usage;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferCI, nullptr, &buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buffer_, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device_, buffer_, memory_, 0);
}

Buffer::~Buffer() {
    if (buffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, buffer_, nullptr);
    if (memory_ != VK_NULL_HANDLE) vkFreeMemory(device_, memory_, nullptr);
}

void Buffer::upload(const void* data, VkDeviceSize dataSize) {
    void* mapped;
    vkMapMemory(device_, memory_, 0, dataSize, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(dataSize));
    vkUnmapMemory(device_, memory_);
}

uint32_t Buffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

}