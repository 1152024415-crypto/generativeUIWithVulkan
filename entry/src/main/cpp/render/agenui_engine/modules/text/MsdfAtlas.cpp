/*
 * MSDF Atlas loader impl.
 * CPU metadata is delegated to MsdfAtlasData; this handles GPU texture only.
 */

#include "MsdfAtlas.h"
#include "backend/vulkan/VkTexture.h"
#include "logger_common.h"

#include <vector>
#include <fstream>

#ifdef __OHOS__
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#endif

namespace AgenUIEngine {

MsdfAtlas::~MsdfAtlas() {
    // explicit cleanup() should have been called; avoid double-free here.
}

void MsdfAtlas::cleanup(VkDevice device) {
    if (m_texture) {
        m_texture->cleanup(device);
        delete m_texture;
        m_texture = nullptr;
    }
    // MsdfAtlasData will clean itself up via destructor
}

const MsdfGlyph* MsdfAtlas::getGlyph(uint32_t codepoint) const {
    return m_data.getGlyph(codepoint);
}

// Reads JSON bytes from HarmonyOS rawfile resource.
#ifdef __OHOS__
static bool readRawFileBytes(void* resourceManager, const std::string& name, std::vector<uint8_t>& out) {
    NativeResourceManager* mgr = static_cast<NativeResourceManager*>(resourceManager);
    RawFile* rawFile = OH_ResourceManager_OpenRawFile(mgr, name.c_str());
    if (!rawFile) {
        LOGE("MsdfAtlas: failed to open rawfile %s", name.c_str());
        return false;
    }
    long size = OH_ResourceManager_GetRawFileSize(rawFile);
    if (size <= 0) {
        OH_ResourceManager_CloseRawFile(rawFile);
        return false;
    }
    out.resize(static_cast<size_t>(size));
    int bytesRead = OH_ResourceManager_ReadRawFile(rawFile, out.data(), size);
    OH_ResourceManager_CloseRawFile(rawFile);
    return bytesRead == size;
}
#endif

bool MsdfAtlas::loadFromRawFile(VkDevice device, VkPhysicalDevice physicalDevice,
                                VkCommandPool commandPool, VkQueue graphicsQueue,
                                void* resourceManager,
                                const std::string& pngName, const std::string& jsonName) {
    std::vector<uint8_t> jsonBytes;
#ifdef __OHOS__
    if (!readRawFileBytes(resourceManager, jsonName, jsonBytes)) {
        LOGE("MsdfAtlas: failed to read rawfile %s", jsonName.c_str());
        return false;
    }
#else
    LOGE("MsdfAtlas: Windows platform not yet wired up for rawfile loading");
    return false;
#endif

    if (!m_data.loadFromJson(jsonBytes)) return false;

    if (m_texture) {
        m_texture->cleanup(device);
        delete m_texture;
    }
    m_texture = new VkTexture();
    if (!m_texture->loadFromRawFile(device, physicalDevice, commandPool, graphicsQueue,
                                    resourceManager, pngName, VK_FORMAT_R8G8B8A8_UNORM)) {
        LOGE("MsdfAtlas: failed to load png %s", pngName.c_str());
        delete m_texture;
        m_texture = nullptr;
        return false;
    }

    LOGI("MsdfAtlas: loaded %u glyphs, atlas %ux%u, distanceRange=%.1f (rawfile)",
         (uint32_t)m_data.getAtlasWidth(), m_data.getAtlasWidth(), m_data.getAtlasHeight(), m_data.getDistanceRange());
    return true;
}

bool MsdfAtlas::loadFromFiles(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkCommandPool commandPool, VkQueue graphicsQueue,
                              const std::string& pngPath, const std::string& jsonPath) {
    // 1. Read JSON from file system
    std::ifstream jf(jsonPath, std::ios::binary);
    if (!jf) {
        LOGE("MsdfAtlas: cannot open json file %s", jsonPath.c_str());
        return false;
    }
    std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(jf)),
                                   std::istreambuf_iterator<char>());
    if (jsonBytes.empty()) {
        LOGE("MsdfAtlas: json file empty %s", jsonPath.c_str());
        return false;
    }

    if (!m_data.loadFromJson(jsonBytes)) return false;

    // 2. Load PNG via VkTexture::loadFromFile (stb_image reads absolute path)
    if (m_texture) {
        m_texture->cleanup(device);
        delete m_texture;
    }
    m_texture = new VkTexture();
    if (!m_texture->loadFromFile(device, physicalDevice, commandPool, graphicsQueue, pngPath)) {
        LOGE("MsdfAtlas: failed to load png file %s", pngPath.c_str());
        delete m_texture;
        m_texture = nullptr;
        return false;
    }

    LOGI("MsdfAtlas: loaded %u glyphs, atlas %ux%u, distanceRange=%.1f (file)",
         (uint32_t)m_data.getAtlasWidth(), m_data.getAtlasWidth(), m_data.getAtlasHeight(), m_data.getDistanceRange());
    return true;
}

} // namespace AgenUIEngine
