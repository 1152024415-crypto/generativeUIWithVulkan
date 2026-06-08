#ifndef AGENUI_PIPELINE_MANAGER_H
#define AGENUI_PIPELINE_MANAGER_H

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "VkShader.h"
#include "thirdparty/glm/glm.hpp"
#include "modules/text/MsdfAtlas.h"
#include "modules/text/text_layout/TextLayout.h"

namespace AgenUIEngine {

// Forward declarations
class VulkanContext;
class TextLayoutEngine;
class TextLayoutManager;
struct TextDrawData;
struct PreparedGlyphRun;
struct TextLayoutResult;
struct PreparedTextWithSegments;
struct LayoutLinesResult;
struct LayoutLine;

/**
 * Base class for all pipeline types.
 */
class IPipeline {
public:
    virtual ~IPipeline() = default;
    virtual bool create(VulkanContext& ctx) = 0;
    virtual void cleanup(VulkanContext& ctx) = 0;
};

/**
 * Base class for pipelines that share common Vulkan resource management.
 * Provides unified cleanup, shader loading, and vertex input helpers.
 */
class PipelineBase : public IPipeline {
public:
    void cleanup(VulkanContext& ctx) override;

    // Common getters
    VkPipeline getPipeline() const { return m_pipeline; }
    VkPipelineLayout getLayout() const { return m_layout; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
    VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

protected:
    explicit PipelineBase(const char* descriptorName);

    // Shader loading helpers
    bool loadShaders(VulkanContext& ctx, const char* vertFile, const char* fragFile);
    bool loadShadersWithFallback(VulkanContext& ctx,
                                  const char* vert, const char* frag,
                                  const char* fbVert, const char* fbFrag);

    // Vertex input helper — covers the shared "2 attributes at binding 0" pattern
    struct VertexInputConfig {
        uint32_t stride;
        VkFormat attr1Format;
        uint32_t attr1Offset;
    };
    void setupVertexInput(const VertexInputConfig& config,
                          std::vector<VkVertexInputBindingDescription>& bindings,
                          std::vector<VkVertexInputAttributeDescription>& attrs);

    // Common Vulkan resource handles
    const char* m_descriptorName;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkShaderProgram m_shader;
};

/**
 * Simple rectangle rendering pipeline.
 */
class RectPipeline : public PipelineBase {
public:
    RectPipeline() : PipelineBase("rectPipeline") {}
    bool create(VulkanContext& ctx) override;
};

/**
 * Rounded rectangle rendering pipeline with SDF corner radius.
 */
class RoundedRectPipeline : public PipelineBase {
public:
    RoundedRectPipeline() : PipelineBase("roundedRectPipeline") {}
    bool create(VulkanContext& ctx, VkDescriptorSetLayout bgDescriptorSetLayout = VK_NULL_HANDLE);
    bool create(VulkanContext& ctx) override { return create(ctx, VK_NULL_HANDLE); }
};

/**
 * Circle rendering pipeline with SDF anti-aliasing.
 */
class CirclePipeline : public PipelineBase {
public:
    CirclePipeline() : PipelineBase("circlePipeline") {}
    bool create(VulkanContext& ctx) override;
};

/**
 * Image/texture rendering pipeline.
 */
class ImagePipeline : public PipelineBase {
public:
    ImagePipeline() : PipelineBase("") {}
    bool create(VulkanContext& ctx) override;
};

/**
 * Text rendering pipeline — owns all GPU resources for text rendering.
 * Delegates CPU layout work to TextLayoutEngine.
 * Handles: pipeline creation, atlas texture upload, buffer management, draw commands.
 */
class TextPipeline : public IPipeline {
public:
    explicit TextPipeline(TextLayoutEngine& layoutEngine);
    bool create(VulkanContext& ctx) override;
    void cleanup(VulkanContext& ctx) override;

    // High-level draw API
    void drawText(VulkanContext& ctx,
                  const std::string& text,
                  const glm::vec2& ndcPos,
                  uint32_t fontSize,
                  const glm::vec3& color);

    void drawTextBlock(VulkanContext& ctx,
                       TextLayoutManager& mgr,
                       const PreparedTextWithSegments& prepared,
                       float x, float y,
                       float maxWidth, float lineHeight,
                       uint32_t fontSize,
                       const glm::vec3& color);

    void drawTextBlock(VulkanContext& ctx,
                       const PreparedTextWithSegments& prepared,
                       const LayoutLinesResult& layoutResult,
                       float x, float y,
                       float maxWidth, float lineHeight,
                       uint32_t fontSize,
                       const glm::vec3& color);

    /** Execute GPU draw from pre-built TextDrawData. */
    void executeDraw(VulkanContext& ctx, const TextDrawData& drawData);

    /** Reset persistent buffer offsets at frame start. */
    void resetPersistentBuffers();

    /**
     * Ensure MSDF GPU resources (atlas texture + pipeline) are ready.
     * Called lazily on first glow draw.
     */
    bool ensureMsdfReady(VulkanContext& ctx);

    /** Upload pending glyph atlas data to GPU (bitmap + SDF). */
    void uploadPendingAtlases(VulkanContext& ctx);

    VkPipelineLayout getPipelineLayout() const { return m_textPipelineLayout; }

    /** Resolved pipeline handles for a given render path. */
    struct PipelineHandles {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
    };

    /**
     * Get resolved pipeline/descriptor/layout for a render path.
     * Handles MSDF lazy init internally.
     */
    PipelineHandles getPipelineHandles(TextRenderPath path, VulkanContext& ctx);

private:
    // Internal GPU helpers
    bool createTextPipeline(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass);
    void cleanupTextPipeline(VkDevice device);
    bool createDescriptorSetLayout(VkDevice device);
    bool createDescriptorPoolAndSets(VkDevice device);
    void uploadBitmapAtlas(VulkanContext& ctx);
    void uploadSDFAtlas(VulkanContext& ctx);

    // Persistent buffer management
    bool initPersistentBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
    void cleanupPersistentBuffers(VkDevice device);
    bool writeVertexData(const float* data, size_t byteSize, ::VkBuffer& outBuffer, VkDeviceSize& outOffset);
    bool writeIndexData(const uint16_t* data, size_t byteSize, ::VkBuffer& outBuffer, VkDeviceSize& outOffset);

    // Buffer creation (fallback for persistent overflow)
    void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                            const float* vertices, size_t size, ::VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                           const uint16_t* indices, size_t size, ::VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Reference to CPU layout engine (not owned)
    TextLayoutEngine& m_layoutEngine;

    // Vulkan pipeline objects
    VkPipeline m_textPipeline = VK_NULL_HANDLE;
    VkPipeline m_msdfPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_textPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_textDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_textDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_textDescriptorSet = VK_NULL_HANDLE;

    // SDF descriptor set (allocated from same pool)
    VkDescriptorSet m_sdfDescriptorSet = VK_NULL_HANDLE;

    // MSDF descriptor set and atlas
    VkDescriptorSet m_msdfDescriptorSet = VK_NULL_HANDLE;
    std::unique_ptr<class MsdfAtlas> m_msdfAtlas;
    bool m_msdfInitAttempted = false;

    // Atlas textures (GPU side)
    class VkTexture* m_bitmapTexture = nullptr;  // owned
    bool m_bitmapTextureOwned = false;
    class VkTexture* m_sdfTexture = nullptr;      // owned
    bool m_sdfTextureOwned = false;

    // Persistent staging buffers
    static const size_t PERSISTENT_VB_SIZE = 256 * 1024;
    static const size_t PERSISTENT_IB_SIZE = 64 * 1024;

    struct PersistentBuffer {
        ::VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        size_t capacity = 0;
    };

    PersistentBuffer m_persistentVB;
    PersistentBuffer m_persistentIB;
    size_t m_persistentVBOffset = 0;
    size_t m_persistentIBOffset = 0;
    bool m_persistentBuffersInitialized = false;

    // Cached Vulkan handles (from create())
    VkRenderPass m_cachedRenderPass = VK_NULL_HANDLE;
};

} // namespace AgenUIEngine

#endif // AGENUI_PIPELINE_MANAGER_H
