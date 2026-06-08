#ifndef AGENUI_ENGINE_VK_TEXTURE_CACHE_H
#define AGENUI_ENGINE_VK_TEXTURE_CACHE_H

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define AGENUI_PLATFORM_WINDOWS 1
#elif defined(__OHOS__) || defined(AGENUI_PLATFORM_HARMONYOS)
    #define AGENUI_PLATFORM_HARMONYOS 1
#endif

#if AGENUI_PLATFORM_HARMONYOS
    #include <rawfile/raw_file_manager.h>
#endif

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "VkTexture.h"

namespace AgenUIEngine {

// Forward declarations
class VulkanContext;
class ImagePipeline;

/**
 * @brief Image loading and descriptor-set cache
 *
 * Manages a path → (texture, descriptorSet) map.  On first request the image
 * is loaded from the file-system (or HarmonyOS raw-file), a descriptor set is
 * allocated from the ImagePipeline's pool, and both are cached for subsequent
 * frames.
 */
class VkTextureCache {
public:
    VkTextureCache() = default;
    ~VkTextureCache();

    /// Return a cached descriptor set for *imagePath*, or load + create one.
    VkDescriptorSet getOrLoad(VulkanContext& ctx, ImagePipeline& pipeline,
                              const std::string& imagePath
#if AGENUI_PLATFORM_HARMONYOS
                              , NativeResourceManager* resourceManager = nullptr
#endif
                              );

    /// Destroy all cached textures and clear the map (swapchain rebuild).
    void clear(VkDevice device);

    /// Full teardown — same as clear().
    void cleanup(VkDevice device);

private:
    struct ImageCacheEntry {
        VkTexture texture;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };
    std::unordered_map<std::string, ImageCacheEntry> m_cache;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_VK_TEXTURE_CACHE_H
