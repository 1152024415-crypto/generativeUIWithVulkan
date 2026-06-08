/*
 * Vulkan Renderer - thin coordinator implementing Core::IRenderer
 * Delegates Vulkan infrastructure to VulkanContext, pipelines to PipelineManager,
 * image caching to VkTextureCache, and glass effect to GlassEffect.
 */

#ifndef AGENUI_ENGINE_VKRENDERER_H
#define AGENUI_ENGINE_VKRENDERER_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_ohos.h>
#include <native_window/external_window.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

#include <string>
#include <memory>
#include <unordered_map>
#include "thirdparty/glm/glm.hpp"
#include "core/IGraphicsAPI.h"
#include "core/GeometryBuilder.h"
#include "core/TextLayoutCache.h"

namespace AgenUIEngine {

// Forward declarations
class VulkanContext;
class RectPipeline;
class RoundedRectPipeline;
class CirclePipeline;
class ImagePipeline;
class TextPipeline;
class TextLayoutEngine;
class TextLayoutManager;
class VkTextureCache;
class GlassEffect;
struct PreparedGlyphRun;
struct TextLayoutResult;

/**
 * Cross-platform Vulkan renderer implementing Core::IRenderer
 * Thin coordinator - delegates to VulkanContext, PipelineManager,
 * VkTextureCache, GlassEffect.
 */
class VkRenderer : public Core::IRenderer {
public:
    VkRenderer();
    ~VkRenderer() override;

    // -------------------------------------------------------------------------
    // IRenderer interface - Lifecycle
    // -------------------------------------------------------------------------

    bool initialize(const Core::RendererInitParams& params) override;
    void beginFrame() override;
    void endFrame() override;
    void cleanup() override;
    bool isInitialized() const override;

    /** @brief Legacy initialization (used by plugin_render.cpp, Application.cpp) */
    bool initialize(void* nativeWindow, int width, int height);

    // -------------------------------------------------------------------------
    // IRenderer interface - High-Level Draw Primitives
    // -------------------------------------------------------------------------

    void drawRect(const glm::vec2& position, const glm::vec2& size,
                  const glm::vec3& color) override;

    void drawRoundedRect(const glm::vec2& position, const glm::vec2& size,
                         float cornerRadius, const glm::vec3& color, float alpha = 1.0f) override;

    void drawGlassRoundedRect(const glm::vec2& position, const glm::vec2& size,
                               float cornerRadius, const glm::vec3& color) override;

    void drawPolygon(const glm::vec2& center, const std::vector<glm::vec2>& vertices,
                     const glm::vec3& color, float alpha = 1.0f) override;

    void drawCircle(const glm::vec2& center, float radius,
                    const glm::vec3& color, float alpha = 1.0f) override;

    void drawText(const std::string& text, const glm::vec2& position,
                  uint32_t fontSize, const glm::vec3& color,
                  const std::string& fontFamily = "default") override;

    void drawMultiLineText(const std::string& text, const glm::vec2& position,
                           uint32_t fontSize, const glm::vec3& color,
                           float maxWidth, float lineHeight) override;

    void setTextGlow(float glowWidth, float glowIntensity) override;
    void setTextGradient(bool hasGradient, const glm::vec3& endColor, int direction = 0) override;
    void setTextStroke(float width, const glm::vec3& color) override;

    void drawImage(const std::string& imagePath, const glm::vec2& position,
                   const glm::vec2& size, float rotation = 0.0f,
                   const std::vector<glm::vec2>& clipVertices = {},
                   const glm::vec2& clipCenter = {}) override;

    // -------------------------------------------------------------------------
    // IRenderer interface - Setup
    // -------------------------------------------------------------------------

    void setClearColor(float r, float g, float b, float a) override;
    void setResourceManager(void* resourceManager) override;
    bool initializeFonts(void* resourceManager, const std::string& fontName) override;

    bool createPipelines() override;

    // -------------------------------------------------------------------------
    // IRenderer interface - Queries
    // -------------------------------------------------------------------------

    int getWidth() const override;
    int getHeight() const override;
    Core::GraphicsAPI getAPI() const override;
    void waitIdle() override;
    void recreateSwapChain() override;
    void updateSurfaceSize(int width, int height) override;
    void setCoordinateMapping(int width, int height) override;

    // Glass refraction support
    void prepareGlassPass() override;

    // Access FontManager for configuration-driven registration
    FontManager* getFontManager() override;

    // Pre-cache glyphs for a text string to avoid run-time atlas upload stalls
    void precacheText(const std::string& text, uint32_t fontSize) override;

    // Stream text support (typewriter effect)
    uint32_t prepareTextLayout(const std::string& key, const std::string& text,
                                const glm::vec2& position, uint32_t fontSize,
                                const std::string& fontFamily = "default",
                                float maxWidth = 0.0f) override;
    uint32_t prepareTextLayout(const std::string& key,
                                const std::vector<Core::TextBlock>& blocks,
                                const glm::vec2& position, uint32_t baseFontSize,
                                float maxWidth = 0.0f) override;
    void drawTextPartial(const std::string& key, const glm::vec3& color,
                          uint32_t visibleChars) override;
    void clearTextLayoutCache() override;

    // -------------------------------------------------------------------------
    // Batch Execution hooks (Template Method pattern)
    // -------------------------------------------------------------------------

    void onBeginBatch() override;
    void onDrawTypeChange(Core::DrawType newType) override;
    void onEndBatch(Core::FrameStats& stats) override;

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    bool initializeFonts(NativeResourceManager* resourceManager, const std::string& fontName);

    // Pipeline lifecycle helper
    void cleanupPipelines();

    // Draw helpers
    void drawRoundedRectImpl(const glm::vec2& position, const glm::vec2& size,
                              float cornerRadius, const glm::vec3& color,
                              bool glass, float alpha = 1.0f);

    // Vulkan-specific text layout cache (extends Core::TextLayoutCacheBase with GPU handles)
    struct VkTextLayoutCache : Core::TextLayoutCacheBase {
        // Vulkan GPU resources
        ::VkBuffer gpuVertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory gpuVertexMemory = VK_NULL_HANDLE;
        size_t gpuVertexCapacity = 0;
        ::VkBuffer gpuIndexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory gpuIndexMemory = VK_NULL_HANDLE;
        size_t gpuIndexCapacity = 0;
        uint32_t totalIndexCount = 0;

        // Non-styled path pipeline handles
        VkPipeline cachedPipeline = VK_NULL_HANDLE;
        VkDescriptorSet cachedDescriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout cachedPipelineLayout = VK_NULL_HANDLE;
        float cachedPushConstants[32] = {};     // Shader push constants (GPU layer)

        // NDC coordinates (GPU-specific, converted from base::pixelPos)
        glm::vec2 ndcPos{0};

        // Per-segment Vulkan handles + GPU push constants (parallel to base::segments)
        struct VkSegmentData {
            float pushConstants[32] = {};
            VkPipeline pipeline = VK_NULL_HANDLE;
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            VkPipelineLayout layout = VK_NULL_HANDLE;
        };
        std::vector<VkSegmentData> vkSegments;
    };

    // Stream text GPU buffer helpers
    bool createStreamBuffer(VkDevice device, VkPhysicalDevice physDevice,
                            const void* data, size_t size,
                            VkBufferUsageFlags usage,
                            ::VkBuffer& buffer, VkDeviceMemory& memory);
    bool uploadStreamBuffer(VkDevice device, VkPhysicalDevice physDevice,
                            const void* data, size_t size,
                            VkBufferUsageFlags usage,
                            ::VkBuffer& buffer, VkDeviceMemory& memory,
                            size_t& capacity);
    bool uploadStreamBufferAt(VkDevice device, VkDeviceMemory memory,
                              size_t offset, const void* data, size_t size);
    void releaseLayoutGpuResources(VkDevice device, VkTextLayoutCache& cache);
    void drawStyledTextPartial(const VkTextLayoutCache& cache, const glm::vec3& color,
                                uint32_t visibleChars);

    // Unified draw submission — handles BGRA swap + glass special path
    void submitDraw(const Core::DrawPacket& pkt, VkPipeline pipeline, VkPipelineLayout layout,
                    VkDescriptorSet descriptorSet = VK_NULL_HANDLE);

    // Update identity MVP for pipelines that need it
    void updateIdentityMVP(const char* pipelineName);

    // Component ownership
    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<RectPipeline> m_rectPipeline;
    std::unique_ptr<RoundedRectPipeline> m_roundedRectPipeline;
    std::unique_ptr<CirclePipeline> m_circlePipeline;
    std::unique_ptr<ImagePipeline> m_imagePipeline;
    std::unique_ptr<TextPipeline> m_textPipeline;

    // Text rendering
    std::unique_ptr<TextLayoutEngine> m_textLayoutEngine;
    std::unique_ptr<TextLayoutManager> m_textLayoutManager;

    // Delegated subsystems
    std::unique_ptr<VkTextureCache> m_textureCache;
    std::unique_ptr<GlassEffect> m_glassEffect;

    // Glow state
    float m_glowIntensity = 0.0f;
    float m_glowWidth = 0.0f;

    // Gradient state
    bool m_hasGradient = false;
    glm::vec3 m_gradientEndColor{0.0f};
    int m_gradientDirection = 0;

    // Stroke state
    float m_strokeWidth = 0.0f;
    glm::vec3 m_strokeColor{0.0f};

    // Batch execution state tracking
    struct BatchState {
        uint32_t pipelineChanges = 0;
    } m_batchState;

    // Reusable scratch buffers for variable-size geometry (drawCircle, drawPolygon, drawImage)
    // .clear() preserves capacity — zero heap allocation after first frame's largest draw
    std::vector<float> m_scratchVerts;
    std::vector<uint16_t> m_scratchIndices;

    // State
    bool m_initialized = false;

    // Coordinate mapper (backend-agnostic, updated on init/recreate/setCoordinateMapping)
    Core::CoordinateMapper m_coordMapper;

    std::unordered_map<std::string, VkTextLayoutCache> m_textLayoutCache;

    // Platform resource manager
    NativeResourceManager* m_resourceManager = nullptr;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_VKRENDERER_H
