/*
 * MsdfAtlasData — Pure CPU MSDF atlas metadata (no Vulkan dependency).
 * Holds glyph metrics parsed from msdf-atlas-gen JSON output.
 * Used by TextLayoutEngine for MSDF glyph positioning.
 * The GPU texture (VkTexture) lives in MsdfAtlas in backend/vulkan/.
 */

#ifndef AGENUI_ENGINE_MSDF_ATLAS_DATA_H
#define AGENUI_ENGINE_MSDF_ATLAS_DATA_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace AgenUIEngine {

/**
 * Metrics for a single MSDF glyph.
 *   planeBounds = rendering rectangle relative to pen baseline, in em units.
 *   atlasBounds = texture region in atlas, in pixels (yOrigin = bottom).
 *   Glyphs without geometry (e.g. space) have hasBounds == false.
 */
struct MsdfGlyph {
    uint32_t codepoint = 0;
    float advance = 0.0f;
    float planeLeft = 0.0f, planeBottom = 0.0f, planeRight = 0.0f, planeTop = 0.0f;
    float atlasLeft = 0.0f, atlasBottom = 0.0f, atlasRight = 0.0f, atlasTop = 0.0f;
    bool hasBounds = false;
};

/**
 * Pure CPU representation of MSDF atlas metadata.
 * Parses JSON from msdf-atlas-gen and provides glyph lookup.
 * No Vulkan, no GPU texture — fully backend-agnostic.
 */
class MsdfAtlasData {
public:
    MsdfAtlasData() = default;
    ~MsdfAtlasData() = default;

    /**
     * Parse MSDF atlas metadata from raw JSON bytes.
     * @param jsonBytes Raw bytes of the atlas JSON file
     * @return true on success
     */
    bool loadFromJson(const std::vector<uint8_t>& jsonBytes);

    /** Look up a glyph by Unicode codepoint. Returns nullptr if not found. */
    const MsdfGlyph* getGlyph(uint32_t codepoint) const;

    /** Atlas dimensions (from JSON "atlas" section). */
    uint32_t getAtlasWidth()  const { return m_width; }
    uint32_t getAtlasHeight() const { return m_height; }

    /** Distance range for SDF → shader pxRange. */
    float getDistanceRange() const { return m_distanceRange; }

    /** Font metrics. */
    float getLineHeight()  const { return m_lineHeight; }
    float getAscender()    const { return m_ascender; }

    /** Whether data was loaded successfully. */
    bool isLoaded() const { return m_loaded; }

private:
    std::unordered_map<uint32_t, MsdfGlyph> m_glyphs;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    float    m_distanceRange = 8.0f;
    float    m_lineHeight = 1.0f;
    float    m_ascender = 0.0f;
    bool     m_loaded = false;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_MSDF_ATLAS_DATA_H
