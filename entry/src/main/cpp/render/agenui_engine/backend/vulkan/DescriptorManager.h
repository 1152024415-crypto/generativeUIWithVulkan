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

#ifndef AGENUI_ENGINE_DESCRIPTOR_MANAGER_H
#define AGENUI_ENGINE_DESCRIPTOR_MANAGER_H

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace AgenUIEngine {

/**
 * @brief Manages descriptor set layouts, pools, and allocations
 *
 * Provides automatic resource management and simplifies descriptor
 * operations that are otherwise verbose and error-prone.
 */
class DescriptorManager {
public:
    /**
     * @brief Information about a single descriptor set allocation
     */
    struct DescriptorSetInfo {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkDeviceSize uniformBufferSize = 0;
        ::VkBuffer uniformBuffer = VK_NULL_HANDLE;
        VkDeviceMemory uniformMemory = VK_NULL_HANDLE;
        void* mappedData = nullptr;
    };

    DescriptorManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~DescriptorManager();

    // Disable copying
    DescriptorManager(const DescriptorManager&) = delete;
    DescriptorManager& operator=(const DescriptorManager&) = delete;

    /**
     * @brief Create only the descriptor set layout (without allocating descriptors)
     * Use this when you need the layout before creating the full descriptor set.
     *
     * @param name Unique identifier for this descriptor set
     * @param shaderStages Which stages access the uniform buffer
     * @return true if successful
     */
    bool createDescriptorSetLayout(const std::string& name, VkShaderStageFlags shaderStages);

    /**
     * @brief Get descriptor set layout by name
     *
     * @param name Name of the descriptor set
     * @return Descriptor set layout, or VK_NULL_HANDLE if not found
     */
    VkDescriptorSetLayout getDescriptorSetLayout(const std::string& name) const;

    /**
     * @brief Create a descriptor set layout and allocate resources
     *
     * @param name Unique identifier for this descriptor set
     * @param uniformBufferSize Size of uniform buffer (0 if not needed)
     * @param shaderStages Which stages access the uniform buffer
     * @return true if successful
     */
    bool createDescriptorSet(const std::string& name,
                             VkDeviceSize uniformBufferSize,
                             VkShaderStageFlags shaderStages);

    /**
     * @brief Get descriptor set info by name
     *
     * @param name Name of the descriptor set
     * @return Pointer to descriptor set info, or nullptr if not found
     */
    DescriptorSetInfo* getDescriptorSet(const std::string& name);

    /**
     * @brief Complete descriptor set creation (buffer, pool, set) after layout was created
     * Use this after createDescriptorSetLayout() to finish creating the descriptor set.
     *
     * @param name Name of the descriptor set (must have layout already created)
     * @param uniformBufferSize Size of uniform buffer to create
     * @return true if successful
     */
    bool completeDescriptorSet(const std::string& name, VkDeviceSize uniformBufferSize);

    /**
     * @brief Update uniform buffer data for a descriptor set
     *
     * @param name Name of the descriptor set
     * @param data Pointer to data to copy
     * @param size Size of data in bytes
     */
    void updateUniformBuffer(const std::string& name, const void* data, VkDeviceSize size);

    /**
     * @brief Bind descriptor set for rendering
     *
     * @param name Name of the descriptor set
     * @param commandBuffer Command buffer to record bind operation
     * @param layout Pipeline layout to use
     */
    void bindDescriptorSet(const std::string& name, VkCommandBuffer commandBuffer, VkPipelineLayout layout);

    /**
     * @brief Clean up all resources for a descriptor set
     *
     * @param name Name of the descriptor set to destroy
     */
    void destroyDescriptorSet(const std::string& name);

    /**
     * @brief Clean up all resources
     */
    void cleanup();

private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;

    std::unordered_map<std::string, DescriptorSetInfo> m_descriptorSets;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      ::VkBuffer& buffer, VkDeviceMemory& memory);
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_DESCRIPTOR_MANAGER_H
