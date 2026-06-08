/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DynamicVertexBufferPool.h"
#include "logger_common.h"

#include <cstring>
#include <algorithm>

namespace AgenUIEngine {


DynamicVertexBufferPool::DynamicVertexBufferPool(VkDevice device, VkPhysicalDevice physicalDevice)
    : m_device(device)
    , m_physicalDevice(physicalDevice)
    , m_initialized(false)
{
}

DynamicVertexBufferPool::~DynamicVertexBufferPool() {
    cleanup();
}

bool DynamicVertexBufferPool::initialize(uint32_t bufferSize, uint32_t bufferCount) {
    if (m_initialized) {
        LOGW("DynamicVertexBufferPool already initialized");
        return true;
    }

    if (bufferSize < MIN_ALIGNMENT) {
        LOGE("Buffer size too small: %u (minimum: %u)", bufferSize, MIN_ALIGNMENT);
        return false;
    }

    LOGI("Initializing DynamicVertexBufferPool: bufferSize=%u, bufferCount=%u",
            bufferSize, bufferCount);

    // Allocate vertex buffers
    m_vertexBuffers.resize(bufferCount);
    for (size_t i = 0; i < m_vertexBuffers.size(); ++i) {
        m_vertexBuffers[i].totalSize = bufferSize;
        if (!createBuffer(m_vertexBuffers[i], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, bufferSize)) {
            LOGE("Failed to create vertex buffer %zu", i);
            cleanup();
            return false;
        }
        LOGI("Created vertex buffer %zu: size=%u", i, bufferSize);
    }

    // Allocate index buffers
    m_indexBuffers.resize(bufferCount);
    for (size_t i = 0; i < m_indexBuffers.size(); ++i) {
        m_indexBuffers[i].totalSize = bufferSize;
        if (!createBuffer(m_indexBuffers[i], VK_BUFFER_USAGE_INDEX_BUFFER_BIT, bufferSize)) {
            LOGE("Failed to create index buffer %zu", i);
            cleanup();
            return false;
        }
        LOGI("Created index buffer %zu: size=%u", i, bufferSize);
    }

    m_initialized = true;
    LOGI("DynamicVertexBufferPool initialized successfully");
    return true;
}

void DynamicVertexBufferPool::cleanup() {
    if (!m_initialized) {
        return;
    }

    LOGI("Cleaning up DynamicVertexBufferPool...");

    for (auto& slot : m_vertexBuffers) {
        destroyBuffer(slot);
    }
    m_vertexBuffers.clear();

    for (auto& slot : m_indexBuffers) {
        destroyBuffer(slot);
    }
    m_indexBuffers.clear();

    m_initialized = false;
    LOGI("DynamicVertexBufferPool cleaned up");
}

DynamicVertexAllocation DynamicVertexBufferPool::allocateVertexData(
    const void* data, uint32_t size, uint32_t currentFrame) {

    if (!m_initialized) {
        LOGE("DynamicVertexBufferPool not initialized");
        return DynamicVertexAllocation{};
    }

    if (!data || size == 0) {
        LOGE("Invalid vertex data: data=%p, size=%u", data, size);
        return DynamicVertexAllocation{};
    }

    // Align size to MIN_ALIGNMENT boundary
    uint32_t alignedSize = (size + MIN_ALIGNMENT - 1) & ~(MIN_ALIGNMENT - 1);

    // Find a buffer with enough space
    for (auto& slot : m_vertexBuffers) {
        if (slot.currentOffset + alignedSize <= slot.totalSize) {
            DynamicVertexAllocation alloc;
            alloc.buffer = slot.buffer;
            alloc.memory = slot.memory;
            alloc.mappedPtr = static_cast<uint8_t*>(slot.mappedPtr) + slot.currentOffset;
            alloc.offset = slot.currentOffset;
            alloc.size = size;
            alloc.frameIndex = currentFrame;

            // Copy data to the mapped memory
            memcpy(alloc.mappedPtr, data, size);

            // Advance offset
            slot.currentOffset += alignedSize;

            LOGI("Allocated vertex data: offset=%u, size=%u (aligned: %u)",
                    alloc.offset, size, alignedSize);

            return alloc;
        }
    }

    // All buffers are full
    LOGW("Vertex buffer pool exhausted! Consider increasing buffer size or count.");
    LOGW("Required: %u bytes, Available buffers: %zu", alignedSize, m_vertexBuffers.size());
    return DynamicVertexAllocation{};
}

DynamicVertexAllocation DynamicVertexBufferPool::allocateIndexData(
    const void* data, uint32_t size, uint32_t currentFrame) {

    if (!m_initialized) {
        LOGE("DynamicVertexBufferPool not initialized");
        return DynamicVertexAllocation{};
    }

    if (!data || size == 0) {
        LOGE("Invalid index data: data=%p, size=%u", data, size);
        return DynamicVertexAllocation{};
    }

    // Align size to MIN_ALIGNMENT boundary
    uint32_t alignedSize = (size + MIN_ALIGNMENT - 1) & ~(MIN_ALIGNMENT - 1);

    // Find a buffer with enough space
    for (auto& slot : m_indexBuffers) {
        if (slot.currentOffset + alignedSize <= slot.totalSize) {
            DynamicVertexAllocation alloc;
            alloc.buffer = slot.buffer;
            alloc.memory = slot.memory;
            alloc.mappedPtr = static_cast<uint8_t*>(slot.mappedPtr) + slot.currentOffset;
            alloc.offset = slot.currentOffset;
            alloc.size = size;
            alloc.frameIndex = currentFrame;

            // Copy data to the mapped memory
            memcpy(alloc.mappedPtr, data, size);

            // Advance offset
            slot.currentOffset += alignedSize;

            LOGI("Allocated index data: offset=%u, size=%u (aligned: %u)",
                    alloc.offset, size, alignedSize);

            return alloc;
        }
    }

    // All buffers are full
    LOGW("Index buffer pool exhausted! Consider increasing buffer size or count.");
    LOGW("Required: %u bytes, Available buffers: %zu", alignedSize, m_indexBuffers.size());
    return DynamicVertexAllocation{};
}

void DynamicVertexBufferPool::resetFrame(uint32_t frameIndex) {
    if (!m_initialized) {
        return;
    }

    LOGI("Resetting frame %u allocations", frameIndex);

    // Reset all buffer offsets to 0
    // Simple implementation: reset all buffers each frame
    // This is safe because we use fences to ensure frames are complete
    for (auto& slot : m_vertexBuffers) {
        slot.currentOffset = 0;
    }
    for (auto& slot : m_indexBuffers) {
        slot.currentOffset = 0;
    }
}

uint32_t DynamicVertexBufferPool::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("Failed to find suitable memory type");
    return UINT32_MAX;
}

bool DynamicVertexBufferPool::createBuffer(BufferSlot& slot, VkBufferUsageFlags usage, VkDeviceSize size) {
    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &slot.buffer) != VK_SUCCESS) {
        LOGE("Failed to create buffer");
        return false;
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, slot.buffer, &memRequirements);

    // Find suitable memory type (HOST_VISIBLE | HOST_COHERENT for dynamic updates)
    uint32_t memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (memoryTypeIndex == UINT32_MAX) {
        LOGE("Failed to find suitable memory type for buffer");
        vkDestroyBuffer(m_device, slot.buffer, nullptr);
        slot.buffer = VK_NULL_HANDLE;
        return false;
    }

    // Allocate memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &slot.memory) != VK_SUCCESS) {
        LOGE("Failed to allocate buffer memory");
        vkDestroyBuffer(m_device, slot.buffer, nullptr);
        slot.buffer = VK_NULL_HANDLE;
        return false;
    }

    // Bind memory to buffer
    vkBindBufferMemory(m_device, slot.buffer, slot.memory, 0);

    // Map memory for CPU access
    if (vkMapMemory(m_device, slot.memory, 0, size, 0, &slot.mappedPtr) != VK_SUCCESS) {
        LOGE("Failed to map buffer memory");
        vkDestroyBuffer(m_device, slot.buffer, nullptr);
        vkFreeMemory(m_device, slot.memory, nullptr);
        slot.buffer = VK_NULL_HANDLE;
        slot.memory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void DynamicVertexBufferPool::destroyBuffer(BufferSlot& slot) {
    if (slot.mappedPtr != nullptr) {
        vkUnmapMemory(m_device, slot.memory);
        slot.mappedPtr = nullptr;
    }

    if (slot.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, slot.buffer, nullptr);
        slot.buffer = VK_NULL_HANDLE;
    }

    if (slot.memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, slot.memory, nullptr);
        slot.memory = VK_NULL_HANDLE;
    }

    slot.currentOffset = 0;
    slot.totalSize = 0;
}

} // namespace AgenUIEngine
