/*
 * TextLayoutEngine — Pure CPU text layout (no Vulkan dependency).
 *
 * Responsibilities:
 * - Font loading and glyph rasterization (via FontManager)
 * - Text atlas management (via TextAtlas)
 * - Prepare → Layout → buildDrawData pipeline
 * - Atlas dirty tracking for GPU upload
 *
 * Split from the original TextRenderer which mixed CPU layout with GPU operations.
 * All Vulkan/GPU code lives in TextPipeline (backend/vulkan/PipelineManager).
 */

#ifndef AGENUI_ENGINE_TEXT_LAYOUT_ENGINE_H
#define AGENUI_ENGINE_TEXT_LAYOUT_ENGINE_H

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define AGENUI_PLATFORM_WINDOWS 1
#elif defined(__OHOS__) || defined(AGENUI_PLATFORM_HARMONYOS)
    #define AGENUI_PLATFORM_HARMONYOS 1
#endif

#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include "thirdparty/glm/glm.hpp"
#include "text_layout/TextLayout.h"
#include "MsdfAtlasData.h"

#if AGENUI_PLATFORM_HARMONYOS
struct NativeResourceManager;
#endif

namespace AgenUIEngine {

// Forward declarations
class FontManager;
class TextAtlas;
struct Glyph;
class TextLayoutManager;
struct PreparedTextWithSegments;
struct LayoutLine;
struct LayoutLinesResult;

// Engine-agnostic types from core
namespace Core { struct TextBlock; }

// Forward declaration for styled layout context (defined in .cpp)
struct StyledLayoutData;

/**
 * Pure CPU text layout engine.
 * Handles font management, glyph rasterization, atlas placement,
 * text layout, and generation of GPU-ready draw data (TextDrawData).
 * No Vulkan types or GPU operations.
 */
class TextLayoutEngine {
public:
    TextLayoutEngine();
    ~TextLayoutEngine();

    // ============================================================
    // Lifecycle (no Vulkan parameters)
    // ============================================================

#if AGENUI_PLATFORM_HARMONYOS
    /**
     * Initialize with HarmonyOS resource manager.
     * Creates FontManager + TextAtlas, but no GPU resources.
     */
    bool initialize(void* resourceManager, const std::string& fontName);
#endif

#if AGENUI_PLATFORM_WINDOWS
    /**
     * Initialize from file path (Windows).
     * Creates FontManager + TextAtlas, but no GPU resources.
     */
    bool initializeFromFile(const char* fontPath);
#endif

    /** Cleanup CPU-side resources. */
    void cleanup();

    bool isInitialized() const { return m_initialized; }

    // ============================================================
    // Prepare → Layout → buildDrawData pipeline
    // ============================================================

    /**
     * Prepare text for rendering (expensive: glyph lookup, atlas placement).
     * Determines render path (MSDF/SDF/Bitmap), resolves glyphs, places in atlas.
     */
    std::unique_ptr<PreparedGlyphRun> prepareText(const std::string& text, uint32_t fontSize);

    /**
     * Layout a single line of prepared glyphs (cheap: kerning, positioning).
     * Pure arithmetic — no GPU operations.
     */
    TextLayoutResult layoutSingleLine(const PreparedGlyphRun& run);

    /**
     * Layout a multi-line block of text.
     * Pure arithmetic — no GPU operations.
     */
    TextLayoutResult layoutMultiLine(
        const PreparedTextWithSegments& prepared,
        const LayoutLinesResult& layoutResult,
        uint32_t fontSize,
        float lineHeight);

    /**
     * Build GPU-ready draw data from a prepared run + layout result.
     * Generates NDC vertices, indices, and push constants.
     * Pure CPU — no Vulkan calls.
     */
    TextDrawData buildDrawData(
        const PreparedGlyphRun& run,
        const TextLayoutResult& layout,
        const glm::vec2& ndcPosition,
        const glm::vec3& color);

    /**
     * Layout multi-styled block text (pure CPU, no GPU dependency).
     *
     * Returns: combined glyph layout, per-segment push constant / index metadata,
     * and decorative rectangles (backgrounds, HR lines).
     */
    std::unique_ptr<StyledLayoutResult> layoutStyledBlocks(
        const std::vector<Core::TextBlock>& blocks,
        uint32_t baseFontSize,
        float maxWidth,               // design pixels, 0 = no wrap
        const glm::vec2& ndcPos,      // text origin in NDC
        float normX, float normY,     // pixel→NDC factors
        float scale);                 // design→scaled pixel factor

    /**
     * Layout plain (single-style) text with auto-wrapping (pure CPU).
     * Handles single-line and multi-line (\n-separated) text.
     * Returns prepared glyphs + layout result.
     */
    struct PlainLayoutResult {
        std::unique_ptr<PreparedGlyphRun> prepared;
        std::unique_ptr<TextLayoutResult> layout;
        uint32_t totalGlyphs = 0;
    };

    std::unique_ptr<PlainLayoutResult> layoutPlainText(
        const std::string& text,
        uint32_t fontSize,
        float maxWidth,               // design pixels, 0 = no wrap
        float scale,                  // design→scaled pixel factor
        const std::string& fontId = "default");

    // ============================================================
    // Atlas management (CPU side)
    // ============================================================

    /** Pre-cache glyphs for a text string and upload atlas once. */
    void precacheText(const std::string& text, uint32_t fontSize);

    /** Check if bitmap atlas needs GPU upload. */
    bool hasPendingAtlasUpload() const { return m_atlasNeedsUpdate; }

    /** Get raw bitmap atlas data for GPU upload. */
    const uint8_t* getAtlasData() const;

    /** Get bitmap atlas dimensions. */
    uint32_t getAtlasWidth() const;
    uint32_t getAtlasHeight() const;

    /** Mark bitmap atlas as uploaded to GPU. */
    void markAtlasUploaded() { m_atlasNeedsUpdate = false; }

    /** Check if SDF atlas needs GPU upload. */
    bool hasPendingSDFAtlasUpload() const { return m_sdfAtlasNeedsUpdate; }

    /** Get raw SDF atlas data for GPU upload. */
    const uint8_t* getSDFAtlasData() const;

    /** Mark SDF atlas as uploaded to GPU. */
    void markSDFAtlasUploaded() { m_sdfAtlasNeedsUpdate = false; }

    // ============================================================
    // MSDF data access (CPU only)
    // ============================================================

    /** Get the MSDF atlas data for glyph lookup. May return nullptr if not loaded. */
    const MsdfAtlasData* getMsdfAtlasData() const { return m_msdfAtlasData ? &m_msdfAtlasData.value() : nullptr; }

    /**
     * Try to load MSDF atlas data from JSON file (CPU only, no GPU texture).
     * Returns true if loaded successfully.
     */
    bool loadMsdfDataFromJson(const std::string& jsonPath);

    // ============================================================
    // State / Effects
    // ============================================================

    /** Set normalization factors for converting pixel metrics to NDC. */
    void setNormalizationFactors(float normW, float normH) {
        m_normWidth = normW;
        m_normHeight = normH;
    }

    float getNormWidth() const { return m_normWidth; }
    float getNormHeight() const { return m_normHeight; }

    void setGlowIntensity(float intensity) { m_glowIntensity = intensity; }
    float getGlowIntensity() const { return m_glowIntensity; }

    void setGlowWidth(float width) { m_glowWidth = width; }
    float getGlowWidth() const { return m_glowWidth; }

    void setTextGradient(bool hasGradient, const glm::vec3& endColor, int direction = 0) {
        m_hasGradient = hasGradient;
        m_gradientEndColor = endColor;
        m_gradientDirection = direction;
    }
    bool hasGradient() const { return m_hasGradient; }
    const glm::vec3& getGradientEndColor() const { return m_gradientEndColor; }
    int getGradientDirection() const { return m_gradientDirection; }

    void setTextStroke(float width, const glm::vec3& color) {
        m_strokeWidth = width;
        m_strokeColor = color;
    }
    float getStrokeWidth() const { return m_strokeWidth; }
    const glm::vec3& getStrokeColor() const { return m_strokeColor; }

    // ============================================================
    // Accessors
    // ============================================================

    FontManager* getFontManager() const { return m_fontManager.get(); }

    /** Get render statistics. */
    struct TextStats {
        uint32_t glyphsDrawn = 0;
        uint32_t atlasUploads = 0;
        uint32_t drawCalls = 0;
    };
    const TextStats& getStats() const { return m_stats; }
    TextStats& getStatsMutable() { return m_stats; }

private:
    // ============================================================
    // Internal helpers
    // ============================================================

    /** Ensure glyphs are placed in the bitmap atlas (CPU side). */
    void ensureGlyphsInAtlas(const std::vector<Glyph>& glyphs, uint32_t fontSize);

    /** Ensure SDF glyphs are placed in the SDF atlas (CPU side). */
    void ensureSDFGlyphsInAtlas(const std::vector<const Glyph*>& sdfGlyphs, uint32_t fontSize, uint32_t spread);

    /** Try to ensure MSDF data is available. Returns true if MSDF data is loaded. */
    bool ensureMsdfDataReady();

    // ============================================================
    // Styled layout sub-methods (layoutStyledBlocks decomposition)
    // ============================================================

    // --- Shared utilities ---

    /** Compute axis-aligned bounding box of a glyph range. */
    void computeGlyphBoundingBox(
        const std::vector<PositionedGlyph>& glyphs,
        size_t start, size_t end,
        float& minX, float& maxX, float& minY, float& maxY);

    /** Build push constants (projection + italic shear + color) for a segment. */
    void buildSegmentPushConstants(
        StyledSegmentInfo& si,
        const glm::vec2& ndcPos, float normY,
        bool italic, const std::vector<PositionedGlyph>& glyphs);

    // --- Block dispatch ---

    void layoutHorizontalRule(StyledLayoutData& ctx,
                              const Core::TextBlock& block);

    void layoutTableBlock(StyledLayoutData& ctx,
                          const std::vector<Core::TextBlock>& blocks,
                          size_t startIdx, size_t endIdx);

    void layoutGenericBlock(StyledLayoutData& ctx,
                            const Core::TextBlock& block,
                            size_t blockIdx);

    void flushBlockquoteBorders(StyledLayoutData& ctx,
                                const std::vector<Core::TextBlock>& blocks);

    // --- Table sub-steps ---

    void measureTableColumns(StyledLayoutData& ctx,
                             const std::vector<Core::TextBlock>& blocks,
                             size_t startIdx, size_t endIdx,
                             int colCount, float tableFontSize,
                             std::vector<float>& colMaxWidth);

    void distributeColumnWidths(float tableMaxWidth, int colCount,
                                const std::vector<float>& colMaxWidth,
                                float cellPadding,
                                std::vector<float>& colStartX,
                                std::vector<float>& colWidths);

    void layoutTableCells(StyledLayoutData& ctx,
                          const std::vector<Core::TextBlock>& blocks,
                          size_t startIdx, size_t endIdx,
                          int colCount, float tableFontSize, float tableLineHeight,
                          const std::vector<float>& colStartX,
                          const std::vector<float>& colWidths,
                          const std::vector<int>& columnAligns,
                          float cellPadding,
                          float& tableWidth,
                          uint32_t& firstRowGlyphCount);

    void generateTableDecorations(StyledLayoutData& ctx,
                                  int colCount, float tableLineHeight,
                                  const std::vector<float>& colStartX,
                                  const std::vector<float>& colWidths,
                                  float cellPadding,
                                  float actualTableWidth,
                                  float tableYStart, uint32_t tableCharStart,
                                  uint32_t firstRowGlyphCount, uint32_t tableTotalGlyphs);

    // --- Generic block sub-steps ---

    void wrapBlockGlyphs(StyledLayoutData& ctx,
                         size_t blockGlyphStart,
                         uint32_t scaledBlockFontSize);

    void applyBlockIndent(StyledLayoutData& ctx,
                          const Core::TextBlock& block,
                          size_t blockGlyphStart);

    void generateInlineCodeBackgrounds(StyledLayoutData& ctx);

    void generateCodeBlockBackground(StyledLayoutData& ctx,
                                     const Core::TextBlock& block,
                                     size_t blockGlyphStart,
                                     uint32_t blockCharStart);

    void generateUnderline(StyledLayoutData& ctx,
                           const Core::TextBlock& block,
                           size_t blockGlyphStart,
                           uint32_t blockCharStart,
                           uint32_t scaledBlockFontSize);

    // ============================================================
    // Member Variables
    // ============================================================

    // Font management (CPU)
    std::unique_ptr<FontManager> m_fontManager;
    std::unique_ptr<TextAtlas> m_textAtlas;

    // SDF atlas (CPU side)
    std::unique_ptr<TextAtlas> m_sdfAtlas;

    // MSDF data (CPU only — no VkTexture here)
    std::optional<MsdfAtlasData> m_msdfAtlasData;
    bool m_msdfDataInitAttempted = false;
    std::string m_msdfJsonSearchDir;  // directory to search for msdf_atlas.json

    // Normalization factors: 2.0 / swapchain_extent
    float m_normWidth = 2.0f / 1920.0f;
    float m_normHeight = 2.0f / 1080.0f;

    // State
    bool m_initialized = false;
    bool m_atlasNeedsUpdate = false;
    bool m_sdfAtlasNeedsUpdate = false;
    std::unordered_set<uint64_t> m_glyphsInAtlas;
    std::unordered_set<uint64_t> m_sdfGlyphsInAtlas;

    // Effects state
    float m_glowIntensity = 0.0f;
    float m_glowWidth = 0.0f;
    bool m_hasGradient = false;
    glm::vec3 m_gradientEndColor{0.0f};
    int m_gradientDirection = 0;
    float m_strokeWidth = 0.0f;
    glm::vec3 m_strokeColor{0.0f};

    // SDF rendered at fixed small fontSize for performance
    static constexpr uint32_t SDF_RENDER_FONT_SIZE = 64;
    static constexpr uint32_t SDF_SPREAD = 8;

    // Statistics
    TextStats m_stats;

    // Reusable vectors (avoid per-frame allocations)
    std::vector<float> m_glyphWidths;
    std::vector<float> m_glyphHeights;
    std::vector<float> m_glyphBitmapTops;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_TEXT_LAYOUT_ENGINE_H
