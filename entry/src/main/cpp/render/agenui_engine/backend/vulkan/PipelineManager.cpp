#include "PipelineManager.h"
#include "VulkanContext.h"
#include "PipelineBuilder.h"
#include "DescriptorManager.h"
#include "modules/text/TextLayoutEngine.h"
#include "modules/text/MsdfAtlas.h"
#include "modules/text/text_layout/TextLayout.h"
#include "VkTexture.h"
#include "logger_common.h"
#include <cstring>
#include <fstream>

#if AGENUI_PLATFORM_HARMONYOS
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#endif

namespace AgenUIEngine {

namespace {
#if AGENUI_PLATFORM_HARMONYOS
    std::vector<char> loadRawFileSpv(NativeResourceManager* rm, const char* name) {
        std::vector<char> result;
        if (!rm) return result;
        RawFile* rawFile = OH_ResourceManager_OpenRawFile(rm, name);
        if (!rawFile) {
            LOGE("CirclePipeline: rawfile not found: %s", name);
            return result;
        }
        int fileSize = OH_ResourceManager_GetRawFileSize(rawFile);
        if (fileSize <= 0 || (fileSize % 4) != 0) {
            OH_ResourceManager_CloseRawFile(rawFile);
            return result;
        }
        result.resize(fileSize);
        int bytesRead = OH_ResourceManager_ReadRawFile(rawFile, result.data(), fileSize);
        OH_ResourceManager_CloseRawFile(rawFile);
        if (bytesRead != fileSize) {
            result.clear();
        }
        return result;
    }
#endif
}

// ============================================================================
// PipelineBase
// ============================================================================

PipelineBase::PipelineBase(const char* descriptorName)
    : m_descriptorName(descriptorName) {
}

void PipelineBase::cleanup(VulkanContext& ctx) {
    VkDevice device = ctx.getDevice();

    if (m_descriptorName[0] != '\0') {
        auto* dm = ctx.getDescriptorManager();
        if (dm) dm->destroyDescriptorSet(m_descriptorName);
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSet = VK_NULL_HANDLE;

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }

    m_shader.cleanup(device);
}

bool PipelineBase::loadShaders(VulkanContext& ctx, const char* vertFile, const char* fragFile) {
    VkDevice device = ctx.getDevice();
    m_shader.cleanup(device);
    if (!m_shader.loadFromFiles(device, vertFile, fragFile)) {
        LOGE("Failed to load shaders: %s / %s", vertFile, fragFile);
        return false;
    }
    return true;
}

bool PipelineBase::loadShadersWithFallback(VulkanContext& ctx,
                                            const char* vert, const char* frag,
                                            const char* fbVert, const char* fbFrag) {
    VkDevice device = ctx.getDevice();
    m_shader.cleanup(device);
    if (m_shader.loadFromFiles(device, vert, frag)) {
        LOGI("Loaded dedicated shaders: %s / %s", vert, frag);
        return true;
    }
    if (m_shader.loadFromFiles(device, fbVert, fbFrag)) {
        LOGI("Fallback: using shaders %s / %s", fbVert, fbFrag);
        return true;
    }
    LOGE("Failed to load any shaders (tried %s/%s and %s/%s)", vert, frag, fbVert, fbFrag);
    return false;
}

void PipelineBase::setupVertexInput(const VertexInputConfig& config,
                                     std::vector<VkVertexInputBindingDescription>& bindings,
                                     std::vector<VkVertexInputAttributeDescription>& attrs) {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = config.stride;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[2];
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = 0;
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = config.attr1Format;
    attributeDescriptions[1].offset = config.attr1Offset;

    bindings = {bindingDescription};
    attrs = {attributeDescriptions[0], attributeDescriptions[1]};
}

// ============================================================================
// RectPipeline
// ============================================================================

bool RectPipeline::create(VulkanContext& ctx) {
    LOGI("RectPipeline::create: Creating pipeline for simple rectangles...");
    cleanup(ctx);
    if (!loadShaders(ctx, "simple_rect.vert", "simple_rect.frag")) return false;

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    setupVertexInput({7 * sizeof(float), VK_FORMAT_R32G32B32A32_SFLOAT, 3 * sizeof(float)},
                     bindings, attrs);

    auto* descriptorManager = ctx.getDescriptorManager();
    if (!descriptorManager->createDescriptorSetLayout("rectPipeline", VK_SHADER_STAGE_VERTEX_BIT)) {
        LOGE("Failed to create rect descriptor set layout");
        return false;
    }
    m_descriptorSetLayout = descriptorManager->getDescriptorSetLayout("rectPipeline");

    PipelineBuilder builder(ctx.getDevice(), ctx.getRenderPass());
    builder.addShaderStage(m_shader.getVertexShader()->getShaderModule(), VK_SHADER_STAGE_VERTEX_BIT)
           .addShaderStage(m_shader.getFragmentShader()->getShaderModule(), VK_SHADER_STAGE_FRAGMENT_BIT)
           .setVertexInput(bindings, attrs)
           .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
           .setCullMode(VK_CULL_MODE_NONE)
           .setFrontFace(VK_FRONT_FACE_CLOCKWISE)
           .setBlendEnable(true)
           .setBlendFactors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                           VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO)
           .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
           .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
           .setDescriptorSetLayout(m_descriptorSetLayout);

    if (!builder.build(&m_pipeline, &m_layout)) {
        LOGE("Failed to create rect graphics pipeline");
        descriptorManager->destroyDescriptorSet("rectPipeline");
        return false;
    }

    const VkDeviceSize bufferSize = 64;
    if (!descriptorManager->completeDescriptorSet("rectPipeline", bufferSize)) {
        LOGE("Failed to complete rect descriptor set");
        vkDestroyPipeline(ctx.getDevice(), m_pipeline, nullptr);
        vkDestroyPipelineLayout(ctx.getDevice(), m_layout, nullptr);
        descriptorManager->destroyDescriptorSet("rectPipeline");
        return false;
    }
    auto* info = descriptorManager->getDescriptorSet("rectPipeline");
    m_descriptorSet = info->set;
    m_descriptorPool = info->pool;
    LOGI("Rectangle pipeline created successfully");
    return true;
}

// ============================================================================
// RoundedRectPipeline
// ============================================================================

bool RoundedRectPipeline::create(VulkanContext& ctx, VkDescriptorSetLayout bgDescriptorSetLayout) {
    LOGI("RoundedRectPipeline::create: Creating pipeline for rounded rectangles...");
    cleanup(ctx);
    if (!loadShaders(ctx, "simple_rounded_rect.vert", "simple_rounded_rect.frag")) return false;

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    setupVertexInput({5 * sizeof(float), VK_FORMAT_R32G32_SFLOAT, 3 * sizeof(float)},
                     bindings, attrs);

    auto* descriptorManager = ctx.getDescriptorManager();
    if (!descriptorManager->createDescriptorSetLayout("roundedRectPipeline", VK_SHADER_STAGE_VERTEX_BIT)) {
        LOGE("Failed to create rounded rect descriptor set layout");
        return false;
    }
    m_descriptorSetLayout = descriptorManager->getDescriptorSetLayout("roundedRectPipeline");

    PipelineBuilder builder(ctx.getDevice(), ctx.getRenderPass());
    builder.addShaderStage(m_shader.getVertexShader()->getShaderModule(), VK_SHADER_STAGE_VERTEX_BIT)
           .addShaderStage(m_shader.getFragmentShader()->getShaderModule(), VK_SHADER_STAGE_FRAGMENT_BIT)
           .setVertexInput(bindings, attrs)
           .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
           .setCullMode(VK_CULL_MODE_NONE)
           .setFrontFace(VK_FRONT_FACE_CLOCKWISE)
           .setBlendEnable(true)
           .setBlendFactors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                           VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO)
           .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
           .addDynamicState(VK_DYNAMIC_STATE_SCISSOR);

    if (bgDescriptorSetLayout != VK_NULL_HANDLE) {
        builder.setDescriptorSetLayouts({m_descriptorSetLayout, bgDescriptorSetLayout});
    } else {
        builder.setDescriptorSetLayout(m_descriptorSetLayout);
    }

    builder.addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                  24 * sizeof(float));

    if (!builder.build(&m_pipeline, &m_layout)) {
        LOGE("Failed to create rounded rect graphics pipeline");
        descriptorManager->destroyDescriptorSet("roundedRectPipeline");
        return false;
    }

    const VkDeviceSize uniformBufferSize = 64;
    if (!descriptorManager->completeDescriptorSet("roundedRectPipeline", uniformBufferSize)) {
        LOGE("Failed to complete rounded rect descriptor set");
        vkDestroyPipeline(ctx.getDevice(), m_pipeline, nullptr);
        vkDestroyPipelineLayout(ctx.getDevice(), m_layout, nullptr);
        descriptorManager->destroyDescriptorSet("roundedRectPipeline");
        return false;
    }
    auto* info = descriptorManager->getDescriptorSet("roundedRectPipeline");
    m_descriptorSet = info->set;
    m_descriptorPool = info->pool;
    LOGI("Rounded rectangle pipeline created successfully");
    return true;
}

// ============================================================================
// CirclePipeline
// ============================================================================

bool CirclePipeline::create(VulkanContext& ctx) {
    LOGI("CirclePipeline::create: Creating pipeline for circles...");
    cleanup(ctx);
    VkDevice device = ctx.getDevice();

    // Unique shader loading with rawfile preference
    bool shaderLoaded = false;
#if AGENUI_PLATFORM_HARMONYOS
    {
        auto* rm = ctx.getResourceManager();
        if (rm) {
            auto vertData = loadRawFileSpv(rm, "circle.vert.spv");
            auto fragData = loadRawFileSpv(rm, "circle.frag.spv");
            if (!vertData.empty() && !fragData.empty()) {
                shaderLoaded = m_shader.loadFromSPIRV(device, vertData, fragData);
                if (shaderLoaded) {
                    LOGI("CirclePipeline: loaded shaders from rawfile manager");
                }
            }
        }
    }
#endif
    if (!shaderLoaded) {
        shaderLoaded = m_shader.loadFromFiles(device, "circle.vert", "circle.frag");
    }
    if (!shaderLoaded) {
        LOGE("Failed to load circle shaders");
        return false;
    }

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    setupVertexInput({5 * sizeof(float), VK_FORMAT_R32G32_SFLOAT, 3 * sizeof(float)},
                     bindings, attrs);

    auto* descriptorManager = ctx.getDescriptorManager();
    if (!descriptorManager->createDescriptorSetLayout("circlePipeline", VK_SHADER_STAGE_VERTEX_BIT)) {
        LOGE("Failed to create circle descriptor set layout");
        return false;
    }
    m_descriptorSetLayout = descriptorManager->getDescriptorSetLayout("circlePipeline");

    PipelineBuilder builder(device, ctx.getRenderPass());
    builder.addShaderStage(m_shader.getVertexShader()->getShaderModule(), VK_SHADER_STAGE_VERTEX_BIT)
           .addShaderStage(m_shader.getFragmentShader()->getShaderModule(), VK_SHADER_STAGE_FRAGMENT_BIT)
           .setVertexInput(bindings, attrs)
           .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
           .setCullMode(VK_CULL_MODE_NONE)
           .setFrontFace(VK_FRONT_FACE_CLOCKWISE)
           .setBlendEnable(true)
           .setBlendFactors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                           VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO)
           .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
           .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
           .setDescriptorSetLayout(m_descriptorSetLayout)
           .addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                  8 * sizeof(float));

    if (!builder.build(&m_pipeline, &m_layout)) {
        LOGE("Failed to create circle graphics pipeline");
        descriptorManager->destroyDescriptorSet("circlePipeline");
        return false;
    }

    const VkDeviceSize uniformBufferSize = 64;
    if (!descriptorManager->completeDescriptorSet("circlePipeline", uniformBufferSize)) {
        LOGE("Failed to complete circle descriptor set");
        vkDestroyPipeline(device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        descriptorManager->destroyDescriptorSet("circlePipeline");
        return false;
    }
    auto* info = descriptorManager->getDescriptorSet("circlePipeline");
    m_descriptorSet = info->set;
    m_descriptorPool = info->pool;
    LOGI("Circle pipeline created successfully");
    return true;
}

// ============================================================================
// ImagePipeline
// ============================================================================

bool ImagePipeline::create(VulkanContext& ctx) {
    LOGI("ImagePipeline::create: Creating pipeline for image/textures...");
    cleanup(ctx);
    if (!loadShadersWithFallback(ctx, "image_vert.vert", "image_frag.frag",
                                  "text_vert.vert", "text_frag.frag")) return false;

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    setupVertexInput({5 * sizeof(float), VK_FORMAT_R32G32_SFLOAT, 3 * sizeof(float)},
                     bindings, attrs);

    VkDevice device = ctx.getDevice();

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create image descriptor set layout");
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1000;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1000;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create image descriptor pool");
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
        return false;
    }

    PipelineBuilder builder(device, ctx.getRenderPass());
    builder.addShaderStage(m_shader.getVertexShader()->getShaderModule(), VK_SHADER_STAGE_VERTEX_BIT)
           .addShaderStage(m_shader.getFragmentShader()->getShaderModule(), VK_SHADER_STAGE_FRAGMENT_BIT)
           .setVertexInput(bindings, attrs)
           .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
           .setCullMode(VK_CULL_MODE_NONE)
           .setFrontFace(VK_FRONT_FACE_CLOCKWISE)
           .setBlendEnable(true)
           .setBlendFactors(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                           VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO)
           .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
           .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
           .setDescriptorSetLayout(m_descriptorSetLayout)
           .addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 16 * sizeof(float));

    if (!builder.build(&m_pipeline, &m_layout)) {
        LOGE("Failed to create image graphics pipeline");
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSetLayout = VK_NULL_HANDLE;
        return false;
    }
    LOGI("Image pipeline created successfully");
    return true;
}

// ============================================================================
// TextPipeline — GPU text rendering pipeline
// ============================================================================

TextPipeline::TextPipeline(TextLayoutEngine& layoutEngine)
    : m_layoutEngine(layoutEngine) {
}

bool TextPipeline::create(VulkanContext& ctx) {
    LOGI("TextPipeline::create: Creating text rendering pipeline");
    VkDevice device = ctx.getDevice();
    if (device == VK_NULL_HANDLE) { LOGE("TextPipeline: Device is NULL"); return false; }
    VkRenderPass renderPass = ctx.getRenderPass();
    if (renderPass == VK_NULL_HANDLE) { LOGE("TextPipeline: RenderPass is NULL"); return false; }

    m_cachedRenderPass = renderPass;
    cleanupTextPipeline(device);
    m_msdfInitAttempted = false;

    if (!createTextPipeline(device, ctx.getPhysicalDevice(), renderPass)) {
        return false;
    }

    if (!initPersistentBuffers(device, ctx.getPhysicalDevice())) {
        LOGW("TextPipeline: Persistent buffer init failed, falling back to per-draw alloc");
    }

    LOGI("TextPipeline: Text pipeline created successfully");
    return true;
}

void TextPipeline::cleanup(VulkanContext& ctx) {
    LOGI("TextPipeline::cleanup: Cleaning up text pipeline");
    VkDevice device = ctx.getDevice();
    cleanupPersistentBuffers(device);
    cleanupTextPipeline(device);
}

void TextPipeline::uploadPendingAtlases(VulkanContext& ctx) {
    if (m_layoutEngine.hasPendingAtlasUpload()) {
        uploadBitmapAtlas(ctx);
    }
    if (m_layoutEngine.hasPendingSDFAtlasUpload()) {
        uploadSDFAtlas(ctx);
    }
}

// ============================================================================
// TextPipeline — High-level draw API
// ============================================================================

void TextPipeline::drawText(VulkanContext& ctx,
                             const std::string& text,
                             const glm::vec2& ndcPos,
                             uint32_t fontSize,
                             const glm::vec3& color) {
    // Prepare (CPU)
    auto run = m_layoutEngine.prepareText(text, fontSize);
    if (!run || run->glyphs.empty()) return;

    // Layout (CPU)
    auto layout = m_layoutEngine.layoutSingleLine(*run);

    // Build draw data (CPU)
    TextDrawData drawData = m_layoutEngine.buildDrawData(*run, layout, ndcPos, color);

    // Upload atlas if needed (GPU)
    if (m_layoutEngine.hasPendingAtlasUpload()) {
        uploadBitmapAtlas(ctx);
    }
    if (m_layoutEngine.hasPendingSDFAtlasUpload()) {
        uploadSDFAtlas(ctx);
    }

    // Execute draw (GPU)
    executeDraw(ctx, drawData);
}

void TextPipeline::drawTextBlock(VulkanContext& ctx,
                                  TextLayoutManager& mgr,
                                  const PreparedTextWithSegments& prepared,
                                  float x, float y,
                                  float maxWidth, float lineHeight,
                                  uint32_t fontSize,
                                  const glm::vec3& color) {
    LayoutLinesResult layoutResult = mgr.layoutWithLines(prepared, maxWidth, lineHeight);
    if (layoutResult.lines.empty()) return;
    drawTextBlock(ctx, prepared, layoutResult, x, y, maxWidth, lineHeight, fontSize, color);
}

void TextPipeline::drawTextBlock(VulkanContext& ctx,
                                  const PreparedTextWithSegments& prepared,
                                  const LayoutLinesResult& layoutResult,
                                  float x, float y,
                                  float maxWidth, float lineHeight,
                                  uint32_t fontSize,
                                  const glm::vec3& color) {
    if (layoutResult.lines.empty()) return;

    // Prepare a dummy run for the Bitmap path
    PreparedGlyphRun run;
    run.renderPath = TextRenderPath::Bitmap;
    run.fontSize = fontSize;

    // Layout multi-line (CPU)
    auto layout = m_layoutEngine.layoutMultiLine(prepared, layoutResult, fontSize, lineHeight);

    // Build draw data (CPU)
    TextDrawData drawData = m_layoutEngine.buildDrawData(run, layout, glm::vec2(x, y), color);

    // Upload atlas if needed (GPU)
    if (m_layoutEngine.hasPendingAtlasUpload()) {
        uploadBitmapAtlas(ctx);
    }
    if (m_layoutEngine.hasPendingSDFAtlasUpload()) {
        uploadSDFAtlas(ctx);
    }

    // Execute draw (GPU)
    executeDraw(ctx, drawData);
}

void TextPipeline::executeDraw(VulkanContext& ctx, const TextDrawData& drawData) {
    if (drawData.vertices.empty() || drawData.indices.empty()) return;

    VkDevice device = ctx.getDevice();
    VkCommandBuffer commandBuffer = ctx.getCurrentCommandBuffer();

    // Select pipeline + descriptor set based on render path
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    switch (drawData.renderPath) {
        case TextRenderPath::MSDF:
            ensureMsdfReady(ctx);
            pipeline = m_msdfPipeline;
            descriptorSet = m_msdfDescriptorSet;
            break;
        case TextRenderPath::SDF:
            pipeline = m_textPipeline;
            descriptorSet = m_sdfDescriptorSet;
            break;
        case TextRenderPath::Bitmap:
        default:
            pipeline = m_textPipeline;
            descriptorSet = m_textDescriptorSet;
            break;
    }

    if (pipeline == VK_NULL_HANDLE) {
        LOGE("TextPipeline::executeDraw: Selected pipeline is null (path=%d)", static_cast<int>(drawData.renderPath));
        return;
    }

    // Bind pipeline + descriptor set
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    if (descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_textPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    }

    // Push constants
    vkCmdPushConstants(commandBuffer, m_textPipelineLayout,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0, sizeof(drawData.pushConstants), drawData.pushConstants);

    // Write to persistent buffers (fallback to per-draw allocation on overflow)
    ::VkBuffer vertexBuffer;
    VkDeviceSize vbOffset;
    ::VkBuffer indexBuffer;
    VkDeviceSize ibOffset;

    uint32_t frame = ctx.getCurrentFrame();
    bool usedPersistent = writeVertexData(drawData.vertices.data(), drawData.vertices.size() * sizeof(float),
                                           vertexBuffer, vbOffset);
    if (usedPersistent) {
        usedPersistent = writeIndexData(drawData.indices.data(), drawData.indices.size() * sizeof(uint16_t),
                                         indexBuffer, ibOffset);
    }
    if (!usedPersistent) {
        VkDeviceMemory vbMem, ibMem;
        createVertexBuffer(device, ctx.getPhysicalDevice(), ctx.getCommandPool(),
                           drawData.vertices.data(), drawData.vertices.size() * sizeof(float),
                           vertexBuffer, vbMem);
        createIndexBuffer(device, ctx.getPhysicalDevice(), ctx.getCommandPool(),
                          drawData.indices.data(), drawData.indices.size() * sizeof(uint16_t),
                          indexBuffer, ibMem);
        vbOffset = 0;
        ibOffset = 0;
        ctx.getCleanupBuffers(frame).push_back(vertexBuffer);
        ctx.getCleanupBuffers(frame).push_back(indexBuffer);
        ctx.getCleanupMemories(frame).push_back(vbMem);
        ctx.getCleanupMemories(frame).push_back(ibMem);
    }

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vbOffset);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, ibOffset, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(drawData.indices.size()), 1, 0, 0, 0);

    m_layoutEngine.getStatsMutable().glyphsDrawn += static_cast<uint32_t>(drawData.indices.size() / 6);
    m_layoutEngine.getStatsMutable().drawCalls++;
}

void TextPipeline::resetPersistentBuffers() {
    m_persistentVBOffset = 0;
    m_persistentIBOffset = 0;
}

TextPipeline::PipelineHandles TextPipeline::getPipelineHandles(TextRenderPath path, VulkanContext& ctx) {
    PipelineHandles h;
    h.layout = m_textPipelineLayout;

    switch (path) {
        case TextRenderPath::MSDF:
            ensureMsdfReady(ctx);
            h.pipeline = m_msdfPipeline;
            h.descriptorSet = m_msdfDescriptorSet;
            break;
        case TextRenderPath::SDF:
            h.pipeline = m_textPipeline;
            h.descriptorSet = m_sdfDescriptorSet;
            break;
        case TextRenderPath::Bitmap:
        default:
            h.pipeline = m_textPipeline;
            h.descriptorSet = m_textDescriptorSet;
            break;
    }
    return h;
}

// ============================================================================
// TextPipeline — MSDF lazy initialization
// ============================================================================

bool TextPipeline::ensureMsdfReady(VulkanContext& ctx) {
    if (m_msdfPipeline != VK_NULL_HANDLE && m_msdfAtlas && m_msdfDescriptorSet != VK_NULL_HANDLE) {
        return true;
    }
    if (m_msdfInitAttempted) return false;
    VkDevice device = ctx.getDevice();
    if (m_textDescriptorPool == VK_NULL_HANDLE ||
        m_textDescriptorSetLayout == VK_NULL_HANDLE ||
        m_textPipelineLayout == VK_NULL_HANDLE ||
        m_cachedRenderPass == VK_NULL_HANDLE) {
        return false;
    }
    m_msdfInitAttempted = true;

    LOGI("TextPipeline::ensureMsdfReady: starting MSDF init (LAZY)");

    // 1) Load atlas from app sandbox
    m_msdfAtlas = std::make_unique<MsdfAtlas>();
    static const std::vector<std::string> searchPaths = {
        "/data/storage/el2/base/haps/entry/files/rawfile/",
        "/data/storage/el2/base/files/rawfile/"
    };
    bool loaded = false;
    for (const auto& dir : searchPaths) {
        if (m_msdfAtlas->loadFromFiles(device, ctx.getPhysicalDevice(),
                                        ctx.getCommandPool(), ctx.getGraphicsQueue(),
                                        dir + "msdf_atlas.png", dir + "msdf_atlas.json")) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        LOGE("TextPipeline::ensureMsdfReady: atlas load FAILED");
        m_msdfAtlas.reset();
        return false;
    }

    LOGI("TextPipeline::ensureMsdfReady: atlas loaded");

    // 2) Load MSDF shaders
    VkShaderProgram msdfProgram;
    if (!msdfProgram.loadFromFiles(device, "text_vert.vert", "msdf_text_frag.frag")) {
        LOGE("TextPipeline::ensureMsdfReady: shader load failed");
        m_msdfAtlas->cleanup(device);
        m_msdfAtlas.reset();
        return false;
    }

    // 3) Build pipeline
    VkPipelineShaderStageCreateInfo stages[2];
    stages[0] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = msdfProgram.getVertexShader()->getShaderModule();
    stages[0].pName = "main";
    stages[1] = {};
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = msdfProgram.getFragmentShader()->getShaderModule();
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 8 * sizeof(float);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[4]{};
    for (int i = 0; i < 4; ++i) {
        attrs[i].binding = 0;
        attrs[i].location = i;
        attrs[i].format = VK_FORMAT_R32G32_SFLOAT;
        attrs[i].offset = i * 2 * sizeof(float);
    }
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 4;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rast{};
    rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.lineWidth = 1.0f;
    rast.cullMode = VK_CULL_MODE_NONE;
    rast.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState att{};
    att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &att;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyns{};
    dyns.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyns.dynamicStateCount = 2;
    dyns.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vs;
    pi.pRasterizationState = &rast;
    pi.pMultisampleState = &ms;
    pi.pColorBlendState = &cb;
    pi.pDynamicState = &dyns;
    pi.layout = m_textPipelineLayout;
    pi.renderPass = m_cachedRenderPass;
    pi.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &m_msdfPipeline) != VK_SUCCESS) {
        LOGE("TextPipeline::ensureMsdfReady: pipeline create failed");
        msdfProgram.cleanup(device);
        m_msdfAtlas->cleanup(device);
        m_msdfAtlas.reset();
        m_msdfPipeline = VK_NULL_HANDLE;
        return false;
    }
    msdfProgram.cleanup(device);

    // 4) Allocate descriptor set
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_textDescriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_textDescriptorSetLayout;
    if (vkAllocateDescriptorSets(device, &ai, &m_msdfDescriptorSet) != VK_SUCCESS) {
        LOGE("TextPipeline::ensureMsdfReady: descriptor set alloc failed");
        return false;
    }
    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView = m_msdfAtlas->getTexture()->getImageView();
    ii.sampler = m_msdfAtlas->getTexture()->getSampler();
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = m_msdfDescriptorSet;
    w.dstBinding = 0;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.descriptorCount = 1;
    w.pImageInfo = &ii;
    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);

    LOGI("TextPipeline::ensureMsdfReady: MSDF pipeline ready");
    return true;
}

// ============================================================================
// TextPipeline — Internal GPU helpers
// ============================================================================

bool TextPipeline::createTextPipeline(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass) {
    LOGI("TextPipeline: Creating text rendering pipeline");

    VkShaderProgram shaderProgram;
    if (!shaderProgram.loadFromFiles(device, "text_vert.vert", "text_frag.frag")) {
        LOGE("Failed to load text shaders");
        return false;
    }

    VkShaderModule vertShaderModule = shaderProgram.getVertexShader()->getShaderModule();
    VkShaderModule fragShaderModule = shaderProgram.getFragmentShader()->getShaderModule();

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = 8 * sizeof(float);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[4];
    attributeDescriptions[0].binding = 0; attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[0].offset = 0;
    attributeDescriptions[1].binding = 0; attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[1].offset = 2 * sizeof(float);
    attributeDescriptions[2].binding = 0; attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[2].offset = 4 * sizeof(float);
    attributeDescriptions[3].binding = 0; attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[3].offset = 6 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 4;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128;

    if (!createDescriptorSetLayout(device)) return false;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_textDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_textPipelineLayout) != VK_SUCCESS) {
        LOGE("Failed to create text pipeline layout");
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_textPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_textPipeline) != VK_SUCCESS) {
        LOGE("Failed to create text pipeline");
        return false;
    }

    shaderProgram.cleanup(device);

    if (!createDescriptorPoolAndSets(device)) return false;

    LOGI("TextPipeline: Text pipeline created successfully");
    return true;
}

void TextPipeline::cleanupTextPipeline(VkDevice device) {
    if (device == VK_NULL_HANDLE) return;

    if (m_textPipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, m_textPipeline, nullptr); m_textPipeline = VK_NULL_HANDLE; }
    if (m_msdfPipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, m_msdfPipeline, nullptr); m_msdfPipeline = VK_NULL_HANDLE; }
    if (m_textPipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, m_textPipelineLayout, nullptr); m_textPipelineLayout = VK_NULL_HANDLE; }
    if (m_textDescriptorPool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, m_textDescriptorPool, nullptr); m_textDescriptorPool = VK_NULL_HANDLE; }
    if (m_textDescriptorSetLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, m_textDescriptorSetLayout, nullptr); m_textDescriptorSetLayout = VK_NULL_HANDLE; }

    if (m_bitmapTextureOwned && m_bitmapTexture) {
        m_bitmapTexture->cleanup(device);
        delete m_bitmapTexture;
        m_bitmapTexture = nullptr;
        m_bitmapTextureOwned = false;
    }

    if (m_sdfTextureOwned && m_sdfTexture) {
        m_sdfTexture->cleanup(device);
        delete m_sdfTexture;
        m_sdfTexture = nullptr;
        m_sdfTextureOwned = false;
    }

    if (m_msdfAtlas) {
        m_msdfAtlas->cleanup(device);
        m_msdfAtlas.reset();
    }
    m_msdfDescriptorSet = VK_NULL_HANDLE;
}

bool TextPipeline::createDescriptorSetLayout(VkDevice device) {
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &samplerBinding;

    if (vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &m_textDescriptorSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create text descriptor set layout");
        return false;
    }
    return true;
}

bool TextPipeline::createDescriptorPoolAndSets(VkDevice device) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 3;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 10;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_textDescriptorPool) != VK_SUCCESS) {
        LOGE("Failed to create text descriptor pool");
        return false;
    }

    VkDescriptorSetAllocateInfo descriptorAllocInfo{};
    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = m_textDescriptorPool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &m_textDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &descriptorAllocInfo, &m_textDescriptorSet) != VK_SUCCESS) {
        LOGE("Failed to allocate text descriptor set");
        return false;
    }

    // Bitmap texture descriptor will be updated when atlas is first uploaded
    return true;
}

void TextPipeline::uploadBitmapAtlas(VulkanContext& ctx) {
    const uint8_t* data = m_layoutEngine.getAtlasData();
    if (!data) return;

    uint32_t w = m_layoutEngine.getAtlasWidth();
    uint32_t h = m_layoutEngine.getAtlasHeight();
    if (w == 0 || h == 0) return;

    VkDevice device = ctx.getDevice();

    if (!m_bitmapTexture) {
        m_bitmapTexture = new VkTexture();
        m_bitmapTextureOwned = true;
        if (!m_bitmapTexture->createEmpty(device, ctx.getPhysicalDevice(),
                                           ctx.getCommandPool(), ctx.getGraphicsQueue(),
                                           w, h, VK_FORMAT_R8_UNORM)) {
            LOGE("TextPipeline: Failed to create bitmap atlas texture");
            delete m_bitmapTexture;
            m_bitmapTexture = nullptr;
            m_bitmapTextureOwned = false;
            return;
        }

        VkDescriptorImageInfo descriptorImageInfo{};
        descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descriptorImageInfo.imageView = m_bitmapTexture->getImageView();
        descriptorImageInfo.sampler = m_bitmapTexture->getSampler();

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_textDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &descriptorImageInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    if (!m_bitmapTexture->updateData(device, ctx.getCommandPool(), ctx.getGraphicsQueue(),
                                      0, 0, w, h, data)) {
        LOGE("TextPipeline: Failed to upload bitmap atlas data");
        return;
    }

    m_layoutEngine.markAtlasUploaded();
    m_layoutEngine.getStatsMutable().atlasUploads++;
    LOGI("TextPipeline: Uploaded bitmap atlas %ux%u", w, h);
}

void TextPipeline::uploadSDFAtlas(VulkanContext& ctx) {
    const uint8_t* data = m_layoutEngine.getSDFAtlasData();
    if (!data) return;

    VkDevice device = ctx.getDevice();
    uint32_t w = 2048, h = 2048;

    if (!m_sdfTexture) {
        m_sdfTexture = new VkTexture();
        m_sdfTextureOwned = true;
        if (!m_sdfTexture->createEmpty(device, ctx.getPhysicalDevice(),
                                        ctx.getCommandPool(), ctx.getGraphicsQueue(),
                                        w, h, VK_FORMAT_R8_UNORM)) {
            LOGE("TextPipeline: Failed to create SDF atlas texture");
            delete m_sdfTexture;
            m_sdfTexture = nullptr;
            m_sdfTextureOwned = false;
            return;
        }

        VkDescriptorSetAllocateInfo sdfAllocInfo{};
        sdfAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        sdfAllocInfo.descriptorPool = m_textDescriptorPool;
        sdfAllocInfo.descriptorSetCount = 1;
        sdfAllocInfo.pSetLayouts = &m_textDescriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &sdfAllocInfo, &m_sdfDescriptorSet) != VK_SUCCESS) {
            LOGE("TextPipeline: Failed to allocate SDF descriptor set");
            return;
        }

        VkDescriptorImageInfo sdfImageInfo{};
        sdfImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sdfImageInfo.imageView = m_sdfTexture->getImageView();
        sdfImageInfo.sampler = m_sdfTexture->getSampler();

        VkWriteDescriptorSet sdfWrite{};
        sdfWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sdfWrite.dstSet = m_sdfDescriptorSet;
        sdfWrite.dstBinding = 0;
        sdfWrite.dstArrayElement = 0;
        sdfWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sdfWrite.descriptorCount = 1;
        sdfWrite.pImageInfo = &sdfImageInfo;

        vkUpdateDescriptorSets(device, 1, &sdfWrite, 0, nullptr);
    }

    if (!m_sdfTexture->updateData(device, ctx.getCommandPool(), ctx.getGraphicsQueue(),
                                   0, 0, w, h, data)) {
        LOGE("TextPipeline: Failed to upload SDF atlas data");
        return;
    }

    m_layoutEngine.markSDFAtlasUploaded();
}

// ============================================================================
// TextPipeline — Persistent buffer management
// ============================================================================

bool TextPipeline::initPersistentBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {
    if (m_persistentBuffersInitialized) return true;

    auto createPersistent = [&](size_t size, VkBufferUsageFlags usage,
                                PersistentBuffer& out) -> bool {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = size;
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufInfo, nullptr, &out.buffer) != VK_SUCCESS) return false;

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(device, out.buffer, &memReq);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &out.memory) != VK_SUCCESS) {
            vkDestroyBuffer(device, out.buffer, nullptr);
            out.buffer = VK_NULL_HANDLE;
            return false;
        }

        vkBindBufferMemory(device, out.buffer, out.memory, 0);
        vkMapMemory(device, out.memory, 0, size, 0, &out.mapped);
        out.capacity = size;
        return true;
    };

    if (!createPersistent(PERSISTENT_VB_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_persistentVB)) return false;
    if (!createPersistent(PERSISTENT_IB_SIZE, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_persistentIB)) {
        vkDestroyBuffer(device, m_persistentVB.buffer, nullptr);
        vkFreeMemory(device, m_persistentVB.memory, nullptr);
        m_persistentVB = {};
        return false;
    }

    m_persistentBuffersInitialized = true;
    LOGI("TextPipeline: Persistent buffers initialized (VB=%zuKB, IB=%zuKB)",
         PERSISTENT_VB_SIZE / 1024, PERSISTENT_IB_SIZE / 1024);
    return true;
}

void TextPipeline::cleanupPersistentBuffers(VkDevice device) {
    if (!m_persistentBuffersInitialized) return;

    if (m_persistentVB.mapped) { vkUnmapMemory(device, m_persistentVB.memory); m_persistentVB.mapped = nullptr; }
    if (m_persistentVB.buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_persistentVB.buffer, nullptr); }
    if (m_persistentVB.memory != VK_NULL_HANDLE) { vkFreeMemory(device, m_persistentVB.memory, nullptr); }
    m_persistentVB = {};

    if (m_persistentIB.mapped) { vkUnmapMemory(device, m_persistentIB.memory); m_persistentIB.mapped = nullptr; }
    if (m_persistentIB.buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_persistentIB.buffer, nullptr); }
    if (m_persistentIB.memory != VK_NULL_HANDLE) { vkFreeMemory(device, m_persistentIB.memory, nullptr); }
    m_persistentIB = {};

    m_persistentVBOffset = 0;
    m_persistentIBOffset = 0;
    m_persistentBuffersInitialized = false;
}

bool TextPipeline::writeVertexData(const float* data, size_t byteSize,
                                     ::VkBuffer& outBuffer, VkDeviceSize& outOffset) {
    if (!m_persistentBuffersInitialized) return false;
    if (m_persistentVBOffset + byteSize > m_persistentVB.capacity) return false;
    outOffset = static_cast<VkDeviceSize>(m_persistentVBOffset);
    memcpy(static_cast<uint8_t*>(m_persistentVB.mapped) + m_persistentVBOffset, data, byteSize);
    m_persistentVBOffset += byteSize;
    m_persistentVBOffset = (m_persistentVBOffset + 3) & ~size_t(3);
    outBuffer = m_persistentVB.buffer;
    return true;
}

bool TextPipeline::writeIndexData(const uint16_t* data, size_t byteSize,
                                    ::VkBuffer& outBuffer, VkDeviceSize& outOffset) {
    if (!m_persistentBuffersInitialized) return false;
    if (m_persistentIBOffset + byteSize > m_persistentIB.capacity) return false;
    outOffset = static_cast<VkDeviceSize>(m_persistentIBOffset);
    memcpy(static_cast<uint8_t*>(m_persistentIB.mapped) + m_persistentIBOffset, data, byteSize);
    m_persistentIBOffset += byteSize;
    m_persistentIBOffset = (m_persistentIBOffset + 3) & ~size_t(3);
    outBuffer = m_persistentIB.buffer;
    return true;
}

void TextPipeline::createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool,
                                       const float* vertices, size_t size, ::VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
    void* data;
    vkMapMemory(device, bufferMemory, 0, size, 0, &data);
    memcpy(data, vertices, size);
    vkUnmapMemory(device, bufferMemory);
}

void TextPipeline::createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool,
                                      const uint16_t* indices, size_t size, ::VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create index buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate index buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
    void* data;
    vkMapMemory(device, bufferMemory, 0, size, 0, &data);
    memcpy(data, indices, size);
    vkUnmapMemory(device, bufferMemory);
}

uint32_t TextPipeline::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace AgenUIEngine
