#ifndef TEXTATLAS_H
#define TEXTATLAS_H

#include <cstdint>
#include <vector>

namespace AgenUIEngine {

// Forward declaration
struct Glyph;

// Dirty region structure for tracking modified atlas areas
struct DirtyRegion {
    uint32_t minX = UINT32_MAX;
    uint32_t minY = UINT32_MAX;
    uint32_t maxX = 0;
    uint32_t maxY = 0;
    bool isDirty = false;

    void reset() {
        minX = UINT32_MAX;
        minY = UINT32_MAX;
        maxX = 0;
        maxY = 0;
        isDirty = false;
    }

    void merge(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        if (!isDirty) {
            minX = x;
            minY = y;
            maxX = x + width;
            maxY = y + height;
            isDirty = true;
        } else {
            minX = (minX < x) ? minX : x;
            minY = (minY < y) ? minY : y;
            maxX = (maxX > x + width) ? maxX : x + width;
            maxY = (maxY > y + height) ? maxY : y + height;
        }
    }

    uint32_t width() const { return isDirty ? maxX - minX : 0; }
    uint32_t height() const { return isDirty ? maxY - minY : 0; }
};

class TextAtlas {
public:
    static constexpr uint32_t DEFAULT_PADDING = 2;

    TextAtlas();
    ~TextAtlas();

    // Initialize the atlas with given dimensions
    bool initialize(uint32_t width, uint32_t height);

    // Add a glyph to the atlas
    bool addGlyph(const Glyph& glyph);

    // Get atlas data
    const uint8_t* getData() const { return m_textureData; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    // Check if atlas is full
    bool isFull() const { return m_isFull; }

    // Clear the atlas
    void clear();

    // Dirty region tracking
    bool isDirty() const { return m_dirtyRegion.isDirty; }
    const DirtyRegion& getDirtyRegion() const { return m_dirtyRegion; }
    void clearDirty() { m_dirtyRegion.reset(); }

private:
    // Try to pack glyph in current row
    bool tryPackInCurrentRow(const Glyph& glyph, uint32_t& outX, uint32_t& outY);

    // Move to next row
    void moveToNextRow();

    uint8_t* m_textureData;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_currentX;
    uint32_t m_currentY;
    uint32_t m_currentRowHeight;
    uint32_t m_padding;
    bool m_isFull;
    uint32_t m_textureId;

    // Dirty region tracking for efficient GPU uploads
    DirtyRegion m_dirtyRegion;
};

} // namespace AgenUIEngine

#endif // TEXTATLAS_H
