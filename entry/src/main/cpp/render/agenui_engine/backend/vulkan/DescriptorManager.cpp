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

#include "DescriptorManager.h"

// Platform-specific logging
#if AGENUI_PLATFORM_WINDOWS
    #define LOGI(...) std::printf("[INFO] [DescriptorManager] " __VA_ARGS__); std::printf("\n")
    #define LOGE(...) std::printf("[ERROR] [DescriptorManager] " __VA_ARGS__); std::printf("\n")
#elif AGENUI_PLATFORM_HARMONYOS
    #include "logger_common.h"
#endif

#include <cstring>

namespace AgenUIEngine {

DescriptorManager::DescriptorManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : m_device(device)
    , m_physicalDevice(physicalDevice) {
}

DescriptorManager::~DescriptorManager() {
    cleanup();
}

bool DescriptorManager::createDescriptorSetLayout(const std::string& name, VkShaderStageFlags shaderStages) {
    // Check if already exists
    if (m_descriptorSets.find(name) != m_descriptorSets.end()) {
        LOGE("DescriptorManager: Descriptor set '%s' already exists", name.c_str());
        return false;
    }

    DescriptorSetInfo info{};

    // Create descriptor set layout only
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = shaderStages;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &info.layout) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to create descriptor set layout for '%s'", name.c_str());
        return false;
    }

    // Store the info (layout only, rest is VK_NULL_HANDLE)
    m_descriptorSets[name] = info;

    return true;
}

VkDescriptorSetLayout DescriptorManager::getDescriptorSetLayout(const std::string& name) const {
    auto it = m_descriptorSets.find(name);
    if (it != m_descriptorSets.end()) {
        return it->second.layout;
    }
    return VK_NULL_HANDLE;
}

bool DescriptorManager::createDescriptorSet(const std::string& name,
                                             VkDeviceSize uniformBufferSize,
                                             VkShaderStageFlags shaderStages) {
    // Check if already exists
    if (m_descriptorSets.find(name) != m_descriptorSets.end()) {
        LOGE("DescriptorManager: Descriptor set '%s' already exists", name.c_str());
        return false;
    }

    DescriptorSetInfo info{};

    // Create uniform buffer if needed
    if (uniformBufferSize > 0) {
        info.uniformBufferSize = uniformBufferSize;
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        if (!createBuffer(uniformBufferSize, usage, properties, info.uniformBuffer, info.uniformMemory)) {
            LOGE("DescriptorManager: Failed to create uniform buffer for '%s'", name.c_str());
            return false;
        }

        // Map the buffer memory
        if (vkMapMemory(m_device, info.uniformMemory, 0, uniformBufferSize, 0, &info.mappedData) != VK_SUCCESS) {
            LOGE("DescriptorManager: Failed to map uniform buffer for '%s'", name.c_str());
            vkDestroyBuffer(m_device, info.uniformBuffer, nullptr);
            vkFreeMemory(m_device, info.uniformMemory, nullptr);
            return false;
        }
    }

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = shaderStages;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &info.layout) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to create descriptor set layout for '%s'", name.c_str());
        if (info.mappedData) vkUnmapMemory(m_device, info.uniformMemory);
        vkDestroyBuffer(m_device, info.uniformBuffer, nullptr);
        vkFreeMemory(m_device, info.uniformMemory, nullptr);
        return false;
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &info.pool) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to create descriptor pool for '%s'", name.c_str());
        vkDestroyDescriptorSetLayout(m_device, info.layout, nullptr);
        if (info.mappedData) vkUnmapMemory(m_device, info.uniformMemory);
        vkDestroyBuffer(m_device, info.uniformBuffer, nullptr);
        vkFreeMemory(m_device, info.uniformMemory, nullptr);
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = info.pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &info.layout;

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &info.set) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to allocate descriptor set for '%s'", name.c_str());
        vkDestroyDescriptorPool(m_device, info.pool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, info.layout, nullptr);
        if (info.mappedData) vkUnmapMemory(m_device, info.uniformMemory);
        vkDestroyBuffer(m_device, info.uniformBuffer, nullptr);
        vkFreeMemory(m_device, info.uniformMemory, nullptr);
        return false;
    }

    // Update descriptor set
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = info.uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBufferSize;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = info.set;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);

    // Store the info
    m_descriptorSets[name] = info;

    return true;
}

DescriptorManager::DescriptorSetInfo* DescriptorManager::getDescriptorSet(const std::string& name) {
    auto it = m_descriptorSets.find(name);
    if (it != m_descriptorSets.end()) {
        return &it->second;
    }
    return nullptr;
}

bool DescriptorManager::completeDescriptorSet(const std::string& name, VkDeviceSize uniformBufferSize) {
    auto it = m_descriptorSets.find(name);
    if (it == m_descriptorSets.end()) {
        LOGE("DescriptorManager: Descriptor set '%s' not found (call createDescriptorSetLayout first)", name.c_str());
        return false;
    }

    DescriptorSetInfo& info = it->second;

    // Check if already completed
    if (info.set != VK_NULL_HANDLE) {
        LOGE("DescriptorManager: Descriptor set '%s' already completed", name.c_str());
        return false;
    }

    // Create uniform buffer if needed
    if (uniformBufferSize > 0) {
        info.uniformBufferSize = uniformBufferSize;
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        if (!createBuffer(uniformBufferSize, usage, properties, info.uniformBuffer, info.uniformMemory)) {
            LOGE("DescriptorManager: Failed to create uniform buffer for '%s'", name.c_str());
            return false;
        }

        // Map the buffer memory
        if (vkMapMemory(m_device, info.uniformMemory, 0, uniformBufferSize, 0, &info.mappedData) != VK_SUCCESS) {
            LOGE("DescriptorManager: Failed to map uniform buffer for '%s'", name.c_str());
            return false;
        }
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &info.pool) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to create descriptor pool for '%s'", name.c_str());
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = info.pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &info.layout;

    if (vkAllocateDescriptorSets(m_device, &allocInfo, &info.set) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to allocate descriptor set for '%s'", name.c_str());
        return false;
    }

    // Update descriptor set
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = info.uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBufferSize;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = info.set;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);

    return true;
}

void DescriptorManager::updateUniformBuffer(const std::string& name, const void* data, VkDeviceSize size) {
    DescriptorSetInfo* info = getDescriptorSet(name);
    if (!info) {
        LOGE("DescriptorManager: Descriptor set '%s' not found", name.c_str());
        return;
    }

    if (!info->mappedData) {
        LOGE("DescriptorManager: Descriptor set '%s' has no mapped memory", name.c_str());
        return;
    }

    if (size > info->uniformBufferSize) {
        LOGE("DescriptorManager: Data size %llu exceeds buffer size %llu for '%s'",
             size, info->uniformBufferSize, name.c_str());
        return;
    }

    std::memcpy(info->mappedData, data, static_cast<size_t>(size));
}

void DescriptorManager::bindDescriptorSet(const std::string& name, VkCommandBuffer commandBuffer, VkPipelineLayout layout) {
    DescriptorSetInfo* info = getDescriptorSet(name);
    if (!info) {
        LOGE("DescriptorManager: Descriptor set '%s' not found", name.c_str());
        return;
    }

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           layout, 0, 1, &info->set, 0, nullptr);
}

void DescriptorManager::destroyDescriptorSet(const std::string& name) {
    auto it = m_descriptorSets.find(name);
    if (it == m_descriptorSets.end()) {
        return;
    }

    DescriptorSetInfo& info = it->second;

    // Unmap memory
    if (info.mappedData) {
        vkUnmapMemory(m_device, info.uniformMemory);
        info.mappedData = nullptr;
    }

    // Destroy buffer
    if (info.uniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, info.uniformBuffer, nullptr);
        info.uniformBuffer = VK_NULL_HANDLE;
    }

    // Free memory
    if (info.uniformMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, info.uniformMemory, nullptr);
        info.uniformMemory = VK_NULL_HANDLE;
    }

    // Descriptor set is implicitly destroyed when pool is destroyed

    // Destroy descriptor set layout
    if (info.layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, info.layout, nullptr);
        info.layout = VK_NULL_HANDLE;
    }

    // Destroy descriptor pool
    if (info.pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, info.pool, nullptr);
        info.pool = VK_NULL_HANDLE;
    }

    // Remove from map
    m_descriptorSets.erase(it);
}

void DescriptorManager::cleanup() {
    for (auto& pair : m_descriptorSets) {
        DescriptorSetInfo& info = pair.second;

        if (info.mappedData) {
            vkUnmapMemory(m_device, info.uniformMemory);
        }

        if (info.uniformBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, info.uniformBuffer, nullptr);
        }

        if (info.uniformMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, info.uniformMemory, nullptr);
        }

        if (info.layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, info.layout, nullptr);
        }

        if (info.pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device, info.pool, nullptr);
        }
    }

    m_descriptorSets.clear();
}

uint32_t DescriptorManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("DescriptorManager: Failed to find suitable memory type");
    return UINT32_MAX;
}

bool DescriptorManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                      ::VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to create buffer");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        LOGE("DescriptorManager: Failed to find memory type for buffer");
        vkDestroyBuffer(m_device, buffer, nullptr);
        return false;
    }

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        LOGE("DescriptorManager: Failed to allocate buffer memory");
        vkDestroyBuffer(m_device, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(m_device, buffer, memory, 0);
    return true;
}

} // namespace AgenUIEngine
