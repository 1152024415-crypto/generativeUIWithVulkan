#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <mutex>

// FreeType includes
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MODULE_H

namespace AgenUIEngine {

// Glyph structure for rendering
struct Glyph {
    uint32_t codepoint = 0;     // Unicode codepoint
    uint32_t width = 0;         // Bitmap width
    uint32_t height = 0;        // Bitmap height
    int32_t bearingX = 0;       // Left bearing
    int32_t bearingY = 0;       // Top bearing
    int32_t bitmapLeft = 0;     // Bitmap left position
    int32_t bitmapTop = 0;      // Bitmap top position
    uint32_t advance = 0;       // Advance width
    uint8_t* bitmapData = nullptr;  // RGBA bitmap data
    uint32_t bitmapSize = 0;    // Total size of bitmap data
    uint32_t atlasX = 0;        // Position in texture atlas
    uint32_t atlasY = 0;        // Position in texture atlas
};

/**
 * @brief Per-font entry holding FreeType face + caches + raw data.
 * Font data must stay alive because FreeType holds a pointer into it.
 */
struct FontEntry {
    FT_Face face = nullptr;
    std::vector<uint8_t> fontData;                             // kept alive for FT_Memory_Face
    std::unordered_map<uint64_t, Glyph> glyphCache;            // (cp<<16)|size -> Glyph
    std::unordered_map<uint64_t, Glyph> sdfGlyphCache;         // SDF cache per font
    uint32_t currentSize = 0;                                  // per-font tracked FT face size
};

class FontManager {
public:
    FontManager();
    ~FontManager();

    // Initialize the font manager (FreeType library)
    bool initialize();

    // ================================================================
    // Multi-font API
    // ================================================================

    /**
     * @brief Register a font from a file path with a logical ID.
     * @param fontId  Logical name (e.g. "default", "bold", "mono").
     *                If fontId == active font, the face pointer is updated.
     * @return true on success.
     */
    bool registerFont(const std::string& fontId, const std::string& fontPath);

    /**
     * @brief Register a font from in-memory data with a specific TTC face index.
     *        Useful for loading bold/light variants from a .ttc file.
     */
    bool registerFontFromMemory(const std::string& fontId,
                                const uint8_t* fontData, size_t dataSize,
                                int faceIndex);

    /**
     * @brief Switch the active font. All subsequent rasterizeText/getGlyph/etc
     *        calls use this font until switched again.
     * @return false if fontId not found.
     */
    bool setActiveFont(const std::string& fontId);

    /** @brief Get the current active font ID. */
    const std::string& getActiveFontId() const { return m_activeFontId; }

    /**
     * @brief Resolve a HarmonyOS fontFamily name to a registered font ID.
     * Maps names like "HarmonyOS Sans SC", "sans-serif", "monospace", etc.
     * to internal font IDs ("default", "bold").
     * @return The resolved font ID, or "default" if no mapping found.
     */
    std::string resolveFontFamily(const std::string& fontFamily) const;

    /**
     * @brief Set custom alias mappings from font config.
     * These are checked before the hardcoded alias table.
     */
    void setAliases(const std::unordered_map<std::string, std::string>& aliases);

    // ================================================================
    // Legacy single-font API (kept for backward compatibility)
    // ================================================================

    /** @brief Load default font (maps to registerFont("default", path)). */
    bool loadFont(const std::string& fontPath);

    /** @brief Load default font from memory, auto-select best TTC face. */
    bool loadFontFromMemory(const uint8_t* fontData, size_t dataSize);

    // ================================================================
    // Rendering & cache API (operate on the active font)
    // ================================================================

    // Set font size
    void setFontSize(uint32_t size);

    // Rasterize text to glyphs
    const std::vector<Glyph>& rasterizeText(const std::string& text, uint32_t fontSize);

    // Get cached glyph
    const Glyph* getGlyph(uint32_t codepoint, uint32_t fontSize);

    // Clear glyph cache (active font or all)
    void clearCache();
    void clearAllCaches();

    // Update cached glyph atlas position
    void updateGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t atlasX, uint32_t atlasY);

    // Get cached glyph atlas position
    bool getGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t& outAtlasX, uint32_t& outAtlasY) const;

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // ============================================================
    // SDF rendering API (for glow effect)
    // ============================================================

    bool rasterizeCharSDF(uint32_t codepoint, uint32_t fontSize, uint32_t spread, Glyph& outGlyph);
    const Glyph* getSDFGlyph(uint32_t codepoint, uint32_t fontSize, uint32_t spread);
    void updateSDFGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t spread, uint32_t atlasX, uint32_t atlasY);
    bool getSDFGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t spread, uint32_t& outAtlasX, uint32_t& outAtlasY) const;
    void clearSDFCache();

    // ============================================================
    // Text measurement API (for multi-line layout)
    // ============================================================

    float getGlyphAdvance(char32_t codepoint, float fontSize);
    float measureTextAdvance(const std::u32string& text, float fontSize);
    float getSpaceWidth(float fontSize);
    float getKerning(uint32_t leftCodepoint, uint32_t rightCodepoint, uint32_t fontSize);

private:
    // Rasterize single character using the active face
    bool rasterizeChar(uint32_t codepoint, uint32_t fontSize, Glyph& outGlyph);

    // Convert FT_Bitmap to grayscale
    uint8_t* convertBitmapToRGBA(const FT_Bitmap& bitmap, uint32_t& outSize);

    // Select best face from TTC data (SC > TC > JP > KR > first)
    FT_Face selectBestFaceFromTTC(const uint8_t* data, size_t size);

    FT_Library m_ftLibrary = nullptr;
    uint32_t m_currentFontSize = 16;
    bool m_initialized = false;

    // Multi-font registry
    std::unordered_map<std::string, FontEntry> m_fonts;
    std::string m_activeFontId = "default";

    // Config-driven alias mappings (checked before hardcoded aliases)
    std::unordered_map<std::string, std::string> m_aliases;

    // Convenience: returns active face (nullptr if none)
    FT_Face activeFace() const;

    // Convenience: returns active glyph cache
    std::unordered_map<uint64_t, Glyph>& activeGlyphCache();
    const std::unordered_map<uint64_t, Glyph>& activeGlyphCache() const;
    std::unordered_map<uint64_t, Glyph>& activeSDFCache();
    const std::unordered_map<uint64_t, Glyph>& activeSDFCache() const;

    // Buffer for rasterizeText result
    std::vector<Glyph> m_rasterizeBuffer;
};

} // namespace AgenUIEngine

#endif // FONTMANAGER_H
