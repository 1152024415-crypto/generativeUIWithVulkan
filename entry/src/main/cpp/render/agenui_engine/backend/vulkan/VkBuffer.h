/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Vulkan Buffer - implements Core::IBuffer directly
 * No adapter layer needed.
 */

#ifndef AGENUI_ENGINE_VK_BUFFER_H
#define AGENUI_ENGINE_VK_BUFFER_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include "core/IGraphicsAPI.h"

namespace AgenUIEngine {

/**
 * @brief Unified Vulkan Buffer class implementing Core::IBuffer
 *
 * NOTE: Inside this class, the Vulkan handle type ::VkBuffer (global scope)
 * is distinct from AgenUIEngine::VkBuffer (this class).
 * All Vulkan API types must use the :: prefix to disambiguate.
 */
class VkBuffer : public Core::IBuffer {
public:
    VkBuffer();
    ~VkBuffer() override;

    // -------------------------------------------------------------------------
    // IBuffer interface implementation
    // -------------------------------------------------------------------------

    void* map() override;
    void unmap() override;
    void update(const void* data, size_t size, size_t offset = 0) override;
    size_t getSize() const override;
    Core::BufferUsage getUsage() const override;
    bool isPersistentlyMapped() const override;
    uint64_t getNativeHandle() const override;

    // -------------------------------------------------------------------------
    // Vulkan-specific creation methods
    // -------------------------------------------------------------------------

    bool createUniform(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size);
    bool createVertex(VkDevice device, VkPhysicalDevice physicalDevice,
                     VkCommandPool commandPool, VkQueue graphicsQueue,
                     const void* data, VkDeviceSize size);
    bool createIndex(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool, VkQueue graphicsQueue,
                    const void* data, VkDeviceSize size);
    bool createStaging(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size);

    /**
     * @brief Update buffer data (Vulkan-specific overload with VkDeviceSize)
     */
    void updateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    void cleanup(VkDevice device);

    /**
     * @brief Detach buffer and memory from automatic cleanup
     */
    void detach() {
        m_buffer = VK_NULL_HANDLE;
        m_memory = VK_NULL_HANDLE;
        m_mappedData = nullptr;
        m_size = 0;
    }

    // -------------------------------------------------------------------------
    // Vulkan getters - return native Vulkan handle types
    // -------------------------------------------------------------------------

    ::VkBuffer getBuffer() const { return m_buffer; }
    VkDeviceMemory getMemory() const { return m_memory; }
    void* getMappedData() const { return m_mappedData; }
    VkDeviceSize getVkSize() const { return m_size; }
    bool isMapped() const { return m_mappedData != nullptr; }

private:
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                           uint32_t typeFilter,
                           VkMemoryPropertyFlags properties);

    bool createBufferInternal(VkDevice device, VkPhysicalDevice physicalDevice,
                     VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties);

    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                   ::VkBuffer srcBuffer, ::VkBuffer dstBuffer, VkDeviceSize size);

    ::VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    void* m_mappedData = nullptr;
    VkDeviceSize m_size = 0;
    Core::BufferUsage m_usage = Core::BufferUsage::UniformBuffer;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_VK_BUFFER_H
