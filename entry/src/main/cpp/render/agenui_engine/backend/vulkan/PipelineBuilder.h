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

#ifndef AGENUI_ENGINE_PIPELINE_BUILDER_H
#define AGENUI_ENGINE_PIPELINE_BUILDER_H

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace AgenUIEngine {

/**
 * @brief Builder pattern for creating Vulkan graphics pipelines
 *
 * Provides a fluent interface for configuring pipeline state,
 * eliminating boilerplate and reducing duplication.
 */
class PipelineBuilder {
public:
    PipelineBuilder(VkDevice device, VkRenderPass renderPass);
    ~PipelineBuilder();

    // Disable copying
    PipelineBuilder(const PipelineBuilder&) = delete;
    PipelineBuilder& operator=(const PipelineBuilder&) = delete;

    // Shader stages
    PipelineBuilder& addShaderStage(VkShaderModule module, VkShaderStageFlagBits stage, const char* entry = "main");

    // Vertex input
    PipelineBuilder& setVertexInput(const std::vector<VkVertexInputBindingDescription>& bindings,
                                    const std::vector<VkVertexInputAttributeDescription>& attributes);

    // Input assembly
    PipelineBuilder& setTopology(VkPrimitiveTopology topology);
    PipelineBuilder& setPrimitiveRestartEnable(bool enable);

    // Rasterization
    PipelineBuilder& setPolygonMode(VkPolygonMode mode);
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode);
    PipelineBuilder& setFrontFace(VkFrontFace frontFace);
    PipelineBuilder& setLineWidth(float width);
    PipelineBuilder& setDepthClampEnable(bool enable);
    PipelineBuilder& setRasterizerDiscardEnable(bool enable);

    // Depth/stencil
    PipelineBuilder& setDepthTestEnable(bool enable);
    PipelineBuilder& setDepthWriteEnable(bool enable);
    PipelineBuilder& setDepthCompareOp(VkCompareOp op);
    PipelineBuilder& setDepthBoundsTestEnable(bool enable);
    PipelineBuilder& setStencilTestEnable(bool enable);

    // Multisampling
    PipelineBuilder& setRasterizationSamples(VkSampleCountFlagBits samples);
    PipelineBuilder& setSampleShadingEnable(bool enable, float minSampleShading = 1.0f);

    // Color blending
    PipelineBuilder& setBlendEnable(bool enable);
    PipelineBuilder& setBlendFactors(VkBlendFactor srcColor, VkBlendFactor dstColor,
                                    VkBlendFactor srcAlpha, VkBlendFactor dstAlpha);
    PipelineBuilder& setBlendOps(VkBlendOp colorOp, VkBlendOp alphaOp);
    PipelineBuilder& setColorWriteMask(VkColorComponentFlags mask);
    PipelineBuilder& setBlendConstants(const float blendConstants[4]);

    // Dynamic states
    PipelineBuilder& addDynamicState(VkDynamicState state);
    PipelineBuilder& setDynamicStates(const std::vector<VkDynamicState>& states);

    // Descriptor set layout
    PipelineBuilder& setDescriptorSetLayout(VkDescriptorSetLayout layout);
    PipelineBuilder& setDescriptorSetLayouts(const std::vector<VkDescriptorSetLayout>& layouts);

    // Push constants
    PipelineBuilder& addPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size);

    // Viewport state (for dynamic state)
    PipelineBuilder& setViewportCount(uint32_t count);
    PipelineBuilder& setScissorCount(uint32_t count);

    /**
     * @brief Build the graphics pipeline
     *
     * @param outPipeline Pointer to receive the created pipeline
     * @param outLayout Pointer to receive the created pipeline layout
     * @param pipelineCache Optional pipeline cache for faster compilation
     * @return true if successful
     */
    bool build(VkPipeline* outPipeline, VkPipelineLayout* outLayout, VkPipelineCache pipelineCache = VK_NULL_HANDLE);

    /**
     * @brief Reset the builder to default state
     */
    void reset();

private:
    void initDefaults();

    VkDevice m_device;
    VkRenderPass m_renderPass;

    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

    // Vertex input
    std::vector<VkVertexInputBindingDescription> m_vertexBindings;
    std::vector<VkVertexInputAttributeDescription> m_vertexAttributes;
    VkPipelineVertexInputStateCreateInfo m_vertexInputInfo{};

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};

    // Viewport state
    VkPipelineViewportStateCreateInfo m_viewportState{};

    // Rasterization
    VkPipelineRasterizationStateCreateInfo m_rasterization{};

    // Multisampling
    VkPipelineMultisampleStateCreateInfo m_multisampling{};

    // Depth/stencil
    VkPipelineDepthStencilStateCreateInfo m_depthStencil{};

    // Color blending
    VkPipelineColorBlendAttachmentState m_colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo m_colorBlending{};

    // Dynamic states
    std::vector<VkDynamicState> m_dynamicStates;
    VkPipelineDynamicStateCreateInfo m_dynamicState{};

    // Pipeline layout
    VkPipelineLayoutCreateInfo m_layoutInfo{};
    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    std::vector<VkPushConstantRange> m_pushConstantRanges;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_PIPELINE_BUILDER_H
