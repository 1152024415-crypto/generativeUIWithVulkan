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

#include "PipelineBuilder.h"

// Platform-specific logging
#if AGENUI_PLATFORM_WINDOWS
    #define LOGI(...) std::printf("[INFO] [PipelineBuilder] " __VA_ARGS__); std::printf("\n")
    #define LOGE(...) std::printf("[ERROR] [PipelineBuilder] " __VA_ARGS__); std::printf("\n")
#elif AGENUI_PLATFORM_HARMONYOS
    #include "logger_common.h"
#endif

namespace AgenUIEngine {

PipelineBuilder::PipelineBuilder(VkDevice device, VkRenderPass renderPass)
    : m_device(device)
    , m_renderPass(renderPass) {
    initDefaults();
}

PipelineBuilder::~PipelineBuilder() = default;

void PipelineBuilder::initDefaults() {
    // Initialize all state structures with safe defaults

    // Shader stages - empty by default
    m_shaderStages.clear();

    // Vertex input - empty by default
    m_vertexBindings.clear();
    m_vertexAttributes.clear();
    m_vertexInputInfo = {};
    m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Input assembly
    m_inputAssembly = {};
    m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    m_viewportState = {};
    m_viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_viewportState.viewportCount = 1;
    m_viewportState.scissorCount = 1;

    // Rasterization
    m_rasterization = {};
    m_rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_rasterization.depthClampEnable = VK_FALSE;
    m_rasterization.rasterizerDiscardEnable = VK_FALSE;
    m_rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    m_rasterization.lineWidth = 1.0f;
    m_rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    m_rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // Multisampling
    m_multisampling = {};
    m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_multisampling.sampleShadingEnable = VK_FALSE;
    m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/stencil
    m_depthStencil = {};
    m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_depthStencil.depthTestEnable = VK_FALSE;
    m_depthStencil.depthWriteEnable = VK_FALSE;
    m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    m_depthStencil.stencilTestEnable = VK_FALSE;
    m_depthStencil.minDepthBounds = 0.0f;
    m_depthStencil.maxDepthBounds = 1.0f;
    m_depthStencil.front = {};
    m_depthStencil.front.failOp = VK_STENCIL_OP_KEEP;
    m_depthStencil.front.passOp = VK_STENCIL_OP_KEEP;
    m_depthStencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
    m_depthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
    m_depthStencil.front.compareMask = 0xFF;
    m_depthStencil.front.writeMask = 0xFF;
    m_depthStencil.front.reference = 0;
    m_depthStencil.back = m_depthStencil.front;

    // Color blending
    m_colorBlendAttachment = {};
    m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_colorBlendAttachment.blendEnable = VK_FALSE;

    m_colorBlending = {};
    m_colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_colorBlending.logicOpEnable = VK_FALSE;
    m_colorBlending.logicOp = VK_LOGIC_OP_COPY;
    m_colorBlending.attachmentCount = 1;
    m_colorBlending.pAttachments = &m_colorBlendAttachment;
    m_colorBlending.blendConstants[0] = 0.0f;
    m_colorBlending.blendConstants[1] = 0.0f;
    m_colorBlending.blendConstants[2] = 0.0f;
    m_colorBlending.blendConstants[3] = 0.0f;

    // Dynamic states
    m_dynamicStates.clear();
    m_dynamicState = {};
    m_dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    // Pipeline layout
    m_descriptorSetLayouts.clear();
    m_pushConstantRanges.clear();
    m_layoutInfo = {};
    m_layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
}

// Shader stages
PipelineBuilder& PipelineBuilder::addShaderStage(VkShaderModule module, VkShaderStageFlagBits stage, const char* entry) {
    VkPipelineShaderStageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage = stage;
    createInfo.module = module;
    createInfo.pName = entry;
    m_shaderStages.push_back(createInfo);
    return *this;
}

// Vertex input
PipelineBuilder& PipelineBuilder::setVertexInput(
    const std::vector<VkVertexInputBindingDescription>& bindings,
    const std::vector<VkVertexInputAttributeDescription>& attributes) {
    m_vertexBindings = bindings;
    m_vertexAttributes = attributes;
    m_vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    m_vertexInputInfo.pVertexBindingDescriptions = bindings.data();
    m_vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    m_vertexInputInfo.pVertexAttributeDescriptions = attributes.data();
    return *this;
}

// Input assembly
PipelineBuilder& PipelineBuilder::setTopology(VkPrimitiveTopology topology) {
    m_inputAssembly.topology = topology;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPrimitiveRestartEnable(bool enable) {
    m_inputAssembly.primitiveRestartEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

// Rasterization
PipelineBuilder& PipelineBuilder::setPolygonMode(VkPolygonMode mode) {
    m_rasterization.polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(VkCullModeFlags cullMode) {
    m_rasterization.cullMode = cullMode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setFrontFace(VkFrontFace frontFace) {
    m_rasterization.frontFace = frontFace;
    return *this;
}

PipelineBuilder& PipelineBuilder::setLineWidth(float width) {
    m_rasterization.lineWidth = width;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthClampEnable(bool enable) {
    m_rasterization.depthClampEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setRasterizerDiscardEnable(bool enable) {
    m_rasterization.rasterizerDiscardEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

// Depth/stencil
PipelineBuilder& PipelineBuilder::setDepthTestEnable(bool enable) {
    m_depthStencil.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthWriteEnable(bool enable) {
    m_depthStencil.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthCompareOp(VkCompareOp op) {
    m_depthStencil.depthCompareOp = op;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthBoundsTestEnable(bool enable) {
    m_depthStencil.depthBoundsTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setStencilTestEnable(bool enable) {
    m_depthStencil.stencilTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

// Multisampling
PipelineBuilder& PipelineBuilder::setRasterizationSamples(VkSampleCountFlagBits samples) {
    m_multisampling.rasterizationSamples = samples;
    return *this;
}

PipelineBuilder& PipelineBuilder::setSampleShadingEnable(bool enable, float minSampleShading) {
    m_multisampling.sampleShadingEnable = enable ? VK_TRUE : VK_FALSE;
    m_multisampling.minSampleShading = minSampleShading;
    return *this;
}

// Color blending
PipelineBuilder& PipelineBuilder::setBlendEnable(bool enable) {
    m_colorBlendAttachment.blendEnable = enable ? VK_TRUE : VK_FALSE;
    if (enable) {
        // Set default blend factors for alpha blending
        m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        m_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        m_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    return *this;
}

PipelineBuilder& PipelineBuilder::setBlendFactors(VkBlendFactor srcColor, VkBlendFactor dstColor,
                                                   VkBlendFactor srcAlpha, VkBlendFactor dstAlpha) {
    m_colorBlendAttachment.srcColorBlendFactor = srcColor;
    m_colorBlendAttachment.dstColorBlendFactor = dstColor;
    m_colorBlendAttachment.srcAlphaBlendFactor = srcAlpha;
    m_colorBlendAttachment.dstAlphaBlendFactor = dstAlpha;
    return *this;
}

PipelineBuilder& PipelineBuilder::setBlendOps(VkBlendOp colorOp, VkBlendOp alphaOp) {
    m_colorBlendAttachment.colorBlendOp = colorOp;
    m_colorBlendAttachment.alphaBlendOp = alphaOp;
    return *this;
}

PipelineBuilder& PipelineBuilder::setColorWriteMask(VkColorComponentFlags mask) {
    m_colorBlendAttachment.colorWriteMask = mask;
    return *this;
}

PipelineBuilder& PipelineBuilder::setBlendConstants(const float blendConstants[4]) {
    m_colorBlending.blendConstants[0] = blendConstants[0];
    m_colorBlending.blendConstants[1] = blendConstants[1];
    m_colorBlending.blendConstants[2] = blendConstants[2];
    m_colorBlending.blendConstants[3] = blendConstants[3];
    return *this;
}

// Dynamic states
PipelineBuilder& PipelineBuilder::addDynamicState(VkDynamicState state) {
    m_dynamicStates.push_back(state);
    return *this;
}

PipelineBuilder& PipelineBuilder::setDynamicStates(const std::vector<VkDynamicState>& states) {
    m_dynamicStates = states;
    return *this;
}

// Descriptor set layout
PipelineBuilder& PipelineBuilder::setDescriptorSetLayout(VkDescriptorSetLayout layout) {
    m_descriptorSetLayouts.clear();
    m_descriptorSetLayouts.push_back(layout);
    m_layoutInfo.setLayoutCount = 1;
    m_layoutInfo.pSetLayouts = m_descriptorSetLayouts.data();
    return *this;
}

PipelineBuilder& PipelineBuilder::setDescriptorSetLayouts(const std::vector<VkDescriptorSetLayout>& layouts) {
    m_descriptorSetLayouts = layouts;
    m_layoutInfo.setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size());
    m_layoutInfo.pSetLayouts = m_descriptorSetLayouts.data();
    return *this;
}

// Push constants
PipelineBuilder& PipelineBuilder::addPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size) {
    VkPushConstantRange range{};
    range.stageFlags = stages;
    range.offset = offset;
    range.size = size;
    m_pushConstantRanges.push_back(range);
    m_layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size());
    m_layoutInfo.pPushConstantRanges = m_pushConstantRanges.data();
    return *this;
}

// Viewport state
PipelineBuilder& PipelineBuilder::setViewportCount(uint32_t count) {
    m_viewportState.viewportCount = count;
    return *this;
}

PipelineBuilder& PipelineBuilder::setScissorCount(uint32_t count) {
    m_viewportState.scissorCount = count;
    return *this;
}

// Reset
void PipelineBuilder::reset() {
    initDefaults();
}

// Build
bool PipelineBuilder::build(VkPipeline* outPipeline, VkPipelineLayout* outLayout, VkPipelineCache pipelineCache) {
    if (m_shaderStages.empty()) {
        LOGE("PipelineBuilder: No shader stages provided");
        return false;
    }

    // Update dynamic state
    if (!m_dynamicStates.empty()) {
        m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
        m_dynamicState.pDynamicStates = m_dynamicStates.data();
    }

    // Create pipeline layout
    if (vkCreatePipelineLayout(m_device, &m_layoutInfo, nullptr, outLayout) != VK_SUCCESS) {
        LOGE("PipelineBuilder: Failed to create pipeline layout");
        return false;
    }

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
    pipelineInfo.pStages = m_shaderStages.data();
    pipelineInfo.pVertexInputState = &m_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssembly;
    pipelineInfo.pViewportState = &m_viewportState;
    pipelineInfo.pRasterizationState = &m_rasterization;
    pipelineInfo.pMultisampleState = &m_multisampling;
    pipelineInfo.pDepthStencilState = &m_depthStencil;
    pipelineInfo.pColorBlendState = &m_colorBlending;
    pipelineInfo.pDynamicState = (m_dynamicStates.empty() ? nullptr : &m_dynamicState);
    pipelineInfo.layout = *outLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(m_device, pipelineCache, 1, &pipelineInfo, nullptr, outPipeline) != VK_SUCCESS) {
        LOGE("PipelineBuilder: Failed to create graphics pipeline");
        vkDestroyPipelineLayout(m_device, *outLayout, nullptr);
        return false;
    }

    return true;
}

} // namespace AgenUIEngine
