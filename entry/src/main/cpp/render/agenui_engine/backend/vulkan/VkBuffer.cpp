/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Vulkan Buffer Implementation
 */

#include "VkBuffer.h"
#include "logger_common.h"
#include <cstring>  // for memcpy

namespace AgenUIEngine {

VkBuffer::VkBuffer() = default;

VkBuffer::~VkBuffer() {
    // Note: cleanup() must be called explicitly before destruction
    // This follows the same pattern as the original Texture class
}

// -------------------------------------------------------------------------
// IBuffer interface implementation
// -------------------------------------------------------------------------

void* VkBuffer::map() {
    return m_mappedData;
}

void VkBuffer::unmap() {
    // For persistently mapped uniform buffers, this is a no-op
    // The buffer stays mapped for its lifetime
}

void VkBuffer::update(const void* data, size_t size, size_t offset) {
    updateData(data, static_cast<VkDeviceSize>(size), static_cast<VkDeviceSize>(offset));
}

size_t VkBuffer::getSize() const {
    return static_cast<size_t>(m_size);
}

Core::BufferUsage VkBuffer::getUsage() const {
    return m_usage;
}

bool VkBuffer::isPersistentlyMapped() const {
    return m_mappedData != nullptr;
}

uint64_t VkBuffer::getNativeHandle() const {
    return reinterpret_cast<uint64_t>(m_buffer);
}

// -------------------------------------------------------------------------
// Vulkan-specific creation methods
// -------------------------------------------------------------------------

bool VkBuffer::createUniform(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size) {
    m_usage = Core::BufferUsage::UniformBuffer;

    // Uniform buffers need HOST_VISIBLE and HOST_COHERENT for efficient CPU writes
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!createBufferInternal(device, physicalDevice, size,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, properties)) {
        LOGE("VkBuffer: Failed to create uniform buffer");
        return false;
    }

    // Persistently map the buffer for efficient updates
    if (vkMapMemory(device, m_memory, 0, size, 0, &m_mappedData) != VK_SUCCESS) {
        LOGE("VkBuffer: Failed to map uniform buffer memory");
        cleanup(device);
        return false;
    }

    m_size = size;
    return true;
}

bool VkBuffer::createVertex(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue graphicsQueue,
                         const void* data, VkDeviceSize size) {
    m_usage = Core::BufferUsage::VertexBuffer;

    // Create staging buffer (host visible, host coherent)
    VkBuffer stagingBuffer;
    if (!stagingBuffer.createStaging(device, physicalDevice, size)) {
        LOGE("VkBuffer: Failed to create staging buffer for vertex data");
        return false;
    }

    // Copy data to staging buffer
    stagingBuffer.updateData(data, size);

    // Create vertex buffer (device local for optimal GPU access)
    VkMemoryPropertyFlags deviceLocalProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (!createBufferInternal(device, physicalDevice, size,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     deviceLocalProperties)) {
        LOGE("VkBuffer: Failed to create vertex buffer");
        stagingBuffer.cleanup(device);
        return false;
    }

    // Copy from staging buffer to vertex buffer
    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer.getBuffer(), m_buffer, size);

    // Cleanup staging buffer
    stagingBuffer.cleanup(device);

    m_size = size;
    return true;
}

bool VkBuffer::createIndex(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool, VkQueue graphicsQueue,
                        const void* data, VkDeviceSize size) {
    m_usage = Core::BufferUsage::IndexBuffer;

    // Create staging buffer (host visible, host coherent)
    VkBuffer stagingBuffer;
    if (!stagingBuffer.createStaging(device, physicalDevice, size)) {
        LOGE("VkBuffer: Failed to create staging buffer for index data");
        return false;
    }

    // Copy data to staging buffer
    stagingBuffer.updateData(data, size);

    // Create index buffer (device local for optimal GPU access)
    VkMemoryPropertyFlags deviceLocalProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (!createBufferInternal(device, physicalDevice, size,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     deviceLocalProperties)) {
        LOGE("VkBuffer: Failed to create index buffer");
        stagingBuffer.cleanup(device);
        return false;
    }

    // Copy from staging buffer to index buffer
    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer.getBuffer(), m_buffer, size);

    // Cleanup staging buffer
    stagingBuffer.cleanup(device);

    m_size = size;
    return true;
}

bool VkBuffer::createStaging(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size) {
    m_usage = Core::BufferUsage::StagingBuffer;

    // Staging buffers need HOST_VISIBLE and HOST_COHERENT for CPU uploads
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Note: These buffers may be used directly as vertex/index buffers (without staging copy)
    // So we include both TRANSFER_SRC and VERTEX/INDEX buffer bits
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (!createBufferInternal(device, physicalDevice, size, usage, properties)) {
        LOGE("VkBuffer: Failed to create staging buffer");
        return false;
    }

    // Map staging buffer for data upload
    if (vkMapMemory(device, m_memory, 0, size, 0, &m_mappedData) != VK_SUCCESS) {
        LOGE("VkBuffer: Failed to map staging buffer memory");
        cleanup(device);
        return false;
    }

    m_size = size;
    return true;
}

void VkBuffer::updateData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!m_mappedData) {
        LOGE("VkBuffer: Cannot update data - buffer is not mapped");
        return;
    }

    if (offset + size > m_size) {
        LOGE("VkBuffer: Update data out of bounds (offset=%llu, size=%llu, bufferSize=%llu)",
             offset, size, m_size);
        return;
    }

    // Copy data to mapped memory
    memcpy(static_cast<uint8_t*>(m_mappedData) + offset, data, static_cast<size_t>(size));
}

void VkBuffer::cleanup(VkDevice device) {
    if (m_mappedData) {
        vkUnmapMemory(device, m_memory);
        m_mappedData = nullptr;
    }

    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }

    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }

    m_size = 0;
}

// -------------------------------------------------------------------------
// Private helpers
// -------------------------------------------------------------------------

uint32_t VkBuffer::findMemoryType(VkPhysicalDevice physicalDevice,
                               uint32_t typeFilter,
                               VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("VkBuffer: Failed to find suitable memory type");
    return UINT32_MAX;
}

bool VkBuffer::createBufferInternal(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        LOGE("VkBuffer: Failed to create buffer");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice,
                                              memRequirements.memoryTypeBits,
                                              properties);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        LOGE("VkBuffer: Failed to find memory type for buffer");
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        LOGE("VkBuffer: Failed to allocate buffer memory");
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device, m_buffer, m_memory, 0);
    return true;
}

void VkBuffer::copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                       ::VkBuffer srcBuffer, ::VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

} // namespace AgenUIEngine
