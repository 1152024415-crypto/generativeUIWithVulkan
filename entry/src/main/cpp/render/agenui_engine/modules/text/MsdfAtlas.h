/*
 * MSDF Atlas loader — reads msdf-atlas-gen output (atlas.png + atlas.json)
 * and exposes glyph metrics + RGBA texture for MSDF text rendering.
 *
 * CPU metadata is delegated to MsdfAtlasData (backend-agnostic).
 * This class adds the GPU texture (VkTexture) on top.
 */

#ifndef AGENUI_ENGINE_MSDF_ATLAS_H
#define AGENUI_ENGINE_MSDF_ATLAS_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "MsdfAtlasData.h"

namespace AgenUIEngine {

class VkTexture;

class MsdfAtlas {
public:
    MsdfAtlas() = default;
    ~MsdfAtlas();

    /**
     * Load MSDF atlas from rawfile resources (HarmonyOS).
     * Creates an RGBA VkTexture from the PNG; parses metadata from the JSON.
     */
    bool loadFromRawFile(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue graphicsQueue,
                         void* resourceManager,
                         const std::string& pngName, const std::string& jsonName);

    /**
     * Load MSDF atlas from absolute file paths.
     * Used when the assets have been copied to the app's sandbox (e.g. by
     * Index.ets aboutToAppear). Does not require a resource manager.
     */
    bool loadFromFiles(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool, VkQueue graphicsQueue,
                       const std::string& pngPath, const std::string& jsonPath);

    void cleanup(VkDevice device);

    /** Delegate glyph lookup to the CPU data. */
    const MsdfGlyph* getGlyph(uint32_t codepoint) const;

    /** Access the CPU-only data (for TextLayoutEngine). */
    const MsdfAtlasData& getData() const { return m_data; }

    VkTexture* getTexture() const { return m_texture; }
    uint32_t   getAtlasWidth()  const { return m_data.getAtlasWidth(); }
    uint32_t   getAtlasHeight() const { return m_data.getAtlasHeight(); }
    float      getDistanceRange() const { return m_data.getDistanceRange(); }
    float      getLineHeight()  const { return m_data.getLineHeight(); }
    float      getAscender()    const { return m_data.getAscender(); }
    bool       isLoaded()       const { return m_data.isLoaded() && m_texture != nullptr; }

private:
    MsdfAtlasData m_data;              // CPU metadata (no Vulkan)
    VkTexture* m_texture = nullptr;    // owned GPU texture
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_MSDF_ATLAS_H
