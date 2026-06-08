/*
 * TextLayoutCache — backend-independent text layout cache types
 * =============================================================
 * Defines CoreSegment and TextLayoutCacheBase which hold CPU-only
 * data for stream-text rendering.  Backends (Vulkan, Metal, etc.)
 * extend TextLayoutCacheBase with GPU resource handles.
 */

#ifndef AGENUI_TEXT_LAYOUT_CACHE_H
#define AGENUI_TEXT_LAYOUT_CACHE_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include "thirdparty/glm/glm.hpp"
#include "modules/text/text_layout/TextLayout.h"

namespace AgenUIEngine::Core {

/** CPU-side styled segment data (backend-independent, no GPU shader data). */
struct CoreSegment {
    uint32_t startCharIndex = 0;
    uint32_t charCount = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    glm::vec3 color{1.0f};
    TextRenderPath renderPath = TextRenderPath::Bitmap;
    std::string fontId;
};

/** Base text layout cache (CPU data only, no GPU handles). */
struct TextLayoutCacheBase {
    std::unique_ptr<PreparedGlyphRun> prepared;
    std::unique_ptr<TextLayoutResult> layout;
    uint32_t totalGlyphs = 0;
    glm::vec2 pixelPos{0};  // Pixel coordinates (backend-independent)

    // Multi-style segments (non-empty when using the TextBlock overload)
    std::vector<CoreSegment> segments;

    // Decorative backgrounds (code blocks, HR lines) — reuse DecorativeRect
    std::vector<DecorativeRect> backgrounds;
};

} // namespace AgenUIEngine::Core

#endif // AGENUI_TEXT_LAYOUT_CACHE_H
