/*
 * MsdfAtlasData — Pure CPU MSDF atlas metadata implementation.
 */

#include "MsdfAtlasData.h"
#include "logger_common.h"
#include "thirdparty/json.hpp"

namespace AgenUIEngine {

using json = nlohmann::json;

bool MsdfAtlasData::loadFromJson(const std::vector<uint8_t>& jsonBytes) {
    json root;
    try {
        root = json::parse(jsonBytes.begin(), jsonBytes.end());
    } catch (const std::exception& e) {
        LOGE("MsdfAtlasData: json parse error: %s", e.what());
        return false;
    }

    if (!root.contains("atlas") || !root.contains("glyphs")) {
        LOGE("MsdfAtlasData: malformed json (missing atlas/glyphs)");
        return false;
    }

    const auto& atlas = root["atlas"];
    m_width          = atlas.value("width", 0u);
    m_height         = atlas.value("height", 0u);
    m_distanceRange  = atlas.value("distanceRange", 8.0f);

    if (root.contains("metrics")) {
        const auto& metrics = root["metrics"];
        m_lineHeight = metrics.value("lineHeight", 1.0f);
        m_ascender   = metrics.value("ascender", 0.928f);
    }

    m_glyphs.reserve(root["glyphs"].size());
    for (const auto& g : root["glyphs"]) {
        MsdfGlyph mg;
        mg.codepoint = g.value("unicode", 0u);
        mg.advance   = g.value("advance", 0.0f);
        if (g.contains("planeBounds") && g.contains("atlasBounds")) {
            const auto& pb = g["planeBounds"];
            const auto& ab = g["atlasBounds"];
            mg.planeLeft   = pb.value("left", 0.0f);
            mg.planeBottom = pb.value("bottom", 0.0f);
            mg.planeRight  = pb.value("right", 0.0f);
            mg.planeTop    = pb.value("top", 0.0f);
            mg.atlasLeft   = ab.value("left", 0.0f);
            mg.atlasBottom = ab.value("bottom", 0.0f);
            mg.atlasRight  = ab.value("right", 0.0f);
            mg.atlasTop    = ab.value("top", 0.0f);
            mg.hasBounds   = true;
        }
        m_glyphs[mg.codepoint] = mg;
    }

    m_loaded = true;
    LOGI("MsdfAtlasData: parsed %u glyphs, atlas %ux%u, distanceRange=%.1f",
         (uint32_t)m_glyphs.size(), m_width, m_height, m_distanceRange);
    return true;
}

const MsdfGlyph* MsdfAtlasData::getGlyph(uint32_t codepoint) const {
    auto it = m_glyphs.find(codepoint);
    return (it == m_glyphs.end()) ? nullptr : &it->second;
}

} // namespace AgenUIEngine
