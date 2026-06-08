#ifndef AGENUI_ENGINE_GLASS_EFFECT_H
#define AGENUI_ENGINE_GLASS_EFFECT_H

#include <vulkan/vulkan.h>
#include <vector>

namespace AgenUIEngine {

// Forward declarations
class VulkanContext;
class ImagePipeline;

/**
 * @brief Glass / frosted-glass refraction effect
 *
 * Manages a background texture that captures the current framebuffer contents
 * and provides descriptor sets for the rounded-rect pipeline (set=1) and the
 * image pipeline (fullscreen restore).
 */
class GlassEffect {
public:
    GlassEffect() = default;
    ~GlassEffect();

    // Accessors used by VkRenderer
    VkDescriptorSet getBgDescriptorSet() const { return m_bgDescriptorSet; }
    VkDescriptorSet getBgImageDescriptorSet() const { return m_bgImageDescriptorSet; }
    VkDescriptorSetLayout getBgDescriptorSetLayout() const { return m_bgDescriptorSetLayout; }

    /// Ensure the descriptor-set layout exists (call before pipeline create).
    bool ensureDescriptorSetLayout(VkDevice device);

    /// Lazily create background texture resources (call on first use or swapchain rebuild).
    bool ensureResources(VulkanContext& ctx, ImagePipeline* imagePipeline);

    /// Execute glass pass: end current render pass → copy FB → restart render pass + restore BG.
    void preparePass(VulkanContext& ctx, ImagePipeline* imagePipeline);

    /// Destroy all resources (swapchain rebuild or final cleanup).
    void cleanup(VkDevice device);

private:
    bool createBackgroundTexture(VulkanContext& ctx, ImagePipeline* imagePipeline);
    void copyFramebufferToBackground(VulkanContext& ctx, ImagePipeline* imagePipeline);
    void endRenderPassForCopy(VulkanContext& ctx);
    void beginRenderPassAfterCopy(VulkanContext& ctx, ImagePipeline* imagePipeline);

    VkImage m_bgImage = VK_NULL_HANDLE;
    VkDeviceMemory m_bgImageMemory = VK_NULL_HANDLE;
    VkImageView m_bgImageView = VK_NULL_HANDLE;
    VkSampler m_bgSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bgDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_bgDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_bgDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet m_bgImageDescriptorSet = VK_NULL_HANDLE;
    bool m_bgTextureCreated = false;

    // Stashed VulkanContext pointer (valid between ensureResources / cleanup)
    VulkanContext* m_ctx = nullptr;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_GLASS_EFFECT_H
