#include "FontManager.h"
#include "logger_common.h"
#include <memory>
#include <cstring>
#include <algorithm>

namespace AgenUIEngine {

FontManager::FontManager()
    : m_ftLibrary(nullptr)
    , m_currentFontSize(16)
    , m_initialized(false)
{
}

FontManager::~FontManager() {
    // Clear all glyph caches and free bitmap data
    clearAllCaches();

    // Cleanup all FreeType faces
    for (auto& pair : m_fonts) {
        if (pair.second.face) {
            FT_Done_Face(pair.second.face);
            pair.second.face = nullptr;
        }
    }
    m_fonts.clear();

    if (m_ftLibrary) {
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr;
    }

    m_initialized = false;
}

bool FontManager::initialize() {
    if (m_initialized) {
        LOGI("FontManager already initialized");
        return true;
    }

    FT_Error error = FT_Init_FreeType(&m_ftLibrary);
    if (error) {
        LOGE("FontManager: Failed to initialize FreeType library (error: %d)", error);
        return false;
    }

    LOGI("FontManager: FreeType library initialized successfully");
    m_initialized = true;
    return true;
}

// ============================================================
// Convenience helpers
// ============================================================

FT_Face FontManager::activeFace() const {
    auto it = m_fonts.find(m_activeFontId);
    return (it != m_fonts.end()) ? it->second.face : nullptr;
}

std::unordered_map<uint64_t, Glyph>& FontManager::activeGlyphCache() {
    return m_fonts[m_activeFontId].glyphCache;
}

const std::unordered_map<uint64_t, Glyph>& FontManager::activeGlyphCache() const {
    auto it = m_fonts.find(m_activeFontId);
    return it->second.glyphCache;
}

std::unordered_map<uint64_t, Glyph>& FontManager::activeSDFCache() {
    return m_fonts[m_activeFontId].sdfGlyphCache;
}

const std::unordered_map<uint64_t, Glyph>& FontManager::activeSDFCache() const {
    auto it = m_fonts.find(m_activeFontId);
    return it->second.sdfGlyphCache;
}

// ============================================================
// TTC face selection (extracted from old loadFontFromMemory)
// ============================================================

FT_Face FontManager::selectBestFaceFromTTC(const uint8_t* data, size_t size) {
    const int MAX_FACE_INDICES = 4;
    FT_Face bestFace = nullptr;
    int bestIndex = -1;
    bool foundChineseFont = false;
    bool isTTC = false;

    for (int faceIndex = 0; faceIndex < MAX_FACE_INDICES; ++faceIndex) {
        FT_Face testFace = nullptr;
        FT_Error error = FT_New_Memory_Face(m_ftLibrary, data, static_cast<FT_Long>(size), faceIndex, &testFace);

        if (error == FT_Err_Invalid_Argument) {
            if (testFace) FT_Done_Face(testFace);
            break;
        }
        if (error) {
            if (testFace) FT_Done_Face(testFace);
            continue;
        }

        if (faceIndex == 0) {
            FT_Face nextFace = nullptr;
            FT_Error nextError = FT_New_Memory_Face(m_ftLibrary, data, static_cast<FT_Long>(size), 1, &nextFace);
            if (nextError != FT_Err_Invalid_Argument) isTTC = true;
            if (nextFace) FT_Done_Face(nextFace);
        }

        FT_Select_Charmap(testFace, FT_ENCODING_UNICODE);

        FT_UInt gi1 = FT_Get_Char_Index(testFace, 0x4F60);
        FT_UInt gi2 = FT_Get_Char_Index(testFace, 0x597D);
        FT_UInt gi3 = FT_Get_Char_Index(testFace, 0x4E2D);
        bool supportsChinese = (gi1 != 0 && gi2 != 0 && gi3 != 0);

        const char* styleName = testFace->style_name ? testFace->style_name : "";
        bool isSC = (strstr(styleName, "SC") != nullptr || faceIndex == 2);

        LOGI("FontManager: Face index %d: '%s' '%s' (%ld glyphs), Chinese: %s, SC: %s",
             faceIndex,
             testFace->family_name ? testFace->family_name : "Unknown",
             styleName, testFace->num_glyphs,
             supportsChinese ? "Yes" : "No",
             isSC ? "Yes" : "No");

        if (supportsChinese) {
            if (isSC) {
                if (bestFace) FT_Done_Face(bestFace);
                bestFace = testFace;
                bestIndex = faceIndex;
                foundChineseFont = true;
                break;
            } else if (!foundChineseFont) {
                if (bestFace) FT_Done_Face(bestFace);
                bestFace = testFace;
                bestIndex = faceIndex;
                foundChineseFont = true;
            } else {
                FT_Done_Face(testFace);
            }
        } else {
            if (bestFace == nullptr && !isTTC) {
                bestFace = testFace;
                bestIndex = faceIndex;
            } else {
                FT_Done_Face(testFace);
            }
        }
    }

    if (bestFace) {
        LOGI("FontManager: Selected face index %d: '%s'",
             bestIndex, bestFace->family_name ? bestFace->family_name : "Unknown");
    }
    return bestFace;
}

// ============================================================
// Multi-font API
// ============================================================

bool FontManager::registerFont(const std::string& fontId, const std::string& fontPath) {
    if (!m_initialized) {
        LOGE("FontManager: Not initialized");
        return false;
    }

    FontEntry entry;
    FT_Error error = FT_New_Face(m_ftLibrary, fontPath.c_str(), 0, &entry.face);
    if (error) {
        LOGE("FontManager: Failed to load font '%s' from '%s' (error: %d)",
             fontId.c_str(), fontPath.c_str(), error);
        return false;
    }

    error = FT_Select_Charmap(entry.face, FT_ENCODING_UNICODE);
    if (error) {
        LOGW("FontManager: Failed to select Unicode charmap for '%s' (error: %d)", fontId.c_str(), error);
    }

    LOGI("FontManager: Registered font '%s' = '%s' (%ld glyphs)",
         fontId.c_str(),
         entry.face->family_name ? entry.face->family_name : "Unknown",
         entry.face->num_glyphs);

    // Cleanup old entry AFTER new face is successfully created (atomic replacement)
    auto it = m_fonts.find(fontId);
    if (it != m_fonts.end()) {
        if (it->second.face) FT_Done_Face(it->second.face);
        m_fonts.erase(it);
    }

    m_fonts[fontId] = std::move(entry);

    // Set char size on active font
    FT_Set_Char_Size(m_fonts[fontId].face, m_currentFontSize * 64, m_currentFontSize * 64, 72, 72);
    return true;
}

bool FontManager::registerFontFromMemory(const std::string& fontId,
                                           const uint8_t* fontData, size_t dataSize,
                                           int faceIndex) {
    if (!m_initialized) {
        LOGE("FontManager: Not initialized");
        return false;
    }
    if (!fontData || dataSize == 0) {
        LOGE("FontManager: Invalid font data for '%s'", fontId.c_str());
        return false;
    }

    FontEntry entry;
    // Copy font data so it stays alive for FreeType
    entry.fontData.assign(fontData, fontData + dataSize);

    if (faceIndex >= 0) {
        // Use the specified face index directly
        FT_Error error = FT_New_Memory_Face(m_ftLibrary,
                                             entry.fontData.data(),
                                             static_cast<FT_Long>(entry.fontData.size()),
                                             faceIndex, &entry.face);
        if (error) {
            LOGE("FontManager: Failed to load font '%s' face %d from memory (error: %d)",
                 fontId.c_str(), faceIndex, error);
            return false;
        }
        // Ensure Unicode charmap
        FT_Select_Charmap(entry.face, FT_ENCODING_UNICODE);
    } else {
        // Auto-select best face from TTC
        entry.face = selectBestFaceFromTTC(entry.fontData.data(), entry.fontData.size());
        if (!entry.face) {
            LOGE("FontManager: Failed to load font '%s' from memory", fontId.c_str());
            return false;
        }
    }

    LOGI("FontManager: Registered font '%s' from memory (face %d, '%s', %ld glyphs)",
         fontId.c_str(), faceIndex,
         entry.face->family_name ? entry.face->family_name : "Unknown",
         entry.face->num_glyphs);

    // Cleanup old entry AFTER new face is successfully created (atomic replacement)
    auto it = m_fonts.find(fontId);
    if (it != m_fonts.end()) {
        if (it->second.face) FT_Done_Face(it->second.face);
        m_fonts.erase(it);
    }

    m_fonts[fontId] = std::move(entry);

    // Set char size
    FT_Set_Char_Size(m_fonts[fontId].face, m_currentFontSize * 64, m_currentFontSize * 64, 72, 72);
    return true;
}

bool FontManager::setActiveFont(const std::string& fontId) {
    auto it = m_fonts.find(fontId);
    if (it == m_fonts.end()) {
        LOGW("FontManager: Font '%s' not found, keeping '%s'", fontId.c_str(), m_activeFontId.c_str());
        return false;
    }
    m_activeFontId = fontId;
    LOGI("FontManager: Active font set to '%s'", fontId.c_str());
    return true;
}

void FontManager::setAliases(const std::unordered_map<std::string, std::string>& aliases) {
    m_aliases = aliases;
    LOGI("FontManager: Set %zu custom aliases", aliases.size());
}

std::string FontManager::resolveFontFamily(const std::string& fontFamily) const {
    // Direct match: if the fontFamily is already a registered font ID, use it
    if (m_fonts.find(fontFamily) != m_fonts.end()) {
        return fontFamily;
    }

    // Check config-driven aliases
    auto aliasIt = m_aliases.find(fontFamily);
    if (aliasIt != m_aliases.end()) {
        if (m_fonts.find(aliasIt->second) != m_fonts.end()) {
            return aliasIt->second;
        }
    }

    LOGW("FontManager: Unknown fontFamily '%s', falling back to 'default'", fontFamily.c_str());
    return "default";
}

// ============================================================
// Legacy API — maps to multi-font "default"
// ============================================================

bool FontManager::loadFont(const std::string& fontPath) {
    return registerFont("default", fontPath);
}

bool FontManager::loadFontFromMemory(const uint8_t* fontData, size_t dataSize) {
    return registerFontFromMemory("default", fontData, dataSize, -1);
}

// ============================================================
// Rendering & cache (operate on active font)
// ============================================================

void FontManager::setFontSize(uint32_t size) {
    FT_Face face = activeFace();
    if (!face) {
        LOGE("FontManager: No font face loaded");
        return;
    }

    m_currentFontSize = size;
    auto it = m_fonts.find(m_activeFontId);
    if (it != m_fonts.end()) {
        it->second.currentSize = size;
    }
    FT_Error error = FT_Set_Char_Size(face, size * 64, size * 64, 72, 72);
    if (error) {
        LOGE("FontManager: Failed to set font size (error: %d)", error);
    }
}

const std::vector<Glyph>& FontManager::rasterizeText(const std::string& text, uint32_t fontSize) {
    m_rasterizeBuffer.clear();

    if (!m_initialized) {
        LOGE("FontManager: Not initialized");
        return m_rasterizeBuffer;
    }

    FT_Face face = activeFace();
    if (!face) {
        LOGE("FontManager: No font face loaded");
        return m_rasterizeBuffer;
    }

    {
        auto it = m_fonts.find(m_activeFontId);
        if (it == m_fonts.end() || it->second.currentSize != fontSize) {
            setFontSize(fontSize);
        }
    }

    auto& cache = activeGlyphCache();

    size_t i = 0;
    while (i < text.size()) {
        uint32_t codepoint = 0;
        uint8_t firstByte = static_cast<uint8_t>(text[i]);

        if (firstByte < 0x80) {
            codepoint = firstByte;
            i += 1;
        } else if ((firstByte & 0xE0) == 0xC0) {
            if (i + 1 >= text.size()) break;
            codepoint = ((firstByte & 0x1F) << 6) | (static_cast<uint8_t>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((firstByte & 0xF0) == 0xE0) {
            if (i + 2 >= text.size()) break;
            codepoint = ((firstByte & 0x0F) << 12) |
                       ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 6) |
                       (static_cast<uint8_t>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((firstByte & 0xF8) == 0xF0) {
            if (i + 3 >= text.size()) break;
            codepoint = ((firstByte & 0x07) << 18) |
                       ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 12) |
                       ((static_cast<uint8_t>(text[i + 2]) & 0x3F) << 6) |
                       (static_cast<uint8_t>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            i += 1;
            continue;
        }

        uint64_t cacheKey = (static_cast<uint64_t>(codepoint) << 16) | fontSize;
        auto it = cache.find(cacheKey);

        if (it != cache.end()) {
            m_rasterizeBuffer.push_back(it->second);
        } else {
            Glyph glyph;
            if (rasterizeChar(codepoint, fontSize, glyph)) {
                cache[cacheKey] = glyph;
                m_rasterizeBuffer.push_back(glyph);
            }
        }
    }

    return m_rasterizeBuffer;
}

const Glyph* FontManager::getGlyph(uint32_t codepoint, uint32_t fontSize) {
    if (!m_initialized) return nullptr;

    auto& cache = activeGlyphCache();
    uint64_t cacheKey = (static_cast<uint64_t>(codepoint) << 16) | fontSize;
    auto it = cache.find(cacheKey);

    if (it != cache.end()) {
        return &(it->second);
    }

    // Cache miss — rasterize on demand
    Glyph glyph;
    if (rasterizeChar(codepoint, fontSize, glyph)) {
        cache[cacheKey] = glyph;
        return &(cache[cacheKey]);
    }

    return nullptr;
}

bool FontManager::rasterizeChar(uint32_t codepoint, uint32_t fontSize, Glyph& outGlyph) {
    FT_Face face = activeFace();
    if (!face) {
        LOGE("FontManager: No font face loaded");
        return false;
    }

    FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
    if (glyphIndex == 0) {
        return false;
    }

    FT_Error error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER);
    if (error) {
        LOGE("FontManager: Failed to load glyph (error: %d)", error);
        return false;
    }

    FT_GlyphSlot slot = face->glyph;

    outGlyph.codepoint = codepoint;
    outGlyph.width = slot->bitmap.width;
    outGlyph.height = slot->bitmap.rows;
    outGlyph.bearingX = slot->bitmap_left;
    outGlyph.bearingY = slot->bitmap_top;
    outGlyph.bitmapLeft = slot->bitmap_left;
    outGlyph.bitmapTop = slot->bitmap_top;
    outGlyph.advance = static_cast<uint32_t>(slot->advance.x >> 6);

    uint32_t bitmapSize = 0;
    outGlyph.bitmapData = convertBitmapToRGBA(slot->bitmap, bitmapSize);
    outGlyph.bitmapSize = bitmapSize;
    outGlyph.atlasX = 0;
    outGlyph.atlasY = 0;

    // Accept glyphs with no bitmap (e.g., space) as long as they have advance.
    // The layout engine uses advance to position subsequent characters.
    return true;
}

uint8_t* FontManager::convertBitmapToRGBA(const FT_Bitmap& bitmap, uint32_t& outSize) {
    if (bitmap.width == 0 || bitmap.rows == 0) {
        return nullptr;
    }

    uint32_t width = bitmap.width;
    uint32_t height = bitmap.rows;
    outSize = width * height;
    uint8_t* grayData = new uint8_t[outSize];

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t dstIndex = y * width;
        uint32_t srcIndex = y * bitmap.pitch;

        if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
            for (uint32_t x = 0; x < width; ++x) {
                grayData[dstIndex + x] = bitmap.buffer[srcIndex + x];
            }
        } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
            for (uint32_t x = 0; x < width; ++x) {
                uint8_t byte = bitmap.buffer[srcIndex + x / 8];
                uint8_t bit = 7 - (x % 8);
                grayData[dstIndex + x] = (byte & (1 << bit)) ? 255 : 0;
            }
        } else {
            memset(&grayData[dstIndex], 0, width);
        }
    }

    return grayData;
}

void FontManager::clearCache() {
    auto it = m_fonts.find(m_activeFontId);
    if (it == m_fonts.end()) return;

    for (auto& pair : it->second.glyphCache) {
        if (pair.second.bitmapData) {
            delete[] pair.second.bitmapData;
            pair.second.bitmapData = nullptr;
        }
    }
    it->second.glyphCache.clear();
    clearSDFCache();
}

void FontManager::clearAllCaches() {
    for (auto& fontPair : m_fonts) {
        for (auto& glyphPair : fontPair.second.glyphCache) {
            if (glyphPair.second.bitmapData) {
                delete[] glyphPair.second.bitmapData;
                glyphPair.second.bitmapData = nullptr;
            }
        }
        fontPair.second.glyphCache.clear();

        for (auto& sdfPair : fontPair.second.sdfGlyphCache) {
            if (sdfPair.second.bitmapData) {
                delete[] sdfPair.second.bitmapData;
                sdfPair.second.bitmapData = nullptr;
            }
        }
        fontPair.second.sdfGlyphCache.clear();
    }
}

void FontManager::updateGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t atlasX, uint32_t atlasY) {
    uint64_t cacheKey = (static_cast<uint64_t>(codepoint) << 16) | fontSize;
    auto& cache = activeGlyphCache();
    auto it = cache.find(cacheKey);
    if (it != cache.end()) {
        it->second.atlasX = atlasX;
        it->second.atlasY = atlasY;
    }
}

bool FontManager::getGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t& outAtlasX, uint32_t& outAtlasY) const {
    uint64_t cacheKey = (static_cast<uint64_t>(codepoint) << 16) | fontSize;
    auto it = m_fonts.find(m_activeFontId);
    if (it == m_fonts.end()) return false;
    auto git = it->second.glyphCache.find(cacheKey);
    if (git != it->second.glyphCache.end()) {
        outAtlasX = git->second.atlasX;
        outAtlasY = git->second.atlasY;
        return true;
    }
    return false;
}

// ============================================================
// Text measurement API
// ============================================================

float FontManager::getGlyphAdvance(char32_t codepoint, float fontSize) {
    FT_Face face = activeFace();
    if (!face || !m_initialized) return 0.0f;

    uint32_t size = static_cast<uint32_t>(fontSize);
    {
        auto it = m_fonts.find(m_activeFontId);
        if (it == m_fonts.end() || it->second.currentSize != size) {
            setFontSize(size);
        }
    }

    FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
    if (glyphIndex == 0) return 0.0f;

    FT_Error error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_BITMAP | FT_LOAD_FORCE_AUTOHINT);
    if (error) return 0.0f;

    return static_cast<float>(face->glyph->advance.x >> 6);
}

float FontManager::measureTextAdvance(const std::u32string& text, float fontSize) {
    FT_Face face = activeFace();
    if (text.empty() || !face || !m_initialized) return 0.0f;

    uint32_t size = static_cast<uint32_t>(fontSize);
    {
        auto it = m_fonts.find(m_activeFontId);
        if (it == m_fonts.end() || it->second.currentSize != size) {
            setFontSize(size);
        }
    }

    float total = 0.0f;
    for (char32_t c : text) {
        total += getGlyphAdvance(c, fontSize);
    }
    return total;
}

float FontManager::getSpaceWidth(float fontSize) {
    return getGlyphAdvance(U' ', fontSize);
}

float FontManager::getKerning(uint32_t leftCodepoint, uint32_t rightCodepoint, uint32_t fontSize) {
    FT_Face face = activeFace();
    if (!face || !FT_HAS_KERNING(face)) return 0.0f;

    FT_UInt leftIndex = FT_Get_Char_Index(face, leftCodepoint);
    FT_UInt rightIndex = FT_Get_Char_Index(face, rightCodepoint);
    if (leftIndex == 0 || rightIndex == 0) return 0.0f;

    FT_Vector kerning;
    FT_Error error = FT_Get_Kerning(face, leftIndex, rightIndex, FT_KERNING_DEFAULT, &kerning);
    if (error) return 0.0f;

    return static_cast<float>(kerning.x >> 6);
}

// ============================================================
// SDF rendering API
// ============================================================

static uint64_t makeSDFCacheKey(uint32_t codepoint, uint32_t fontSize, uint32_t spread) {
    return (static_cast<uint64_t>(codepoint) << 32) | (static_cast<uint64_t>(fontSize) << 16) | spread;
}

bool FontManager::rasterizeCharSDF(uint32_t codepoint, uint32_t fontSize, uint32_t spread, Glyph& outGlyph) {
    FT_Face face = activeFace();
    if (!face) {
        LOGE("FontManager: No font face loaded for SDF rasterization");
        return false;
    }

    spread = std::max(2u, std::min(spread, 32u));

    {
        auto it = m_fonts.find(m_activeFontId);
        if (it == m_fonts.end() || it->second.currentSize != fontSize) {
            setFontSize(fontSize);
        }
    }

    FT_Error error = FT_Property_Set(m_ftLibrary, "sdf", "spread", &spread);
    if (error) {
        LOGW("FontManager: Failed to set SDF spread to %u (error: %d)", spread, error);
    }

    FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
    if (glyphIndex == 0) return false;

    error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
    if (error) return false;

    FT_GlyphSlot slot = face->glyph;
    error = FT_Render_Glyph(slot, FT_RENDER_MODE_SDF);
    if (error) return false;

    outGlyph.codepoint = codepoint;
    outGlyph.width = slot->bitmap.width;
    outGlyph.height = slot->bitmap.rows;
    outGlyph.bearingX = slot->bitmap_left;
    outGlyph.bearingY = slot->bitmap_top;
    outGlyph.bitmapLeft = slot->bitmap_left;
    outGlyph.bitmapTop = slot->bitmap_top;
    outGlyph.advance = static_cast<uint32_t>(slot->advance.x >> 6);
    outGlyph.atlasX = 0;
    outGlyph.atlasY = 0;

    uint32_t bitmapSize = 0;
    outGlyph.bitmapData = convertBitmapToRGBA(slot->bitmap, bitmapSize);
    outGlyph.bitmapSize = bitmapSize;

    return true;
}

const Glyph* FontManager::getSDFGlyph(uint32_t codepoint, uint32_t fontSize, uint32_t spread) {
    if (!m_initialized) return nullptr;

    uint64_t cacheKey = makeSDFCacheKey(codepoint, fontSize, spread);
    auto& cache = activeSDFCache();
    auto it = cache.find(cacheKey);
    if (it != cache.end()) {
        return &(it->second);
    }

    Glyph glyph;
    if (rasterizeCharSDF(codepoint, fontSize, spread, glyph)) {
        cache[cacheKey] = std::move(glyph);
        return &(cache[cacheKey]);
    }

    return nullptr;
}

void FontManager::updateSDFGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t spread, uint32_t atlasX, uint32_t atlasY) {
    uint64_t cacheKey = makeSDFCacheKey(codepoint, fontSize, spread);
    auto& cache = activeSDFCache();
    auto it = cache.find(cacheKey);
    if (it != cache.end()) {
        it->second.atlasX = atlasX;
        it->second.atlasY = atlasY;
    }
}

bool FontManager::getSDFGlyphAtlasPosition(uint32_t codepoint, uint32_t fontSize, uint32_t spread, uint32_t& outAtlasX, uint32_t& outAtlasY) const {
    uint64_t cacheKey = makeSDFCacheKey(codepoint, fontSize, spread);
    auto it = m_fonts.find(m_activeFontId);
    if (it == m_fonts.end()) return false;
    auto sit = it->second.sdfGlyphCache.find(cacheKey);
    if (sit != it->second.sdfGlyphCache.end()) {
        outAtlasX = sit->second.atlasX;
        outAtlasY = sit->second.atlasY;
        return true;
    }
    return false;
}

void FontManager::clearSDFCache() {
    auto it = m_fonts.find(m_activeFontId);
    if (it == m_fonts.end()) return;
    for (auto& pair : it->second.sdfGlyphCache) {
        if (pair.second.bitmapData) {
            delete[] pair.second.bitmapData;
            pair.second.bitmapData = nullptr;
        }
    }
    it->second.sdfGlyphCache.clear();
}

} // namespace AgenUIEngine
