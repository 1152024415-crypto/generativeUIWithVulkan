/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Vulkan Shader Implementation
 * Merged from Shader.cpp + VulkanShader.cpp
 */

#include "VkShader.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

#if AGENUI_PLATFORM_HARMONYOS
    #include <hilog/log.h>
#elif AGENUI_PLATFORM_WINDOWS
    #include <cstdio>
#endif
#include <filesystem>

namespace AgenUIEngine {

// Platform-specific logging macros
#if AGENUI_PLATFORM_HARMONYOS
    #undef LOG_TAG
    #define LOG_TAG "AgenUIEngine::VkShader"
    #define LOGI(...) OH_LOG_INFO(LogType::LOG_APP, __VA_ARGS__)
    #define LOGW(...) OH_LOG_WARN(LogType::LOG_APP, __VA_ARGS__)
    #define LOGE(...) OH_LOG_ERROR(LogType::LOG_APP, __VA_ARGS__)
#elif AGENUI_PLATFORM_WINDOWS
    #ifndef LOGI
        #include <cstdio>
        #define LOGI(...) printf("[INFO] [AgenUIEngine::VkShader] "), printf(__VA_ARGS__), printf("\n")
        #define LOGW(...) printf("[WARN] [AgenUIEngine::VkShader] "), printf(__VA_ARGS__), printf("\n")
        #define LOGE(...) printf("[ERROR] [AgenUIEngine::VkShader] "), printf(__VA_ARGS__), printf("\n")
    #endif
#endif

// =========================================================================
// VkShader Implementation
// =========================================================================

VkShader::VkShader() = default;

VkShader::~VkShader() {
    if (m_device != VK_NULL_HANDLE) {
        cleanup(m_device);
    }
}

// -------------------------------------------------------------------------
// IShader interface implementation
// -------------------------------------------------------------------------

const std::vector<uint32_t>& VkShader::getByteCode() const {
    return m_spirv;
}

Core::ShaderStage VkShader::getStage() const {
    return m_stage;
}

const char* VkShader::getEntryPoint() const {
    return m_entryPoint.c_str();
}

// -------------------------------------------------------------------------
// Vulkan-specific methods
// -------------------------------------------------------------------------

bool VkShader::loadFromSPIRV(VkDevice device, const std::vector<uint32_t>& spirv,
                             Core::ShaderStage stage, const char* entryPoint) {
    m_device = device;
    m_spirv = spirv;
    m_stage = stage;
    m_entryPoint = entryPoint ? entryPoint : "main";

    // Create shader module
    m_shaderModule = createShaderModule(device, spirv);
    if (m_shaderModule == VK_NULL_HANDLE) {
        return false;
    }

    return true;
}

bool VkShader::loadFromSPIRVFile(VkDevice device, const std::string& filename,
                                 Core::ShaderStage stage, const char* entryPoint) {
    // Read SPIRV file
    std::vector<uint32_t> spirv = readSPIRVFile(filename);
    if (spirv.empty()) {
        return false;
    }

    return loadFromSPIRV(device, spirv, stage, entryPoint);
}

bool VkShader::loadFromLoader(VkDevice device, ShaderLoaderFunc loader,
                              const std::string& name, Core::ShaderStage stage,
                              const char* entryPoint) {
    if (!loader) {
        LOGE("VkShader: Shader loader function is null");
        return false;
    }

    std::string shaderName = name;

    // Add "shaders/" prefix if not present (for HarmonyOS rawfile structure)
#if AGENUI_PLATFORM_HARMONYOS
    if (shaderName.find("shaders/") != 0) {
        shaderName = "shaders/" + shaderName;
    }
#endif

    // Add .spv extension if not present
    if (shaderName.find(".spv") == std::string::npos) {
        shaderName = shaderName + ".spv";
    }

    LOGI("VkShader: Loading shader '%s'", shaderName.c_str());

    // Load shader using the loader function
    std::vector<char> code = loader(shaderName.c_str());

    if (code.empty()) {
        LOGE("VkShader: Failed to load shader code via loader: %zu bytes", code.size());
        return false;
    }

    // Convert char vector to uint32_t vector
    std::vector<uint32_t> spirv(code.size() / sizeof(uint32_t));
    std::memcpy(spirv.data(), code.data(), code.size());

    return loadFromSPIRV(device, spirv, stage, entryPoint);
}

void VkShader::cleanup(VkDevice device) {
    if (m_shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_shaderModule, nullptr);
        m_shaderModule = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

VkShaderModule VkShader::createShaderModule(VkDevice device, const std::vector<uint32_t>& spirv) {
    if (spirv.empty()) {
        LOGE("VkShader: SPIRV code is empty");
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOGE("VkShader: Failed to create shader module");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

std::vector<uint32_t> VkShader::readSPIRVFile(const std::string& filename) {
    // Build list of paths to try
    std::vector<std::string> pathsToTry;

    // Check if filename already has .spv extension
    bool hasSpvExtension = filename.find(".spv") != std::string::npos;
    std::string shaderFile = filename;

    if (!hasSpvExtension) {
        if (filename.find(".vert") != std::string::npos || filename.find(".frag") != std::string::npos) {
            if (filename.find(".vert.spv") == std::string::npos && filename.find(".frag.spv") == std::string::npos) {
                shaderFile = filename + ".spv";
            }
        } else {
            shaderFile = filename + ".spv";
        }
    }

#if AGENUI_PLATFORM_HARMONYOS
    pathsToTry.push_back("/data/storage/el2/base/haps/entry/files/rawfile/" + shaderFile);
    if (!hasSpvExtension) {
        pathsToTry.push_back("/data/storage/el2/base/haps/entry/files/rawfile/" + filename + ".spv");
    }
    pathsToTry.push_back("/data/storage/el2/base/aps/entry/files/rawfile/" + shaderFile);
    pathsToTry.push_back("/data/storage/el2/base/files/rawfile/" + shaderFile);
#elif AGENUI_PLATFORM_WINDOWS
    namespace fs = std::filesystem;

    fs::path currentPath = fs::current_path();

    pathsToTry.push_back("./shaders/" + shaderFile);
    pathsToTry.push_back("shaders/" + shaderFile);

    std::string absPath2 = currentPath.string() + "\\shaders\\" + shaderFile;
    std::string absPath4 = currentPath.string() + "/shaders/" + shaderFile;
    std::ifstream test2(absPath2, std::ios::binary);
    if (test2.good()) pathsToTry.push_back(absPath2);
    std::ifstream test4(absPath4, std::ios::binary);
    if (test4.good()) pathsToTry.push_back(absPath4);

    std::string absPath1 = currentPath.string() + "\\resources\\shaders\\" + shaderFile;
    std::string absPath3 = currentPath.string() + "/resources/shaders/" + shaderFile;
    std::ifstream test1(absPath1, std::ios::binary);
    if (test1.good()) pathsToTry.push_back(absPath1);
    std::ifstream test3(absPath3, std::ios::binary);
    if (test3.good()) pathsToTry.push_back(absPath3);

    pathsToTry.push_back("./resources/shaders/" + shaderFile);
    pathsToTry.push_back("../resources/shaders/" + shaderFile);
    pathsToTry.push_back("resources/shaders/" + shaderFile);
#endif
    pathsToTry.push_back(shaderFile);

    // Try each path
    std::string foundPath;
    constexpr size_t maxShaderSize = 1000000;
    size_t shaderSize = 0;
    std::vector<uint32_t> result;

    for (const auto& path : pathsToTry) {
        std::ifstream is(path, std::ios::binary | std::ios::in | std::ios::ate);
        if (is.is_open()) {
            shaderSize = static_cast<size_t>(is.tellg());
            if (shaderSize > 0 && shaderSize < maxShaderSize && (shaderSize % sizeof(uint32_t)) == 0) {
                result.resize(shaderSize / sizeof(uint32_t));
                is.seekg(0, std::ios::beg);
                is.read(reinterpret_cast<char*>(result.data()), shaderSize);
                is.close();
                foundPath = path;
                LOGI("VkShader: Loaded shader from: %s (%zu bytes)", path.c_str(), shaderSize);
                break;
            } else {
                is.close();
                LOGW("VkShader: Invalid shader size: %s (%zu bytes)", path.c_str(), shaderSize);
            }
        }
    }

    if (result.empty()) {
        LOGE("VkShader: Failed to open shader file: %s", filename.c_str());
    }

    return result;
}

VkShaderStageFlagBits VkShader::shaderStageToVkFlag(Core::ShaderStage stage) {
    switch (stage) {
        case Core::ShaderStage::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case Core::ShaderStage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case Core::ShaderStage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case Core::ShaderStage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case Core::ShaderStage::TessControl:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case Core::ShaderStage::TessEvaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default:
            return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

// =========================================================================
// VkShaderProgram Implementation
// =========================================================================

VkShaderProgram::VkShaderProgram()
    : m_vertexShader(std::make_unique<VkShader>())
    , m_fragmentShader(std::make_unique<VkShader>())
{
}

bool VkShaderProgram::loadFromFiles(VkDevice device,
                                    const std::string& vertFilename,
                                    const std::string& fragFilename) {
    if (!m_vertexShader->loadFromSPIRVFile(device, vertFilename,
                                           Core::ShaderStage::Vertex, "main")) {
        return false;
    }

    if (!m_fragmentShader->loadFromSPIRVFile(device, fragFilename,
                                             Core::ShaderStage::Fragment, "main")) {
        return false;
    }

    return true;
}

bool VkShaderProgram::loadFromSPIRV(VkDevice device,
                                    const std::vector<char>& vertCode,
                                    const std::vector<char>& fragCode) {
    // Convert char vectors to uint32_t vectors
    std::vector<uint32_t> vertSPIRV(vertCode.size() / sizeof(uint32_t));
    std::memcpy(vertSPIRV.data(), vertCode.data(), vertCode.size());

    std::vector<uint32_t> fragSPIRV(fragCode.size() / sizeof(uint32_t));
    std::memcpy(fragSPIRV.data(), fragCode.data(), fragCode.size());

    if (!m_vertexShader->loadFromSPIRV(device, vertSPIRV,
                                       Core::ShaderStage::Vertex, "main")) {
        return false;
    }

    if (!m_fragmentShader->loadFromSPIRV(device, fragSPIRV,
                                        Core::ShaderStage::Fragment, "main")) {
        return false;
    }

    return true;
}

bool VkShaderProgram::loadFromLoader(VkDevice device, ShaderLoaderFunc loader,
                                     const char* vertShaderName, const char* fragShaderName) {
    if (!loader) {
        LOGE("VkShaderProgram: Shader loader function is null");
        return false;
    }

    if (!m_vertexShader->loadFromLoader(device, loader, vertShaderName,
                                         Core::ShaderStage::Vertex, "main")) {
        return false;
    }

    if (!m_fragmentShader->loadFromLoader(device, loader, fragShaderName,
                                           Core::ShaderStage::Fragment, "main")) {
        return false;
    }

    return true;
}

bool VkShaderProgram::loadFromSPIRVU32(VkDevice device,
                                       const std::vector<uint32_t>& vertCode,
                                       const std::vector<uint32_t>& fragCode) {
    if (!m_vertexShader->loadFromSPIRV(device, vertCode,
                                       Core::ShaderStage::Vertex, "main")) {
        return false;
    }

    if (!m_fragmentShader->loadFromSPIRV(device, fragCode,
                                        Core::ShaderStage::Fragment, "main")) {
        return false;
    }

    return true;
}

void VkShaderProgram::cleanup(VkDevice device) {
    if (m_vertexShader) {
        m_vertexShader->cleanup(device);
    }
    if (m_fragmentShader) {
        m_fragmentShader->cleanup(device);
    }
}

} // namespace AgenUIEngine
