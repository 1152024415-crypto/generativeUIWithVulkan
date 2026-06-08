#include "VkTextureCache.h"
#include "VulkanContext.h"
#include "PipelineManager.h"
#include "logger_common.h"

#if AGENUI_PLATFORM_HARMONYOS
#include <rawfile/raw_file_manager.h>
#endif

namespace AgenUIEngine {

VkTextureCache::~VkTextureCache() {
    // Caller should invoke cleanup() before destruction.
}

VkDescriptorSet VkTextureCache::getOrLoad(VulkanContext& ctx, ImagePipeline& pipeline,
                                           const std::string& imagePath
#if AGENUI_PLATFORM_HARMONYOS
                                           , NativeResourceManager* resourceManager
#endif
                                           ) {
    VkDevice device = ctx.getDevice();

    auto it = m_cache.find(imagePath);
    if (it != m_cache.end()) {
        return it->second.descriptorSet;
    }

    ImageCacheEntry entry;
    bool loadSuccess = false;

#if AGENUI_PLATFORM_HARMONYOS
    if (resourceManager) {
        loadSuccess = entry.texture.loadFromRawFile(device, ctx.getPhysicalDevice(),
                         ctx.getCommandPool(), ctx.getGraphicsQueue(), resourceManager, imagePath);
    }
#endif

    if (!loadSuccess) {
        std::vector<std::string> paths = {
            "/data/storage/el2/base/files/rawfile/" + imagePath,
            "/data/storage/el2/base/haps/entry/files/rawfile/" + imagePath,
            "/data/storage/el2/base/haps/entry/resources/rawfile/" + imagePath
        };
        for (const auto& p : paths) {
            loadSuccess = entry.texture.loadFromFile(device, ctx.getPhysicalDevice(),
                             ctx.getCommandPool(), ctx.getGraphicsQueue(), p);
            if (loadSuccess) break;
        }
    }

    if (!loadSuccess) {
        LOGE("VkTextureCache: Failed to load %s", imagePath.c_str());
        return VK_NULL_HANDLE;
    }

    // Allocate descriptor set from ImagePipeline's pool
    VkDescriptorSetLayout layout = pipeline.getDescriptorSetLayout();
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pipeline.getDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &entry.descriptorSet) != VK_SUCCESS) {
        entry.texture.cleanup(device);
        return VK_NULL_HANDLE;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = entry.texture.getImageView();
    imageInfo.sampler = entry.texture.getSampler();
    VkWriteDescriptorSet dw{};
    dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    dw.dstSet = entry.descriptorSet;
    dw.dstBinding = 0;
    dw.dstArrayElement = 0;
    dw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dw.descriptorCount = 1;
    dw.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, &dw, 0, nullptr);

    m_cache[imagePath] = std::move(entry);

    return m_cache[imagePath].descriptorSet;
}

void VkTextureCache::clear(VkDevice device) {
    for (auto& pair : m_cache) pair.second.texture.cleanup(device);
    m_cache.clear();
}

void VkTextureCache::cleanup(VkDevice device) {
    clear(device);
}

} // namespace AgenUIEngine
