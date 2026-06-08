#ifndef AGENUI_TEXT_ANALYSIS_H
#define AGENUI_TEXT_ANALYSIS_H

#include <string>
#include <vector>
#include <cstdint>

namespace AgenUIEngine {

// ============================================================
// Types matching analysis.ts
// ============================================================

enum class WhiteSpaceMode : uint8_t {
    Normal,
    PreWrap
};

enum class SegmentBreakKind : uint8_t {
    Text,
    Space,
    PreservedSpace,
    Tab,
    Glue,
    ZeroWidthBreak,
    SoftHyphen,
    HardBreak
};

struct SegmentationPiece {
    std::u32string text;
    bool isWordLike;
    SegmentBreakKind kind;
    int32_t start;
};

struct MergedSegmentation {
    int32_t len = 0;
    std::vector<std::u32string> texts;
    std::vector<bool> isWordLike;
    std::vector<SegmentBreakKind> kinds;
    std::vector<int32_t> starts;
};

struct AnalysisChunk {
    int32_t startSegmentIndex;
    int32_t endSegmentIndex;
    int32_t consumedEndSegmentIndex;
};

struct TextAnalysis {
    std::u32string normalized;
    std::vector<AnalysisChunk> chunks;
    int32_t len = 0;
    std::vector<std::u32string> texts;
    std::vector<bool> isWordLike;
    std::vector<SegmentBreakKind> kinds;
    std::vector<int32_t> starts;
};

struct AnalysisProfile {
    bool carryCJKAfterClosingQuote = false;
};

// ============================================================
// TextAnalyzer — port of analysis.ts
// ============================================================

class TextAnalyzer {
public:
    /** Main entry: analyze text into segmented pieces. */
    static TextAnalysis analyzeText(
        const std::u32string& text,
        const AnalysisProfile& profile,
        WhiteSpaceMode whiteSpace = WhiteSpaceMode::Normal
    );

    // Character property tests (exposed for TextMeasurement/TextLayout)
    static bool isCJK(char32_t c);
    static bool isCJK(const std::u32string& s);
    static bool endsWithClosingQuote(const std::u32string& text);

    // Kinsoku character sets
    static const std::vector<char32_t>& getKinsokuStart();
    static const std::vector<char32_t>& getKinsokuEnd();
    static const std::vector<char32_t>& getLeftStickyPunctuation();
};

} // namespace AgenUIEngine

#endif // AGENUI_TEXT_ANALYSIS_H
