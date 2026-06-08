#include "VkRenderer.h"
#include "VulkanContext.h"
#include "PipelineManager.h"
#include "VkTextureCache.h"
#include "GlassEffect.h"
#include "logger_common.h"

#include <hilog/log.h>
#include <vulkan/vulkan_ohos.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include <native_window/external_window.h>
#include <hitrace/trace.h>

#include <vector>
#include <set>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <functional>
#include <cmath>
#include "thirdparty/glm/glm.hpp"
#include "thirdparty/glm/gtc/matrix_transform.hpp"
#include "DynamicVertexBufferPool.h"
#include "DescriptorManager.h"
#include "VkBuffer.h"
#include "VkTexture.h"
#include "modules/text/FontManager.h"
#include "modules/text/TextLayoutEngine.h"
#include "modules/text/TextAtlas.h"
#include "modules/text/text_layout/TextLayout.h"

namespace AgenUIEngine {

// ============================================================================
// Construction / Destruction
// ============================================================================

VkRenderer::VkRenderer() {
    m_textLayoutEngine = std::make_unique<TextLayoutEngine>();
    m_textureCache = std::make_unique<VkTextureCache>();
    m_glassEffect = std::make_unique<GlassEffect>();
}

VkRenderer::~VkRenderer() {
    cleanup();
}

// ============================================================================
// IRenderer - Lifecycle
// ============================================================================

bool VkRenderer::initialize(const Core::RendererInitParams& params) {
    if (m_initialized) return true;
    return initialize(params.nativeWindow, params.width, params.height);
}

bool VkRenderer::isInitialized() const {
    return m_initialized;
}

bool VkRenderer::initialize(void* nativeWindow, int width, int height) {
    if (m_initialized) return true;
    m_context = std::make_unique<VulkanContext>();
    if (!m_context->initialize(nativeWindow, width, height)) {
        LOGE("Failed to initialize Vulkan context");
        return false;
    }
    // Initialize coordinate mapper from swapchain extent + mapping dims
    VkExtent2D extent = m_context->getSwapChainExtent();
    m_coordMapper = Core::CoordinateMapper(
        static_cast<float>(extent.width), static_cast<float>(extent.height),
        static_cast<float>(m_context->getWidth()), static_cast<float>(m_context->getHeight()));
    m_initialized = true;
    return true;
}

void VkRenderer::beginFrame() {
    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:BeginFrame", "");
    if (m_context) m_context->beginFrame();
    // Reset persistent text staging buffer offsets once per frame
    if (m_textPipeline) m_textPipeline->resetPersistentBuffers();
    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
}

void VkRenderer::endFrame() {
    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:EndFrame", "");
    if (m_context) m_context->endFrame();
    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
}

void VkRenderer::cleanup() {
    if (!m_context) return;
    VkDevice device = m_context->getDevice();

    clearTextLayoutCache();
    m_textureCache->cleanup(device);
    cleanupPipelines();
    m_glassEffect->cleanup(device);

    m_textLayoutManager.reset();
    m_textLayoutEngine.reset();
    m_context.reset();
    m_initialized = false;
}

// ============================================================================
// IRenderer - Pipeline Creation & Lifecycle
// ============================================================================

bool VkRenderer::createPipelines() {
    m_rectPipeline = std::make_unique<RectPipeline>();
    if (!m_rectPipeline->create(*m_context)) return false;

    m_glassEffect->ensureDescriptorSetLayout(m_context->getDevice());
    m_roundedRectPipeline = std::make_unique<RoundedRectPipeline>();
    if (!m_roundedRectPipeline->create(*m_context, m_glassEffect->getBgDescriptorSetLayout())) return false;

    m_circlePipeline = std::make_unique<CirclePipeline>();
    if (!m_circlePipeline->create(*m_context)) return false;

    // Text pipeline requires TextLayoutEngine to be initialized (via initializeFonts).
    // On first call it will be skipped; initializeFonts() creates it lazily.
    // On recreateSwapChain() the layout engine is already initialized, so it runs here.
    if (m_textLayoutEngine && m_textLayoutEngine->isInitialized()) {
        m_textPipeline = std::make_unique<TextPipeline>(*m_textLayoutEngine);
        if (!m_textPipeline->create(*m_context)) return false;
    }

    m_imagePipeline = std::make_unique<ImagePipeline>();
    if (!m_imagePipeline->create(*m_context)) return false;

    return true;
}

void VkRenderer::cleanupPipelines() {
    if (m_imagePipeline) { m_imagePipeline->cleanup(*m_context); m_imagePipeline.reset(); }
    if (m_textPipeline) { m_textPipeline->cleanup(*m_context); m_textPipeline.reset(); }
    if (m_circlePipeline) { m_circlePipeline->cleanup(*m_context); m_circlePipeline.reset(); }
    if (m_roundedRectPipeline) { m_roundedRectPipeline->cleanup(*m_context); m_roundedRectPipeline.reset(); }
    if (m_rectPipeline) { m_rectPipeline->cleanup(*m_context); m_rectPipeline.reset(); }
}

// ============================================================================
// IRenderer - Setup
// ============================================================================

void VkRenderer::setClearColor(float r, float g, float b, float a) {
    if (m_context) m_context->setBackgroundColor(r, g, b, a);
}

void VkRenderer::setResourceManager(void* resourceManager) {
    m_resourceManager = static_cast<NativeResourceManager*>(resourceManager);
}

bool VkRenderer::initializeFonts(void* resourceManager, const std::string& fontName) {
    return initializeFonts(static_cast<NativeResourceManager*>(resourceManager), fontName);
}

bool VkRenderer::initializeFonts(NativeResourceManager* resourceManager, const std::string& fontName) {
    if (!m_textLayoutEngine) return false;
    if (!m_textLayoutEngine->initialize(resourceManager, fontName)) {
        return false;
    }
    if (m_textLayoutEngine->getFontManager()) {
        m_textLayoutManager = std::make_unique<TextLayoutManager>(*m_textLayoutEngine->getFontManager());
    }
    // Lazy-create text pipeline now that TextLayoutEngine is initialized
    if (!m_textPipeline) {
        m_textPipeline = std::make_unique<TextPipeline>(*m_textLayoutEngine);
        if (!m_textPipeline->create(*m_context)) {
            LOGW("Failed to create text pipeline during font initialization");
        }
    }
    return true;
}

// ============================================================================
// IRenderer - Queries
// ============================================================================

int VkRenderer::getWidth() const { return m_context ? m_context->getWidth() : 0; }
int VkRenderer::getHeight() const { return m_context ? m_context->getHeight() : 0; }
Core::GraphicsAPI VkRenderer::getAPI() const { return Core::GraphicsAPI::Vulkan; }
void VkRenderer::waitIdle() { if (m_context) m_context->waitIdle(); }
void VkRenderer::updateSurfaceSize(int width, int height) { if (m_context) m_context->updateSwapChainExtent(width, height); }
void VkRenderer::setCoordinateMapping(int width, int height) {
    if (m_context) {
        m_context->setCoordinateMapping(width, height);
        VkExtent2D extent = m_context->getSwapChainExtent();
        m_coordMapper = Core::CoordinateMapper(
            static_cast<float>(extent.width), static_cast<float>(extent.height),
            static_cast<float>(width), static_cast<float>(height));
    }
}

// ============================================================================
// Swap Chain Recreation
// ============================================================================

void VkRenderer::recreateSwapChain() {
    if (!m_context) return;
    VkDevice device = m_context->getDevice();

    // Invalidate stream text GPU buffers (NDC coords are swapchain-dependent)
    clearTextLayoutCache();
    m_textureCache->clear(device);
    cleanupPipelines();
    m_glassEffect->cleanup(device);

    m_context->recreateSwapChain();

    // Update coordinate mapper with new swapchain dimensions
    VkExtent2D extent = m_context->getSwapChainExtent();
    m_coordMapper = Core::CoordinateMapper(
        static_cast<float>(extent.width), static_cast<float>(extent.height),
        static_cast<float>(m_context->getWidth()), static_cast<float>(m_context->getHeight()));

    createPipelines();
}

// ============================================================================
// Draw Primitives
// ============================================================================

void VkRenderer::drawRect(const glm::vec2& position, const glm::vec2& size, const glm::vec3& color) {
    if (!m_rectPipeline || m_rectPipeline->getPipeline() == VK_NULL_HANDLE) return;

    Core::DrawPacket pkt;
    Core::GeometryBuilder::buildRect(pkt, position, size, color, m_coordMapper);
    submitDraw(pkt, m_rectPipeline->getPipeline(), m_rectPipeline->getLayout());
}

void VkRenderer::drawRoundedRect(const glm::vec2& position, const glm::vec2& size,
                                   float cornerRadius, const glm::vec3& color, float alpha) {
    drawRoundedRectImpl(position, size, cornerRadius, color, false, alpha);
}

void VkRenderer::drawGlassRoundedRect(const glm::vec2& position, const glm::vec2& size,
                                        float cornerRadius, const glm::vec3& color) {
    drawRoundedRectImpl(position, size, cornerRadius, color, true);
}

void VkRenderer::drawPolygon(const glm::vec2& center, const std::vector<glm::vec2>& perimeter,
                              const glm::vec3& color, float alpha) {
    if (!m_rectPipeline || m_rectPipeline->getPipeline() == VK_NULL_HANDLE) return;
    if (perimeter.size() < 3) return;

    Core::DrawPacket pkt;
    Core::GeometryBuilder::buildPolygon(pkt, m_scratchVerts, m_scratchIndices,
                                         center, perimeter, color, alpha, m_coordMapper);
    submitDraw(pkt, m_rectPipeline->getPipeline(), m_rectPipeline->getLayout());
}

void VkRenderer::drawCircle(const glm::vec2& center, float radius,
                              const glm::vec3& color, float alpha) {
    if (!m_circlePipeline || m_circlePipeline->getPipeline() == VK_NULL_HANDLE) return;

    updateIdentityMVP("circlePipeline");

    Core::DrawPacket pkt;
    Core::GeometryBuilder::buildCircle(pkt, center, radius, color, alpha, m_coordMapper);
    submitDraw(pkt, m_circlePipeline->getPipeline(), m_circlePipeline->getLayout(),
               m_circlePipeline->getDescriptorSet());
}

void VkRenderer::drawRoundedRectImpl(const glm::vec2& position, const glm::vec2& size,
                                       float cornerRadius, const glm::vec3& color,
                                       bool glass, float alpha) {
    if (!m_roundedRectPipeline || m_roundedRectPipeline->getPipeline() == VK_NULL_HANDLE) return;

    updateIdentityMVP("roundedRectPipeline");

    Core::DrawPacket pkt;
    Core::GeometryBuilder::buildRoundedRect(pkt, position, size, cornerRadius, color, alpha, glass, m_coordMapper);
    submitDraw(pkt, m_roundedRectPipeline->getPipeline(), m_roundedRectPipeline->getLayout(),
               m_roundedRectPipeline->getDescriptorSet());
}

void VkRenderer::drawText(const std::string& text, const glm::vec2& position,
                           uint32_t fontSize, const glm::vec3& color,
                           const std::string& fontFamily) {
    if (!m_textPipeline || !m_textLayoutEngine || !m_textLayoutEngine->isInitialized()) return;

    // Resolve fontFamily name (HarmonyOS names, CSS generics, etc.) to registered font ID
    FontManager* fm = m_textLayoutEngine->getFontManager();
    std::string resolvedFontId = "default";
    if (!fontFamily.empty() && fontFamily != "default" && fm) {
        resolvedFontId = fm->resolveFontFamily(fontFamily);
    }

    // Switch active font if a specific family is requested
    if (fm && resolvedFontId != "default") {
        fm->setActiveFont(resolvedFontId);
    }

    float ndcX, ndcY;
    m_context->pixelToNDC(position.x, position.y, ndcX, ndcY);
    VkExtent2D extent = m_context->getSwapChainExtent();

    float swapW = static_cast<float>(extent.width), swapH = static_cast<float>(extent.height);
    float refW = m_context->getWidth() > 0 ? m_context->getWidth() : swapW;
    float refH = m_context->getHeight() > 0 ? m_context->getHeight() : swapH;
    float scale = (refW / refH < swapW / swapH) ? (swapH / refH) : (swapW / refW);
    uint32_t scaledFontSize = static_cast<uint32_t>(fontSize * scale);

    m_textLayoutEngine->setGlowIntensity(m_glowIntensity);
    m_textLayoutEngine->setGlowWidth(m_glowWidth * scale);
    m_textLayoutEngine->setTextGradient(m_hasGradient, m_gradientEndColor, m_gradientDirection);
    m_textLayoutEngine->setTextStroke(m_strokeWidth * scale, m_strokeColor);
    m_textLayoutEngine->setNormalizationFactors(2.0f / static_cast<float>(extent.width),
                                                 2.0f / static_cast<float>(extent.height));

    m_textPipeline->drawText(*m_context, text, glm::vec2(ndcX, ndcY), scaledFontSize, color);

    // Reset to default font after drawing
    if (fm && resolvedFontId != "default") {
        fm->setActiveFont("default");
    }

    m_context->bindPipelineIfChanged(VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void VkRenderer::setTextGlow(float glowWidth, float glowIntensity) {
    m_glowWidth = glowWidth;
    m_glowIntensity = glowIntensity;
}

void VkRenderer::setTextGradient(bool hasGradient, const glm::vec3& endColor, int direction) {
    m_hasGradient = hasGradient;
    m_gradientEndColor = endColor;
    m_gradientDirection = direction;
}

void VkRenderer::setTextStroke(float width, const glm::vec3& color) {
    m_strokeWidth = width;
    m_strokeColor = color;
}

void VkRenderer::drawMultiLineText(const std::string& text, const glm::vec2& position,
                                     uint32_t fontSize, const glm::vec3& color,
                                     float maxWidth, float lineHeight) {
    if (!m_textPipeline || !m_textLayoutEngine || !m_textLayoutEngine->isInitialized()) return;

    float ndcX, ndcY;
    m_context->pixelToNDC(position.x, position.y, ndcX, ndcY);
    VkExtent2D extent = m_context->getSwapChainExtent();

    float swapW = static_cast<float>(extent.width), swapH = static_cast<float>(extent.height);
    float refW = m_context->getWidth() > 0 ? m_context->getWidth() : swapW;
    float refH = m_context->getHeight() > 0 ? m_context->getHeight() : swapH;
    float scale = (refW / refH < swapW / swapH) ? (swapH / refH) : (swapW / refW);
    uint32_t scaledFontSize = static_cast<uint32_t>(fontSize * scale);
    float scaledMaxWidth = maxWidth * scale;
    float scaledLineHeight = lineHeight * scale;

    m_textLayoutEngine->setNormalizationFactors(2.0f / static_cast<float>(extent.width),
                                                 2.0f / static_cast<float>(extent.height));

    if (m_textLayoutManager) {
        PreparedTextWithSegments* cachedPrepared = nullptr;
        LayoutLinesResult* cachedLayout = nullptr;
        m_textLayoutManager->prepareAndLayoutCached(text, fontSize, maxWidth, lineHeight, cachedPrepared, cachedLayout);

        if (cachedPrepared && cachedLayout) {
            m_textPipeline->drawTextBlock(*m_context, *cachedPrepared, *cachedLayout, ndcX, ndcY,
                scaledMaxWidth, scaledLineHeight, scaledFontSize, color);
            m_context->bindPipelineIfChanged(VK_NULL_HANDLE, VK_NULL_HANDLE);
            return;
        }

        std::u32string text32;
        text32.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            char32_t cp = 0;
            unsigned char c = static_cast<unsigned char>(text[i]);
            if (c < 0x80) { cp = c; i += 1; }
            else if ((c & 0xE0) == 0xC0) { cp = (c & 0x1F) << 6; cp |= (static_cast<unsigned char>(text[i+1]) & 0x3F); i += 2; }
            else if ((c & 0xF0) == 0xE0) { cp = (c & 0x0F) << 12; cp |= (static_cast<unsigned char>(text[i+1]) & 0x3F) << 6; cp |= (static_cast<unsigned char>(text[i+2]) & 0x3F); i += 3; }
            else if ((c & 0xF8) == 0xF0) { cp = (c & 0x07) << 18; cp |= (static_cast<unsigned char>(text[i+1]) & 0x3F) << 12; cp |= (static_cast<unsigned char>(text[i+2]) & 0x3F) << 6; cp |= (static_cast<unsigned char>(text[i+3]) & 0x3F); i += 4; }
            else { i++; continue; }
            text32.push_back(cp);
        }
        auto prepared = m_textLayoutManager->prepareWithSegments(text32, static_cast<float>(fontSize));
        if (prepared) {
            m_textPipeline->drawTextBlock(*m_context, *m_textLayoutManager, *prepared, ndcX, ndcY,
                scaledMaxWidth, scaledLineHeight, scaledFontSize, color);
        }
        m_context->bindPipelineIfChanged(VK_NULL_HANDLE, VK_NULL_HANDLE);
    }
}

void VkRenderer::drawImage(const std::string& imagePath, const glm::vec2& position,
                            const glm::vec2& size, float rotation,
                            const std::vector<glm::vec2>& clipVertices,
                            const glm::vec2& clipCenter) {
    if (!m_imagePipeline || m_imagePipeline->getPipeline() == VK_NULL_HANDLE) return;

    VkDescriptorSet descriptorSet = m_textureCache->getOrLoad(*m_context, *m_imagePipeline, imagePath
        , m_resourceManager
    );
    if (descriptorSet == VK_NULL_HANDLE) return;

    Core::DrawPacket pkt;
    Core::GeometryBuilder::buildImage(pkt, m_scratchVerts, m_scratchIndices,
                                       position, size, rotation, clipVertices, clipCenter, m_coordMapper);
    submitDraw(pkt, m_imagePipeline->getPipeline(), m_imagePipeline->getLayout(), descriptorSet);
}

// ============================================================================
// Unified Draw Submission
// ============================================================================

void VkRenderer::updateIdentityMVP(const char* pipelineName) {
    float mvpData[16] = {};
    mvpData[0] = 1.0f; mvpData[5] = 1.0f; mvpData[10] = 1.0f; mvpData[15] = 1.0f;
    auto* dm = m_context->getDescriptorManager();
    if (dm) dm->updateUniformBuffer(pipelineName, mvpData, sizeof(mvpData));
}

void VkRenderer::submitDraw(const Core::DrawPacket& pkt, VkPipeline pipeline, VkPipelineLayout layout,
                              VkDescriptorSet descriptorSet) {
    // Glass path: manual Vulkan calls to bind background descriptor set
    if (pkt.glass) {
        VkCommandBuffer cmdBuf = m_context->getCurrentCommandBuffer();
        if (cmdBuf == VK_NULL_HANDLE) return;
        auto* bufferPool = m_context->getVertexBufferPool();
        if (!bufferPool || !bufferPool->isInitialized()) return;

        m_context->bindPipelineIfChanged(pipeline, layout);

        VkDescriptorSet rrDescSet = descriptorSet;
        if (rrDescSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    layout, 0, 1, &rrDescSet, 0, nullptr);
        }
        VkDescriptorSet bgDescSet = m_glassEffect->getBgDescriptorSet();
        if (bgDescSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    layout, 1, 1, &bgDescSet, 0, nullptr);
        }

        vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, pkt.pcFloatCount * sizeof(float), pkt.pushConstants);

        uint32_t frame = m_context->getCurrentFrame();
        auto vertexAlloc = bufferPool->allocateVertexData(
            pkt.vertexData(), pkt.vertFloatCount * sizeof(float), frame);
        if (vertexAlloc.buffer == VK_NULL_HANDLE) return;
        auto indexAlloc = bufferPool->allocateIndexData(
            pkt.indexData(), pkt.indexCount * sizeof(uint16_t), frame);
        if (indexAlloc.buffer == VK_NULL_HANDLE) return;

        VkDeviceSize vertexOffset = vertexAlloc.offset;
        vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexAlloc.buffer, &vertexOffset);
        vkCmdBindIndexBuffer(cmdBuf, indexAlloc.buffer, indexAlloc.offset, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmdBuf, pkt.indexCount, 1, 0, 0, 0);
        return;
    }

    // Standard path: BGRA swap for Position3Color4, then drawWithTempBuffers
    if (pkt.format == Core::VertexFormat::Position3Color4) {
        float* v = const_cast<float*>(pkt.vertexData());
        for (uint32_t i = 0; i < pkt.vertFloatCount; i += pkt.vertStride) {
            std::swap(v[i + 3], v[i + 5]);  // swap R and B
        }
    }

    const float* pushConstPtr = pkt.pcFloatCount > 0 ? pkt.pushConstants : nullptr;
    uint32_t pushConstSize = pkt.pcFloatCount * sizeof(float);

    m_context->drawWithTempBuffers(pkt.vertexData(), pkt.vertFloatCount,
                        pkt.indexData(), pkt.indexCount,
                        pipeline, layout,
                        pushConstPtr, pushConstSize,
                        descriptorSet);
}

// ============================================================================
// Glass Effect
// ============================================================================

void VkRenderer::prepareGlassPass() {
    m_glassEffect->preparePass(*m_context, m_imagePipeline.get());
}

FontManager* VkRenderer::getFontManager() {
    return m_textLayoutEngine ? m_textLayoutEngine->getFontManager() : nullptr;
}

void VkRenderer::precacheText(const std::string& text, uint32_t fontSize) {
    if (m_textLayoutEngine && m_textLayoutEngine->isInitialized()) {
        m_textLayoutEngine->precacheText(text, fontSize);
    }
}

// ============================================================================
// Stream Text Support (Typewriter Effect)
// ============================================================================

uint32_t VkRenderer::prepareTextLayout(const std::string& key, const std::string& text,
                                         const glm::vec2& position, uint32_t fontSize,
                                         const std::string& fontFamily,
                                         float maxWidth) {
    if (!m_textPipeline || !m_textLayoutEngine || !m_textLayoutEngine->isInitialized()) return 0;
    if (text.empty()) return 0;

    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:PrepareLayout", "");

    // Resolve font family
    FontManager* fm = m_textLayoutEngine->getFontManager();
    std::string resolvedFontId = "default";
    if (!fontFamily.empty() && fontFamily != "default" && fm) {
        resolvedFontId = fm->resolveFontFamily(fontFamily);
    }

    // NDC conversion and scaling
    float ndcX, ndcY;
    m_context->pixelToNDC(position.x, position.y, ndcX, ndcY);
    VkExtent2D extent = m_context->getSwapChainExtent();
    float swapW = static_cast<float>(extent.width), swapH = static_cast<float>(extent.height);
    float refW = m_context->getWidth() > 0 ? m_context->getWidth() : swapW;
    float refH = m_context->getHeight() > 0 ? m_context->getHeight() : swapH;
    float scale = (refW / refH < swapW / swapH) ? (swapH / refH) : (swapW / refW);
    uint32_t scaledFontSize = static_cast<uint32_t>(fontSize * scale);

    float normX = 2.0f / static_cast<float>(extent.width);
    float normY = 2.0f / static_cast<float>(extent.height);
    m_textLayoutEngine->setNormalizationFactors(normX, normY);

    // Delegate pure CPU layout to TextLayoutEngine
    auto result = m_textLayoutEngine->layoutPlainText(
        text, scaledFontSize, maxWidth, scale, resolvedFontId);

    if (!result || !result->prepared) {
        OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
        return 0;
    }

    // Build VkRenderer cache from result
    VkTextLayoutCache cache;
    cache.pixelPos = position;               // CPU layer: pixel coordinates
    cache.ndcPos = glm::vec2(ndcX, ndcY);    // GPU layer: NDC coordinates
    cache.totalGlyphs = result->totalGlyphs;
    cache.prepared = std::move(result->prepared);
    cache.layout = std::move(result->layout);
    auto oldIt = m_textLayoutCache.find(key);
    bool reusedOldBuffers = false;

    // Upload glyph atlas to GPU before building buffers — without this,
    // drawTextPartial bypasses the normal drawText atlas upload path and
    // glyphs are invisible (especially when no other drawText call exists).
    m_textPipeline->uploadPendingAtlases(*m_context);

    // ---- Plan B: Pre-build full GPU vertex/index buffers ----
    if (cache.totalGlyphs > 0 && cache.prepared && cache.layout) {
        // Build full draw data once (with dummy white color; color overwritten per-draw)
        auto drawData = m_textLayoutEngine->buildDrawData(
            *cache.prepared, *cache.layout, cache.ndcPos, glm::vec3(1.0f));

        if (!drawData.vertices.empty() && !drawData.indices.empty()) {
            VkDevice device = m_context->getDevice();
            VkPhysicalDevice physDevice = m_context->getPhysicalDevice();

            if (oldIt != m_textLayoutCache.end()) {
                cache.gpuVertexBuffer = oldIt->second.gpuVertexBuffer;
                cache.gpuVertexMemory = oldIt->second.gpuVertexMemory;
                cache.gpuVertexCapacity = oldIt->second.gpuVertexCapacity;
                cache.gpuIndexBuffer = oldIt->second.gpuIndexBuffer;
                cache.gpuIndexMemory = oldIt->second.gpuIndexMemory;
                cache.gpuIndexCapacity = oldIt->second.gpuIndexCapacity;
                reusedOldBuffers = true;
            }

            bool vbOk = uploadStreamBuffer(device, physDevice,
                drawData.vertices.data(), drawData.vertices.size() * sizeof(float),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                cache.gpuVertexBuffer, cache.gpuVertexMemory, cache.gpuVertexCapacity);

            bool ibOk = false;
            if (vbOk) {
                ibOk = uploadStreamBuffer(device, physDevice,
                    drawData.indices.data(), drawData.indices.size() * sizeof(uint16_t),
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    cache.gpuIndexBuffer, cache.gpuIndexMemory, cache.gpuIndexCapacity);
            }

            if (vbOk && ibOk) {
                cache.totalIndexCount = static_cast<uint32_t>(drawData.indices.size());
                // Resolve pipeline handles for this render path
                auto handles = m_textPipeline->getPipelineHandles(drawData.renderPath, *m_context);
                cache.cachedPipeline = handles.pipeline;
                cache.cachedDescriptorSet = handles.descriptorSet;
                cache.cachedPipelineLayout = handles.layout;
                memcpy(cache.cachedPushConstants, drawData.pushConstants, sizeof(drawData.pushConstants));
            } else {
                LOGE("StreamText: Failed to create GPU buffers for key '%s'", key.c_str());
                releaseLayoutGpuResources(device, cache);
                if (oldIt != m_textLayoutCache.end() && reusedOldBuffers) {
                    oldIt->second.gpuVertexBuffer = VK_NULL_HANDLE;
                    oldIt->second.gpuVertexMemory = VK_NULL_HANDLE;
                    oldIt->second.gpuVertexCapacity = 0;
                    oldIt->second.gpuIndexBuffer = VK_NULL_HANDLE;
                    oldIt->second.gpuIndexMemory = VK_NULL_HANDLE;
                    oldIt->second.gpuIndexCapacity = 0;
                }
                reusedOldBuffers = false;
            }
        }
    }

    // Release old entry's GPU buffers if key already exists
    if (oldIt != m_textLayoutCache.end() && m_context && !reusedOldBuffers) {
        releaseLayoutGpuResources(m_context->getDevice(), oldIt->second);
    } else if (oldIt != m_textLayoutCache.end() && reusedOldBuffers) {
        oldIt->second.gpuVertexBuffer = VK_NULL_HANDLE;
        oldIt->second.gpuVertexMemory = VK_NULL_HANDLE;
        oldIt->second.gpuVertexCapacity = 0;
        oldIt->second.gpuIndexBuffer = VK_NULL_HANDLE;
        oldIt->second.gpuIndexMemory = VK_NULL_HANDLE;
        oldIt->second.gpuIndexCapacity = 0;
    }

    uint32_t totalGlyphs = cache.totalGlyphs;
    m_textLayoutCache[key] = std::move(cache);

    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);

    return totalGlyphs;
}

void VkRenderer::drawTextPartial(const std::string& key, const glm::vec3& color,
                                   uint32_t visibleChars) {
    if (!m_textPipeline) return;

    auto it = m_textLayoutCache.find(key);
    if (it == m_textLayoutCache.end()) return;

    const auto& cache = it->second;
    if (visibleChars == 0 || cache.totalGlyphs == 0) return;
    if (cache.gpuVertexBuffer == VK_NULL_HANDLE) return;

    // Route to multi-style path if styled segments exist
    if (!cache.segments.empty()) {
        // Draw backgrounds and HR lines before text (respecting stream visibility)
        for (const auto& bg : cache.backgrounds) {
            // Skip backgrounds whose characters are not yet visible
            if (visibleChars <= bg.startCharIndex) continue;

            // For stream-gated backgrounds, scale width to visible portion
            glm::vec2 drawSize = bg.size;
            if (bg.charCount < UINT32_MAX) {
                uint32_t bgEnd = bg.startCharIndex + bg.charCount;
                if (visibleChars < bgEnd) {
                    float ratio = static_cast<float>(visibleChars - bg.startCharIndex) /
                                  static_cast<float>(bg.charCount);
                    drawSize.x = bg.size.x * ratio;
                }
            }
            drawRoundedRect(bg.pos, drawSize, bg.radius, bg.color);
        }
        drawStyledTextPartial(cache, color, visibleChars);
        return;
    }

    uint32_t count = std::min(visibleChars, cache.totalGlyphs);
    uint32_t indexCount = count * 6;  // 6 indices per glyph quad

    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:DrawPartial", "");
    OH_HiTrace_CountTraceEx(HITRACE_LEVEL_INFO, "AgenUI:VisibleGlyphs",
                            static_cast<int64_t>(count));

    VkCommandBuffer cmdBuf = m_context->getCurrentCommandBuffer();

    // Bind pipeline + descriptor set (O(1) — handles from cache)
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, cache.cachedPipeline);
    if (cache.cachedDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                cache.cachedPipelineLayout, 0, 1,
                                &cache.cachedDescriptorSet, 0, nullptr);
    }

    // Overwrite color in cached push constants (BGR layout)
    float pc[32];
    memcpy(pc, cache.cachedPushConstants, sizeof(pc));
    pc[16] = color.b;
    pc[17] = color.g;
    pc[18] = color.r;

    vkCmdPushConstants(cmdBuf, cache.cachedPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), pc);

    // Bind pre-built buffers (O(1) — no data copy!)
    VkDeviceSize vbOffset = 0;
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &cache.gpuVertexBuffer, &vbOffset);
    vkCmdBindIndexBuffer(cmdBuf, cache.gpuIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

    // Draw sub-range only
    vkCmdDrawIndexed(cmdBuf, indexCount, 1, 0, 0, 0);

    m_context->bindPipelineIfChanged(VK_NULL_HANDLE, VK_NULL_HANDLE);

    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
}

// ============================================================================
// Styled Text Layout (multi-font, multi-style) — accepts pre-parsed TextBlocks
// ============================================================================

uint32_t VkRenderer::prepareTextLayout(const std::string& key,
                                         const std::vector<Core::TextBlock>& blocks,
                                         const glm::vec2& position, uint32_t baseFontSize,
                                         float maxWidth) {
    if (!m_textPipeline || !m_textLayoutEngine || !m_textLayoutEngine->isInitialized()) {
        LOGE("prepareTextLayout(styled): early return — pipeline=%p engine=%p",
             m_textPipeline.get(), m_textLayoutEngine.get());
        return 0;
    }
    if (blocks.empty()) {
        LOGE("prepareTextLayout(styled): empty blocks");
        return 0;
    }

    LOGD("[Styled] prepareTextLayout key='%s' fontSize=%u maxWidth=%.0f blocks=%zu pos=(%.0f,%.0f)",
         key.c_str(), baseFontSize, maxWidth, blocks.size(), position.x, position.y);

    // 1. Compute NDC/scale (VulkanContext-dependent)
    float ndcX, ndcY;
    m_context->pixelToNDC(position.x, position.y, ndcX, ndcY);
    VkExtent2D extent = m_context->getSwapChainExtent();
    float swapW = static_cast<float>(extent.width), swapH = static_cast<float>(extent.height);
    float refW = m_context->getWidth() > 0 ? m_context->getWidth() : swapW;
    float refH = m_context->getHeight() > 0 ? m_context->getHeight() : swapH;
    float scale = (refW / refH < swapW / swapH) ? (swapH / refH) : (swapW / refW);

    float normX = 2.0f / static_cast<float>(extent.width);
    float normY = 2.0f / static_cast<float>(extent.height);
    m_textLayoutEngine->setNormalizationFactors(normX, normY);

    // 2. CPU layout (most expensive step)
    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:StyledCPULayout", "");
    auto result = m_textLayoutEngine->layoutStyledBlocks(
        blocks, baseFontSize, maxWidth,
        glm::vec2(ndcX, ndcY), normX, normY, scale);
    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);

    if (!result) {
        return 0;
    }

    // 3. Convert GPU-agnostic result to VkRenderer-specific cache
    VkTextLayoutCache cache;
    cache.pixelPos = position;               // CPU layer: pixel coordinates
    cache.ndcPos = glm::vec2(ndcX, ndcY);    // GPU layer: NDC coordinates
    cache.totalGlyphs = result->totalGlyphs;
    cache.prepared = std::move(result->firstPrepared);
    cache.layout = std::move(result->combinedLayout);
    auto oldIt = m_textLayoutCache.find(key);
    bool reusedOldBuffers = false;

    for (auto& seg : result->segments) {
        // CPU data → Core::CoreSegment (no GPU shader data)
        Core::CoreSegment ss;
        ss.startCharIndex = seg.startCharIndex;
        ss.charCount = seg.charCount;
        ss.indexOffset = seg.indexOffset;
        ss.indexCount = seg.indexCount;
        ss.color = seg.color;
        ss.renderPath = seg.renderPath;
        ss.fontId = seg.fontId;
        cache.segments.push_back(ss);

        // GPU handles + push constants → VkSegmentData (parallel vector)
        auto handles = m_textPipeline->getPipelineHandles(seg.renderPath, *m_context);
        VkTextLayoutCache::VkSegmentData vd;
        memcpy(vd.pushConstants, seg.pushConstants, sizeof(vd.pushConstants));
        vd.pipeline = handles.pipeline;
        vd.descriptorSet = handles.descriptorSet;
        vd.layout = handles.layout;
        cache.vkSegments.push_back(vd);
    }

    // Copy decorative rects directly (same type: DecorativeRect)
    cache.backgrounds = std::move(result->decorations);

    // 4. Upload glyph atlas to GPU
    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:StyledUploadAtlas", "");
    m_textPipeline->uploadPendingAtlases(*m_context);
    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);

    bool appendedGpuData = false;

    if (oldIt != m_textLayoutCache.end() && oldIt->second.gpuVertexBuffer != VK_NULL_HANDLE &&
        oldIt->second.gpuIndexBuffer != VK_NULL_HANDLE && oldIt->second.layout &&
        result->combinedLayout && result->firstPrepared && oldIt->second.backgrounds.empty() &&
        result->decorations.empty() && oldIt->second.segments.size() == result->segments.size()) {
        const auto& oldCache = oldIt->second;
        const size_t oldGlyphCount = oldCache.layout->glyphs.size();
        const size_t newGlyphCount = result->combinedLayout->glyphs.size();
        bool prefixSame = newGlyphCount > oldGlyphCount;
        for (size_t i = 0; prefixSame && i < oldGlyphCount; i++) {
            const auto& a = oldCache.layout->glyphs[i];
            const auto& b = result->combinedLayout->glyphs[i];
            prefixSame = a.x0 == b.x0 && a.y0 == b.y0 && a.x1 == b.x1 && a.y1 == b.y1 &&
                         a.u0 == b.u0 && a.v0 == b.v0 && a.u1 == b.u1 && a.v1 == b.v1;
        }
        for (size_t i = 0; prefixSame && i < result->segments.size(); i++) {
            const auto& oldSeg = oldCache.segments[i];
            const auto& oldVd = oldCache.vkSegments[i];
            const auto& newSeg = result->segments[i];
            auto handles = m_textPipeline->getPipelineHandles(newSeg.renderPath, *m_context);
            prefixSame = oldSeg.startCharIndex == newSeg.startCharIndex &&
                         oldSeg.indexOffset == newSeg.indexOffset &&
                         oldSeg.color.r == newSeg.color.r &&
                         oldSeg.color.g == newSeg.color.g &&
                         oldSeg.color.b == newSeg.color.b &&
                         oldVd.pipeline == handles.pipeline &&
                         oldVd.descriptorSet == handles.descriptorSet &&
                         oldVd.layout == handles.layout;
            if (i + 1 < result->segments.size()) {
                prefixSame = prefixSame && oldSeg.charCount == newSeg.charCount &&
                             oldSeg.indexCount == newSeg.indexCount;
            }
        }

        const size_t oldVertexBytes = oldGlyphCount * 32 * sizeof(float);
        const size_t oldIndexBytes = oldCache.totalIndexCount * sizeof(uint16_t);
        TextLayoutResult suffixLayout;
        if (prefixSame) {
            suffixLayout.glyphs.assign(result->combinedLayout->glyphs.begin() + oldGlyphCount,
                                       result->combinedLayout->glyphs.end());
        }
        if (prefixSame && !suffixLayout.glyphs.empty()) {
            PreparedGlyphRun suffixRun;
            suffixRun.renderPath = result->firstPrepared->renderPath;
            suffixRun.fontSize = result->firstPrepared->fontSize;
            auto suffixDrawData = m_textLayoutEngine->buildDrawData(
                suffixRun, suffixLayout, cache.ndcPos, glm::vec3(1.0f));
            const uint16_t indexBase = static_cast<uint16_t>(oldGlyphCount * 4);
            for (auto& index : suffixDrawData.indices) {
                index = static_cast<uint16_t>(index + indexBase);
            }

            const size_t suffixVertexBytes = suffixDrawData.vertices.size() * sizeof(float);
            const size_t suffixIndexBytes = suffixDrawData.indices.size() * sizeof(uint16_t);
            if (!suffixDrawData.vertices.empty() && !suffixDrawData.indices.empty() &&
                oldCache.gpuVertexCapacity >= oldVertexBytes + suffixVertexBytes &&
                oldCache.gpuIndexCapacity >= oldIndexBytes + suffixIndexBytes) {
                cache.gpuVertexBuffer = oldCache.gpuVertexBuffer;
                cache.gpuVertexMemory = oldCache.gpuVertexMemory;
                cache.gpuVertexCapacity = oldCache.gpuVertexCapacity;
                cache.gpuIndexBuffer = oldCache.gpuIndexBuffer;
                cache.gpuIndexMemory = oldCache.gpuIndexMemory;
                cache.gpuIndexCapacity = oldCache.gpuIndexCapacity;
                reusedOldBuffers = true;
                bool vbOk = uploadStreamBufferAt(m_context->getDevice(), cache.gpuVertexMemory,
                    oldVertexBytes, suffixDrawData.vertices.data(), suffixVertexBytes);
                bool ibOk = uploadStreamBufferAt(m_context->getDevice(), cache.gpuIndexMemory,
                    oldIndexBytes, suffixDrawData.indices.data(), suffixIndexBytes);
                if (vbOk && ibOk) {
                    cache.totalIndexCount = static_cast<uint32_t>(
                        oldCache.totalIndexCount + suffixDrawData.indices.size());
                    appendedGpuData = true;
                } else {
                    reusedOldBuffers = false;
                    cache.gpuVertexBuffer = VK_NULL_HANDLE;
                    cache.gpuVertexMemory = VK_NULL_HANDLE;
                    cache.gpuVertexCapacity = 0;
                    cache.gpuIndexBuffer = VK_NULL_HANDLE;
                    cache.gpuIndexMemory = VK_NULL_HANDLE;
                    cache.gpuIndexCapacity = 0;
                }
            }
        }
    }

    // 5. Build GPU data (vertex/index buffers)
    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:StyledBuildGPU", "");
    if (!appendedGpuData && result->totalGlyphs > 0 && cache.prepared && cache.layout) {
        auto drawData = m_textLayoutEngine->buildDrawData(
            *cache.prepared, *cache.layout, cache.ndcPos, glm::vec3(1.0f));

        LOGD("[Styled] buildDrawData: verts=%zu indices=%zu renderPath=%d",
             drawData.vertices.size(), drawData.indices.size(),
             static_cast<int>(drawData.renderPath));

        if (!drawData.vertices.empty() && !drawData.indices.empty()) {
            VkDevice device = m_context->getDevice();
            VkPhysicalDevice physDevice = m_context->getPhysicalDevice();

            if (oldIt != m_textLayoutCache.end()) {
                cache.gpuVertexBuffer = oldIt->second.gpuVertexBuffer;
                cache.gpuVertexMemory = oldIt->second.gpuVertexMemory;
                cache.gpuVertexCapacity = oldIt->second.gpuVertexCapacity;
                cache.gpuIndexBuffer = oldIt->second.gpuIndexBuffer;
                cache.gpuIndexMemory = oldIt->second.gpuIndexMemory;
                cache.gpuIndexCapacity = oldIt->second.gpuIndexCapacity;
                reusedOldBuffers = true;
            }

            bool vbOk = uploadStreamBuffer(device, physDevice,
                drawData.vertices.data(), drawData.vertices.size() * sizeof(float),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                cache.gpuVertexBuffer, cache.gpuVertexMemory, cache.gpuVertexCapacity);

            bool ibOk = false;
            if (vbOk) {
                ibOk = uploadStreamBuffer(device, physDevice,
                    drawData.indices.data(), drawData.indices.size() * sizeof(uint16_t),
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    cache.gpuIndexBuffer, cache.gpuIndexMemory, cache.gpuIndexCapacity);
            }

            if (vbOk && ibOk) {
                cache.totalIndexCount = static_cast<uint32_t>(drawData.indices.size());
            } else {
                LOGE("StyledText: Failed to create GPU buffers for key '%s'", key.c_str());
                releaseLayoutGpuResources(device, cache);
                if (oldIt != m_textLayoutCache.end() && reusedOldBuffers) {
                    oldIt->second.gpuVertexBuffer = VK_NULL_HANDLE;
                    oldIt->second.gpuVertexMemory = VK_NULL_HANDLE;
                    oldIt->second.gpuVertexCapacity = 0;
                    oldIt->second.gpuIndexBuffer = VK_NULL_HANDLE;
                    oldIt->second.gpuIndexMemory = VK_NULL_HANDLE;
                    oldIt->second.gpuIndexCapacity = 0;
                }
                reusedOldBuffers = false;
            }
        }
    }
    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);

    // Release old entry's GPU buffers if key exists
    if (oldIt != m_textLayoutCache.end() && m_context && !reusedOldBuffers) {
        releaseLayoutGpuResources(m_context->getDevice(), oldIt->second);
    } else if (oldIt != m_textLayoutCache.end() && reusedOldBuffers) {
        oldIt->second.gpuVertexBuffer = VK_NULL_HANDLE;
        oldIt->second.gpuVertexMemory = VK_NULL_HANDLE;
        oldIt->second.gpuVertexCapacity = 0;
        oldIt->second.gpuIndexBuffer = VK_NULL_HANDLE;
        oldIt->second.gpuIndexMemory = VK_NULL_HANDLE;
        oldIt->second.gpuIndexCapacity = 0;
    }

    m_textLayoutCache[key] = std::move(cache);

    LOGD("[Styled] DONE key='%s' totalGlyphs=%u segments=%zu",
         key.c_str(), result->totalGlyphs, m_textLayoutCache[key].segments.size());

    return result->totalGlyphs;
}

void VkRenderer::drawStyledTextPartial(const VkTextLayoutCache& cache, const glm::vec3& color,
                                         uint32_t visibleChars) {
    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:DrawStyledPartial", "");
    LOGD("[Styled] drawStyled visibleChars=%u totalGlyphs=%u segs=%zu vb=%p",
         visibleChars, cache.totalGlyphs, cache.segments.size(),
         (void*)cache.gpuVertexBuffer);
    OH_HiTrace_CountTraceEx(HITRACE_LEVEL_INFO, "AgenUI:VisibleChars",
                            static_cast<int64_t>(visibleChars));

    VkCommandBuffer cmdBuf = m_context->getCurrentCommandBuffer();

    VkDeviceSize vbOffset = 0;
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &cache.gpuVertexBuffer, &vbOffset);
    vkCmdBindIndexBuffer(cmdBuf, cache.gpuIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

    for (size_t i = 0; i < cache.segments.size(); i++) {
        const auto& seg = cache.segments[i];
        const auto& vd = cache.vkSegments[i];

        if (visibleChars <= seg.startCharIndex) break;

        uint32_t segVisibleChars = std::min(
            visibleChars - seg.startCharIndex, seg.charCount);
        if (segVisibleChars == 0) continue;

        uint32_t actualIndexCount = (seg.charCount > 0)
            ? (segVisibleChars * seg.indexCount / seg.charCount)
            : 0;
        actualIndexCount = std::min(actualIndexCount, seg.indexCount);
        if (actualIndexCount == 0) continue;

        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vd.pipeline);
        if (vd.descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vd.layout, 0, 1,
                                    &vd.descriptorSet, 0, nullptr);
        }

        float pc[32];
        memcpy(pc, vd.pushConstants, sizeof(pc));
        if (seg.color == glm::vec3(1.0f)) {
            pc[16] = color.b;
            pc[17] = color.g;
            pc[18] = color.r;
        }

        vkCmdPushConstants(cmdBuf, vd.layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), pc);

        vkCmdDrawIndexed(cmdBuf, actualIndexCount, 1, seg.indexOffset, 0, 0);
    }

    m_context->bindPipelineIfChanged(VK_NULL_HANDLE, VK_NULL_HANDLE);

    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
}


// ============================================================================
// Stream Text — GPU buffer helpers (Plan B)
// ============================================================================

bool VkRenderer::createStreamBuffer(VkDevice device, VkPhysicalDevice physDevice,
                                      const void* data, size_t size,
                                      VkBufferUsageFlags usage,
                                      ::VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &ci, nullptr, &buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer, &memReq);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReq.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device, buffer, memory, 0);
    void* mapped;
    vkMapMemory(device, memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, memory);
    return true;
}

bool VkRenderer::uploadStreamBuffer(VkDevice device, VkPhysicalDevice physDevice,
                                      const void* data, size_t size,
                                      VkBufferUsageFlags usage,
                                      ::VkBuffer& buffer, VkDeviceMemory& memory,
                                      size_t& capacity) {
    if (size == 0) return true;

    if (buffer != VK_NULL_HANDLE && memory != VK_NULL_HANDLE && capacity >= size) {
        void* mapped = nullptr;
        if (vkMapMemory(device, memory, 0, size, 0, &mapped) != VK_SUCCESS) {
            return false;
        }
        memcpy(mapped, data, size);
        vkUnmapMemory(device, memory);
        return true;
    }

    size_t oldCapacity = capacity;
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
    capacity = 0;

    size_t newCapacity = std::max(size, oldCapacity > 0 ? oldCapacity * 2 : size);
    newCapacity = std::max(newCapacity, static_cast<size_t>(4096));

    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = newCapacity;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &ci, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer, &memReq);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReq.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device, buffer, memory, 0);
    void* mapped = nullptr;
    if (vkMapMemory(device, memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }
    memcpy(mapped, data, size);
    vkUnmapMemory(device, memory);
    capacity = newCapacity;
    return true;
}

bool VkRenderer::uploadStreamBufferAt(VkDevice device, VkDeviceMemory memory,
                                        size_t offset, const void* data, size_t size) {
    if (size == 0) return true;
    if (device == VK_NULL_HANDLE || memory == VK_NULL_HANDLE || !data) return false;

    void* mapped = nullptr;
    if (vkMapMemory(device, memory, offset, size, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    memcpy(mapped, data, size);
    vkUnmapMemory(device, memory);
    return true;
}

void VkRenderer::releaseLayoutGpuResources(VkDevice device, VkTextLayoutCache& cache) {
    if (device == VK_NULL_HANDLE) return;
    if (cache.gpuVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, cache.gpuVertexBuffer, nullptr);
        cache.gpuVertexBuffer = VK_NULL_HANDLE;
    }
    if (cache.gpuVertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, cache.gpuVertexMemory, nullptr);
        cache.gpuVertexMemory = VK_NULL_HANDLE;
    }
    cache.gpuVertexCapacity = 0;
    if (cache.gpuIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, cache.gpuIndexBuffer, nullptr);
        cache.gpuIndexBuffer = VK_NULL_HANDLE;
    }
    if (cache.gpuIndexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, cache.gpuIndexMemory, nullptr);
        cache.gpuIndexMemory = VK_NULL_HANDLE;
    }
    cache.gpuIndexCapacity = 0;
    cache.totalIndexCount = 0;
}

void VkRenderer::clearTextLayoutCache() {
    if (m_context) {
        VkDevice device = m_context->getDevice();
        for (auto& [key, cache] : m_textLayoutCache) {
            releaseLayoutGpuResources(device, cache);
        }
    }
    m_textLayoutCache.clear();
}

// ============================================================================
// Batch Execution Hooks
// ============================================================================

void VkRenderer::onBeginBatch() {
    m_batchState = {};
}

void VkRenderer::onDrawTypeChange(Core::DrawType newType) {
    m_batchState.pipelineChanges++;
    if (newType == Core::DrawType::GlassRoundedRectangle) {
        prepareGlassPass();
    }
}

void VkRenderer::onEndBatch(Core::FrameStats& stats) {
    stats.batchedDrawCalls = m_batchState.pipelineChanges;
}

} // namespace AgenUIEngine
