#ifndef AGENUI_TEXT_LINE_BREAK_H
#define AGENUI_TEXT_LINE_BREAK_H

#include <vector>
#include <cstdint>
#include <functional>

#include "TextAnalysis.h"

namespace AgenUIEngine {

// ============================================================
// Types matching line-break.ts
// ============================================================

struct LineBreakCursor {
    int32_t segmentIndex = 0;
    int32_t graphemeIndex = 0;
};

struct PreparedLineChunk {
    int32_t startSegmentIndex = 0;
    int32_t endSegmentIndex = 0;
    int32_t consumedEndSegmentIndex = 0;
};

struct PreparedLineBreakData {
    std::vector<float> widths;
    std::vector<float> lineEndFitAdvances;
    std::vector<float> lineEndPaintAdvances;
    std::vector<SegmentBreakKind> kinds;
    bool simpleLineWalkFastPath = true;
    std::vector<std::vector<float>> breakableWidths;       // inner empty = null
    std::vector<std::vector<float>> breakablePrefixWidths;  // inner empty = null
    float discretionaryHyphenWidth = 0.0f;
    float tabStopAdvance = 0.0f;
    std::vector<PreparedLineChunk> chunks;
};

struct InternalLayoutLine {
    int32_t startSegmentIndex = 0;
    int32_t startGraphemeIndex = 0;
    int32_t endSegmentIndex = 0;
    int32_t endGraphemeIndex = 0;
    float width = 0.0f;
};

// ============================================================
// TextLineBreaker — port of line-break.ts
// ============================================================

class TextLineBreaker {
public:
    /** Count lines for a prepared text (hot path). */
    static int32_t countPreparedLines(
        const PreparedLineBreakData& prepared,
        float maxWidth
    );

    /** Walk all lines with a callback. Returns line count. */
    static int32_t walkPreparedLines(
        const PreparedLineBreakData& prepared,
        float maxWidth,
        std::function<void(const InternalLayoutLine&)> onLine = nullptr
    );

    /** Normalize a cursor to a valid line start position. Returns false if at end. */
    static bool normalizeLineStart(
        const PreparedLineBreakData& prepared,
        LineBreakCursor& start
    );

    /** Layout next line from a cursor (streaming iteration). Returns false if at end. */
    static bool layoutNextLineRange(
        const PreparedLineBreakData& prepared,
        const LineBreakCursor& start,
        float maxWidth,
        InternalLayoutLine& outLine
    );

private:
    static int32_t countPreparedLinesSimple(
        const PreparedLineBreakData& prepared,
        float maxWidth
    );

    static int32_t walkPreparedLinesSimple(
        const PreparedLineBreakData& prepared,
        float maxWidth,
        std::function<void(const InternalLayoutLine&)> onLine
    );

    static bool layoutNextLineRangeSimple(
        const PreparedLineBreakData& prepared,
        const LineBreakCursor& normalizedStart,
        float maxWidth,
        InternalLayoutLine& outLine
    );
};

} // namespace AgenUIEngine

#endif // AGENUI_TEXT_LINE_BREAK_H
