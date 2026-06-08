#ifndef AGENUI_TEXT_LAYOUT_H
#define AGENUI_TEXT_LAYOUT_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <unordered_map>

#include "TextAnalysis.h"
#include "TextMeasurement.h"
#include "TextLineBreak.h"
#include "UnicodeUtils.h"
#include "thirdparty/glm/glm.hpp"

namespace AgenUIEngine {

class FontManager;

// ============================================================
// Render path for glyph runs (GPU-independent)
// ============================================================

/** Which rendering pipeline to use for a glyph run. */
enum class TextRenderPath {
    Bitmap,   // Normal bitmap atlas
    SDF,      // Signed Distance Field (legacy glow/stroke)
    MSDF      // Multi-channel SDF (high-quality glow/stroke)
};

// ============================================================
// Unified glyph types for prepare → layout → render pipeline
// ============================================================

/**
 * Resolved glyph metrics in display-pixel space.
 * Created by prepareText(), consumed by layoutText().
 * All metrics are normalized to display fontSize regardless of render path.
 */
struct GlyphInfo {
    uint32_t codepoint = 0;
    bool hasBitmap = false;   // false for whitespace / control chars

    // Position-independent glyph metrics (all in pixels at display fontSize)
    float advance = 0.0f;     // Horizontal advance width
    float left = 0.0f;        // Horizontal offset from pen position
    float top = 0.0f;         // Vertical offset from baseline (positive = above baseline)
    float width = 0.0f;       // Glyph quad width
    float height = 0.0f;      // Glyph quad height

    // Atlas UV coordinates (pre-computed during prepare)
    float u0 = 0.0f, v0 = 0.0f;   // Top-left atlas UV
    float u1 = 0.0f, v1 = 0.0f;   // Bottom-right atlas UV
};

/**
 * A fully positioned glyph quad ready for vertex buffer building.
 * Created by layoutText(), consumed by renderText().
 * All coordinates in pixel space relative to text origin.
 */
struct PositionedGlyph {
    float x0 = 0.0f, y0 = 0.0f;   // Top-left in pixels
    float x1 = 0.0f, y1 = 0.0f;   // Bottom-right in pixels
    float u0 = 0.0f, v0 = 0.0f;   // Atlas UV top-left
    float u1 = 0.0f, v1 = 0.0f;   // Atlas UV bottom-right
    bool afterSpace = false;       // true if this glyph immediately follows a space
};

/**
 * Result of prepareText() — resolved glyphs with render path info.
 * GPU-independent: no Vulkan handles, no NDC coordinates.
 */
struct PreparedGlyphRun {
    TextRenderPath renderPath = TextRenderPath::Bitmap;
    std::vector<GlyphInfo> glyphs;
    uint32_t fontSize = 0;

    // Baseline info for layout (in display-pixel space)
    float maxBitmapTop = 0.0f;
};

/**
 * Result of layoutText() — positioned glyphs ready for GPU rendering.
 * All positions are in pixel space relative to text origin.
 */
struct TextLayoutResult {
    std::vector<PositionedGlyph> glyphs;
    float totalWidth = 0.0f;
    float totalHeight = 0.0f;
};

/**
 * GPU draw data produced by TextLayoutEngine::buildDrawData().
 * Contains everything the GPU pipeline needs — no Vulkan types.
 * Consumed by TextPipeline::executeDraw().
 */
struct TextDrawData {
    TextRenderPath renderPath = TextRenderPath::Bitmap;
    std::vector<float> vertices;       // 8-float interleaved vertex data (NDC)
    std::vector<uint16_t> indices;
    float pushConstants[32] = {};      // 128 bytes, matches shader layout
};

// ============================================================
// Styled multi-block layout types (GPU-agnostic)
// ============================================================

/** Layout metadata for a single styled segment (GPU-agnostic). */
struct StyledSegmentInfo {
    uint32_t startCharIndex = 0;
    uint32_t charCount = 0;
    uint32_t indexOffset = 0;    // offset in combined index buffer (index count)
    uint32_t indexCount = 0;
    glm::vec3 color{1.0f};
    float pushConstants[32] = {}; // includes NDC projection + italic shear + color
    TextRenderPath renderPath = TextRenderPath::Bitmap;
    std::string fontId;
};

/** A decorative rectangle (background / HR line), design-pixel coordinates. */
struct DecorativeRect {
    glm::vec2 pos{0};
    glm::vec2 size{0};
    glm::vec3 color{0};
    float radius = 0.0f;

    // Optional: character range for stream-text visibility gating.
    // Set to (0, UINT32_MAX) for "always visible" (code block bg, HR, etc.)
    uint32_t startCharIndex = 0;
    uint32_t charCount = UINT32_MAX;
};

/** Complete result of layoutStyledBlocks(). */
struct StyledLayoutResult {
    std::unique_ptr<PreparedGlyphRun> firstPrepared;
    std::unique_ptr<TextLayoutResult> combinedLayout;
    std::vector<StyledSegmentInfo> segments;
    std::vector<DecorativeRect> decorations;
    uint32_t totalGlyphs = 0;
};

// ============================================================
// Public types matching layout.ts
// ============================================================

/** Prepared text handle (opaque, width-independent). */
struct PreparedText : public PreparedLineBreakData {
    virtual ~PreparedText() = default;
};

/** Rich variant that exposes segment text for rendering. */
struct PreparedTextWithSegments : public PreparedText {
    std::vector<std::u32string> segments;   // aligned with parallel arrays
};

/** Cursor position within prepared segments. */
struct LayoutCursor {
    int32_t segmentIndex = 0;
    int32_t graphemeIndex = 0;
};

/** Result of layout() — line count and height. */
struct LayoutResult {
    int32_t lineCount = 0;
    float height = 0.0f;
};

/** A single laid-out line with text, width, and cursor positions. */
struct LayoutLine {
    std::u32string text;
    float width = 0.0f;
    LayoutCursor start;
    LayoutCursor end;
};

/** Line range without text (for walkLineRanges). */
struct LayoutLineRange {
    float width = 0.0f;
    LayoutCursor start;
    LayoutCursor end;
};

/** Result of layoutWithLines() — includes per-line details. */
struct LayoutLinesResult {
    int32_t lineCount = 0;
    float height = 0.0f;
    std::vector<LayoutLine> lines;
};

// ============================================================
// TextLayoutManager — public API facade (port of layout.ts)
// ============================================================

class TextLayoutManager {
public:
    /**
     * Construct with a reference to the FontManager for text measurement.
     */
    explicit TextLayoutManager(FontManager& fontManager);
    ~TextLayoutManager();

    // Non-copyable
    TextLayoutManager(const TextLayoutManager&) = delete;
    TextLayoutManager& operator=(const TextLayoutManager&) = delete;

    // ============================================================
    // Multiline text layout cache
    // ============================================================

    /** Cache entry combining prepared text and layout result. */
    struct MultilineCacheEntry {
        std::string text;                                  // UTF-8 original
        std::u32string text32;                             // UTF-32 decoded
        uint32_t fontSize = 0;
        float maxWidth = 0.0f;
        float lineHeight = 0.0f;
        std::unique_ptr<PreparedTextWithSegments> prepared;
        LayoutLinesResult layoutResult;
    };

    /**
     * Cached prepare + layout for multiline text.
     * Key = (text_utf8, fontSize). If maxWidth/lineHeight also match, reuse layout too.
     * Returns pointers owned by the internal cache (valid until clearMultilineCache).
     */
    bool prepareAndLayoutCached(const std::string& textUtf8,
                                uint32_t fontSize,
                                float maxWidth,
                                float lineHeight,
                                PreparedTextWithSegments*& outPrepared,
                                LayoutLinesResult*& outLayout);

    /** Clear the multiline cache. */
    void clearMultilineCache();

    // ============================================================
    // Prepare (expensive: segments text, measures widths)
    // ============================================================

    /**
     * Prepare text for layout. Returns a handle that can be laid out
     * at any width and line height.
     */
    std::unique_ptr<PreparedText> prepare(
        const std::u32string& text,
        float fontSize,
        WhiteSpaceMode whiteSpace = WhiteSpaceMode::Normal
    );

    /**
     * Rich variant that keeps segment text for rendering.
     */
    std::unique_ptr<PreparedTextWithSegments> prepareWithSegments(
        const std::u32string& text,
        float fontSize,
        WhiteSpaceMode whiteSpace = WhiteSpaceMode::Normal
    );

    // ============================================================
    // Layout (pure arithmetic, hot path)
    // ============================================================

    /** Layout prepared text → line count and height. */
    LayoutResult layout(
        const PreparedText& prepared,
        float maxWidth,
        float lineHeight
    );

    /** Layout prepared text → line count, height, and per-line details. */
    LayoutLinesResult layoutWithLines(
        const PreparedTextWithSegments& prepared,
        float maxWidth,
        float lineHeight
    );

    /** Walk line ranges with a callback. Returns line count. */
    int32_t walkLineRanges(
        const PreparedTextWithSegments& prepared,
        float maxWidth,
        std::function<void(const LayoutLineRange&)> onLine
    );

    /** Stream-based: get the next line from a cursor. Returns false if at end. */
    bool layoutNextLine(
        const PreparedTextWithSegments& prepared,
        const LayoutCursor& start,
        float maxWidth,
        LayoutLine& outLine
    );

    /** Clear all caches. */
    void clearCache();

private:
    FontManager& m_fontManager;
    TextMeasurer m_measurer;
    std::unique_ptr<GraphemeSegmenter> m_graphemeSegmenter;

    // Grapheme text cache per prepared text (for building line text)
    std::unordered_map<const PreparedTextWithSegments*,
        std::unordered_map<int32_t, std::vector<std::u32string>>> m_lineTextCaches;

    // Multiline layout cache: key = hash of (text_utf8 + fontSize)
    std::unordered_map<size_t, MultilineCacheEntry> m_multilineCache;

    /**
     * Bridge function: analyze text + measure segments → prepared data.
     * Port of measureAnalysis() from layout.ts.
     */
    void measureAnalysis(
        const TextAnalysis& analysis,
        float fontSize,
        bool includeSegments,
        PreparedText& outPrepared
    );

    /** Build line text from a range of segments. */
    std::u32string buildLineTextFromRange(
        const PreparedTextWithSegments& prepared,
        int32_t startSegmentIndex,
        int32_t startGraphemeIndex,
        int32_t endSegmentIndex,
        int32_t endGraphemeIndex
    );

    /** Get graphemes for a segment (cached). */
    const std::vector<std::u32string>& getSegmentGraphemes(
        const PreparedTextWithSegments& prepared,
        int32_t segmentIndex
    );
};

} // namespace AgenUIEngine

#endif // AGENUI_TEXT_LAYOUT_H
