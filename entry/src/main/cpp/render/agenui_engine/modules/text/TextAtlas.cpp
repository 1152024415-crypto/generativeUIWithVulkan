#include "TextAtlas.h"
#include "FontManager.h"
#include "logger_common.h"
#include <cstring>
#include <algorithm>

namespace AgenUIEngine {

TextAtlas::TextAtlas()
    : m_textureData(nullptr)
    , m_width(0)
    , m_height(0)
    , m_currentX(0)
    , m_currentY(0)
    , m_currentRowHeight(0)
    , m_padding(DEFAULT_PADDING)
    , m_isFull(false)
    , m_textureId(0)
{
}

TextAtlas::~TextAtlas() {
    clear();
}

bool TextAtlas::initialize(uint32_t width, uint32_t height) {
    // Allocate texture data (R8 format, 1 byte per pixel for grayscale text)
    size_t dataSize = width * height;
    m_textureData = new uint8_t[dataSize];

    // Initialize to 0 (transparent/background)
    memset(m_textureData, 0, dataSize);

    m_width = width;
    m_height = height;

    m_currentX = m_padding;
    m_currentY = m_padding;
    m_currentRowHeight = 0;
    m_isFull = false;

    LOGI("TextAtlas: Initialized %ux%u atlas (R8 format)", width, height);
    return true;
}

bool TextAtlas::addGlyph(const Glyph& glyph) {
    if (m_isFull) {
        LOGE("TextAtlas: Cannot add glyph - atlas is full");
        return false;
    }

    uint32_t glyphX, glyphY;
    if (!tryPackInCurrentRow(glyph, glyphX, glyphY)) {
        // Try moving to next row
        moveToNextRow();
        if (m_isFull) {
            LOGE("TextAtlas: Atlas full during glyph addition");
            return false;
        }

        if (!tryPackInCurrentRow(glyph, glyphX, glyphY)) {
            LOGE("TextAtlas: Cannot fit glyph even in new row");
            return false;
        }
    }

    // Update glyph atlas position
    const_cast<Glyph&>(glyph).atlasX = glyphX;
    const_cast<Glyph&>(glyph).atlasY = glyphY;

    // Copy glyph bitmap to atlas (R8 format - single channel)
    for (uint32_t y = 0; y < glyph.height; ++y) {
        uint32_t atlasY_Calc = glyphY + y;
        if (atlasY_Calc >= m_height) break;

        for (uint32_t x = 0; x < glyph.width; ++x) {
            uint32_t atlasX_Calc = glyphX + x;
            if (atlasX_Calc >= m_width) break;

            // Copy single-channel grayscale data
            uint32_t glyphOffset = y * glyph.width + x;
            uint32_t atlasOffset = atlasY_Calc * m_width + atlasX_Calc;

            if (glyph.bitmapData) {
                m_textureData[atlasOffset] = glyph.bitmapData[glyphOffset];
            }
        }
    }

    // Track dirty region for efficient GPU upload
    m_dirtyRegion.merge(glyphX, glyphY, glyph.width, glyph.height);

    // CRITICAL FIX: Advance currentX position to prevent glyphs from overlapping!
    // This was missing, causing all glyphs to be written to the same position
    m_currentX += glyph.width + m_padding;

    return true;
}

bool TextAtlas::tryPackInCurrentRow(const Glyph& glyph, uint32_t& outX, uint32_t& outY) {
    // Check if glyph fits in current row
    if (m_currentX + glyph.width > m_width) {
        return false;
    }

    // Update row height if needed
    if (glyph.height > m_currentRowHeight) {
        m_currentRowHeight = glyph.height;
    }

    outX = m_currentX;
    outY = m_currentY;
    return true;
}

void TextAtlas::moveToNextRow() {
    // Move to next row
    m_currentX = m_padding;
    m_currentY += m_currentRowHeight + m_padding;

    // Reset row height
    m_currentRowHeight = 0;

    // Check if we've exceeded atlas height
    if (m_currentY >= m_height) {
        m_isFull = true;
        LOGW("TextAtlas: Atlas is full!");
    }
}

void TextAtlas::clear() {
    if (m_textureData) {
        delete[] m_textureData;
        m_textureData = nullptr;
    }
    m_width = 0;
    m_height = 0;
    m_currentX = 0;
    m_currentY = 0;
    m_currentRowHeight = 0;
    m_isFull = false;
    m_textureId = 0;
    m_dirtyRegion.reset();
}

} // namespace AgenUIEngine
