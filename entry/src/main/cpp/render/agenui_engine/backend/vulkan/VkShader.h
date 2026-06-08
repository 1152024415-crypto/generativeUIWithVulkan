/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Vulkan Shader - implements Core::IShader directly
 * Merged from Shader (vert+frag pair) + VulkanShader (single module adapter)
 *
 * VkShader manages a single shader module (vertex OR fragment).
 * VkShaderProgram manages a vert+frag pair for pipeline creation.
 */

#ifndef AGENUI_ENGINE_VK_SHADER_H
#define AGENUI_ENGINE_VK_SHADER_H

#include "core/IGraphicsAPI.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace AgenUIEngine {

// Shader loader function type: takes shader name, returns SPIR-V bytecode
using ShaderLoaderFunc = std::function<std::vector<char>(const char*)>;

/**
 * @brief Single Vulkan shader module implementing Core::IShader
 *
 * Manages a single VkShaderModule (vertex, fragment, compute, etc.)
 * and provides the abstract IShader interface for cross-platform compatibility.
 */
class VkShader : public Core::IShader {
public:
    VkShader();
    ~VkShader() override;

    // -------------------------------------------------------------------------
    // IShader interface implementation
    // -------------------------------------------------------------------------

    const std::vector<uint32_t>& getByteCode() const override;
    Core::ShaderStage getStage() const override;
    const char* getEntryPoint() const override;

    // -------------------------------------------------------------------------
    // Vulkan-specific methods
    // -------------------------------------------------------------------------

    /**
     * @brief Load shader from SPIRV bytecode
     */
    bool loadFromSPIRV(VkDevice device, const std::vector<uint32_t>& spirv,
                      Core::ShaderStage stage, const char* entryPoint = "main");

    /**
     * @brief Load shader from SPIRV file
     */
    bool loadFromSPIRVFile(VkDevice device, const std::string& filename,
                          Core::ShaderStage stage, const char* entryPoint = "main");

    /**
     * @brief Load shader using custom loader function
     */
    bool loadFromLoader(VkDevice device, ShaderLoaderFunc loader,
                       const std::string& name, Core::ShaderStage stage,
                       const char* entryPoint = "main");

    void cleanup(VkDevice device);

    VkShaderModule getShaderModule() const { return m_shaderModule; }

    // Static utilities
    static VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& spirv);
    static std::vector<uint32_t> readSPIRVFile(const std::string& filename);

private:
    static VkShaderStageFlagBits shaderStageToVkFlag(Core::ShaderStage stage);

    VkShaderModule m_shaderModule = VK_NULL_HANDLE;
    std::vector<uint32_t> m_spirv;
    Core::ShaderStage m_stage = Core::ShaderStage::Vertex;
    std::string m_entryPoint = "main";
    VkDevice m_device = VK_NULL_HANDLE;
};

/**
 * @brief Helper class for managing shader pairs (vertex + fragment)
 *
 * Replaces the original AgenUIEngine::Shader vert+frag pair pattern.
 */
class VkShaderProgram {
public:
    VkShaderProgram();

    VkShader* getVertexShader() { return m_vertexShader.get(); }
    VkShader* getFragmentShader() { return m_fragmentShader.get(); }
    const VkShader* getVertexShader() const { return m_vertexShader.get(); }
    const VkShader* getFragmentShader() const { return m_fragmentShader.get(); }

    /**
     * @brief Load vertex and fragment shaders from SPIRV files
     */
    bool loadFromFiles(VkDevice device,
                      const std::string& vertFilename,
                      const std::string& fragFilename);

    /**
     * @brief Load vertex and fragment shaders from SPIRV bytecode (char vectors)
     */
    bool loadFromSPIRV(VkDevice device,
                      const std::vector<char>& vertCode,
                      const std::vector<char>& fragCode);

    /**
     * @brief Load vertex and fragment shaders using custom loader function
     */
    bool loadFromLoader(VkDevice device, ShaderLoaderFunc loader,
                       const char* vertShaderName, const char* fragShaderName);

    /**
     * @brief Load from SPIRV uint32_t vectors
     */
    bool loadFromSPIRVU32(VkDevice device,
                          const std::vector<uint32_t>& vertCode,
                          const std::vector<uint32_t>& fragCode);

    void cleanup(VkDevice device);

private:
    std::unique_ptr<VkShader> m_vertexShader;
    std::unique_ptr<VkShader> m_fragmentShader;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_VK_SHADER_H
