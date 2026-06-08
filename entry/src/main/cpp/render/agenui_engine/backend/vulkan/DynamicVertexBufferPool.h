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

#ifndef AGENUI_ENGINE_DYNAMIC_VERTEX_BUFFER_POOL_H
#define AGENUI_ENGINE_DYNAMIC_VERTEX_BUFFER_POOL_H

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace AgenUIEngine {

/**
 * @brief Represents a vertex/index buffer allocation from the pool
 *
 * This structure holds information about an allocated region within
 * a pooled buffer, including the buffer handle, memory handle, mapped pointer,
 * offset, size, and frame index for tracking.
 */
struct DynamicVertexAllocation {
    ::VkBuffer buffer = VK_NULL_HANDLE;      // The Vulkan buffer handle
    VkDeviceMemory memory = VK_NULL_HANDLE; // The Vulkan device memory handle
    void* mappedPtr = nullptr;             // Pointer to mapped memory (if applicable)
    uint32_t offset = 0;                   // Offset into the buffer in bytes
    uint32_t size = 0;                     // Size of the allocation in bytes
    uint32_t frameIndex = 0;               // Frame index for tracking
};

/**
 * @brief Dynamic vertex and index buffer pool for Vulkan
 *
 * This class manages a pool of vertex and index buffers that can be
 * dynamically allocated and reused across frames. It eliminates the
 * overhead of creating and destroying buffers every frame.
 *
 * Features:
 * - Pre-allocated buffer pool for reduced allocation overhead
 * - Ring-buffer style reuse with automatic reset per frame
 * - Support for both vertex and index buffers
 * - Automatic alignment for optimal GPU access
 */
class DynamicVertexBufferPool {
public:
    static constexpr uint32_t DEFAULT_BUFFER_SIZE = 1024 * 1024;  // 1MB per buffer
    static constexpr uint32_t DEFAULT_BUFFER_COUNT = 4;           // 4 buffers (2 for vertices, 2 for indices)
    static constexpr uint32_t MIN_ALIGNMENT = 16;                 // 16-byte minimum alignment

    /**
     * @brief Construct a new Dynamic Vertex Buffer Pool object
     *
     * @param device The Vulkan logical device
     * @param physicalDevice The Vulkan physical device
     */
    DynamicVertexBufferPool(VkDevice device, VkPhysicalDevice physicalDevice);

    /**
     * @brief Destroy the Dynamic Vertex Buffer Pool object
     *
     * Automatically cleans up all allocated buffers and memory
     */
    ~DynamicVertexBufferPool();

    /**
     * @brief Initialize the buffer pool
     *
     * Creates the specified number of vertex and index buffers with the given size.
     * Each buffer is created with HOST_VISIBLE and HOST_COHERENT memory properties
     * for efficient CPU-GPU data transfer.
     *
     * @param bufferSize Size of each buffer in bytes (default: 1MB)
     * @param bufferCount Number of buffers to create for each type (default: 4)
     * @return true if initialization succeeded
     * @return false if initialization failed
     */
    bool initialize(uint32_t bufferSize = DEFAULT_BUFFER_SIZE,
                    uint32_t bufferCount = DEFAULT_BUFFER_COUNT);

    /**
     * @brief Clean up all resources
     *
     * Destroys all buffers and frees associated memory
     */
    void cleanup();

    /**
     * @brief Allocate space for vertex data
     *
     * Copies the provided vertex data into the buffer pool and returns
     * an allocation descriptor that can be used for rendering.
     *
     * @param data Pointer to the vertex data
     * @param size Size of the vertex data in bytes
     * @param currentFrame Current frame index for tracking
     * @return DynamicVertexAllocation Allocation information, or empty allocation if failed
     */
    DynamicVertexAllocation allocateVertexData(const void* data, uint32_t size, uint32_t currentFrame);

    /**
     * @brief Allocate space for index data
     *
     * Copies the provided index data into the buffer pool and returns
     * an allocation descriptor that can be used for rendering.
     *
     * @param data Pointer to the index data
     * @param size Size of the index data in bytes
     * @param currentFrame Current frame index for tracking
     * @return DynamicVertexAllocation Allocation information, or empty allocation if failed
     */
    DynamicVertexAllocation allocateIndexData(const void* data, uint32_t size, uint32_t currentFrame);

    /**
     * @brief Reset allocations for a specific frame
     *
     * Resets the allocation offsets, making previously used buffer space
     * available for new allocations. This should be called at the beginning
     * of each frame after waiting for the frame to complete.
     *
     * @param frameIndex The frame index to reset
     */
    void resetFrame(uint32_t frameIndex);

    /**
     * @brief Check if the pool is initialized
     *
     * @return true if the pool has been initialized
     * @return false if the pool is not initialized
     */
    bool isInitialized() const { return m_initialized; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    bool m_initialized = false;

    /**
     * @brief Internal structure representing a single buffer slot
     */
    struct BufferSlot {
        ::VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mappedPtr = nullptr;
        uint32_t currentOffset = 0;
        uint32_t totalSize = 0;
    };

    std::vector<BufferSlot> m_vertexBuffers;
    std::vector<BufferSlot> m_indexBuffers;

    /**
     * @brief Find a suitable memory type index
     *
     * @param typeFilter Bitmask of acceptable memory types
     * @param properties Required memory property flags
     * @return uint32_t Memory type index, or UINT32_MAX if not found
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    /**
     * @brief Create a single buffer with specified usage flags
     *
     * @param slot Reference to the buffer slot to initialize
     * @param usage Vulkan buffer usage flags
     * @param size Size of the buffer in bytes
     * @return true if buffer creation succeeded
     * @return false if buffer creation failed
     */
    bool createBuffer(BufferSlot& slot, VkBufferUsageFlags usage, VkDeviceSize size);

    /**
     * @brief Destroy a single buffer slot
     *
     * @param slot Reference to the buffer slot to clean up
     */
    void destroyBuffer(BufferSlot& slot);
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_DYNAMIC_VERTEX_BUFFER_POOL_H
