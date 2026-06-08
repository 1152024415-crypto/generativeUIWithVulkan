/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Platform-Independent Render Commands
 * =====================================
 * Defines render commands that work across all graphics backends.
 * Only contains semantic data (pixel coordinates, colors, text, etc.).
 * No backend-specific pointers or GPU data.
 */

#ifndef AGENUI_RENDER_COMMAND_H
#define AGENUI_RENDER_COMMAND_H

#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

// Include GLM for vec2, vec3, mat4 types
#include "thirdparty/glm/glm.hpp"

namespace AgenUIEngine::Core {

/**
 * @brief Draw command types
 */
enum class DrawType {
    Rectangle,
    RoundedRectangle,
    Circle,
    GlassRoundedRectangle,
    Polygon,
    Text,
    MultiLineText,
    TextPartial,
    Image,
    Custom
};

/**
 * @brief Sorting key for render command batching
 *
 * Layout: [Transparent:1][DrawType:4][Depth:19][InsertionOrder:40]
 * - Opaque objects: grouped by DrawType (fewer pipeline switches),
 *   then front-to-back by depth (less overdraw)
 * - Transparent objects: back-to-front by depth (correct blending)
 */
struct SortKey {
    uint64_t value = 0;

    static SortKey makeOpaque(DrawType type, float depth, uint64_t order) {
        uint64_t t = 0;
        uint64_t dt = static_cast<uint64_t>(type) & 0xF;
        uint64_t d = static_cast<uint64_t>(depth * 524287.0f) & 0x7FFFF;
        return SortKey{(t << 63) | (dt << 59) | (d << 40) | (order & 0xFFFFFFFFFF)};
    }

    static SortKey makeTransparent(float depth, uint64_t order) {
        uint64_t t = 1;
        uint64_t d = static_cast<uint64_t>((1.0f - depth) * 524287.0f) & 0x7FFFF;
        return SortKey{(t << 63) | (0xFULL << 59) | (d << 40) | (order & 0xFFFFFFFFFF)};
    }

    bool operator<(const SortKey& o) const { return value < o.value; }
};

/**
 * @brief Platform-independent render command
 *
 * This struct contains all semantic information needed to render an object.
 * No backend-specific data (no pipeline/buffer/texture pointers).
 * The backend renderer interprets these commands into actual GPU draw calls.
 */
struct RenderCommand {
    // -------------------------------------------------------------------------
    // Type and Sorting
    // -------------------------------------------------------------------------
    DrawType type = DrawType::Rectangle;
    float depth = 0.0f;
    bool transparent = false;
    SortKey sortKey;
    uint64_t insertionOrder = 0;

    // -------------------------------------------------------------------------
    // Geometry (pixel coordinates)
    // -------------------------------------------------------------------------
    glm::vec2 position;
    glm::vec2 size;
    glm::vec3 color;
    float alpha = 1.0f;
    float cornerRadius = 0.0f;
    float circleRadius = 0.0f;    // for DrawType::Circle

    // -------------------------------------------------------------------------
    // Text
    // -------------------------------------------------------------------------
    std::string text;
    uint32_t fontSize = 0;
    std::string fontFamily;          // logical font ID (e.g. "default", "bold", "mono")
    float maxWidth = 0.0f;
    float lineHeight = 0.0f;
    float glowWidth = 0.0f;
    float glowIntensity = 0.0f;
    bool hasGradient = false;
    glm::vec3 gradientEndColor{0.0f};
    int gradientDirection = 0;  // 0=vertical, 1=horizontal
    float strokeWidth = 0.0f;
    glm::vec3 strokeColor{0.0f};

    // Stream text partial draw
    std::string textPartialKey;     // cache key for prepared layout
    uint32_t visibleChars = 0;      // number of glyphs to draw

    // -------------------------------------------------------------------------
    // Polygon (filled triangle fan)
    // -------------------------------------------------------------------------
    std::vector<glm::vec2> polygonVertices;  // perimeter vertices in pixel coords
    glm::vec2 polygonCenter;                  // fan center in pixel coords

    // -------------------------------------------------------------------------
    // Image
    // -------------------------------------------------------------------------
    std::string imagePath;
    float rotation = 0.0f;                                  // degrees, around center
    std::vector<glm::vec2> clipVertices;                    // polygon clip vertices (pixel coords)
    glm::vec2 clipCenter;                                    // polygon clip center (pixel coords)

    // -------------------------------------------------------------------------
    // Sorting helper
    // -------------------------------------------------------------------------
    bool operator<(const RenderCommand& other) const {
        return sortKey < other.sortKey;
    }
};

/**
 * @brief Frame statistics
 */
struct FrameStats {
    uint32_t drawCalls = 0;
    uint32_t batchedDrawCalls = 0;
    uint32_t triangleCount = 0;
    uint32_t commandCount = 0;

    void clear() {
        drawCalls = 0;
        batchedDrawCalls = 0;
        triangleCount = 0;
        commandCount = 0;
    }
};

} // namespace AgenUIEngine::Core

#endif // AGENUI_RENDER_COMMAND_H
