#ifndef AGENUI_TEXT_MEASUREMENT_H
#define AGENUI_TEXT_MEASUREMENT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace AgenUIEngine {

class FontManager;

// ============================================================
// Types matching measurement.ts
// ============================================================

struct SegmentMetrics {
    float width = 0.0f;
    bool containsCJK = false;
    std::vector<float> graphemeWidths;        // per-grapheme widths (null if <=1 grapheme)
    std::vector<float> graphemePrefixWidths;  // cumulative prefix widths
};

struct EngineProfile {
    float lineFitEpsilon = 0.005f;
    bool carryCJKAfterClosingQuote = false;
    bool preferPrefixWidthsForBreakableRuns = false;
    bool preferEarlySoftHyphenBreak = false;
};

// ============================================================
// TextMeasurer — port of measurement.ts
// ============================================================

class TextMeasurer {
public:
    /**
     * Construct with a reference to the FontManager for glyph advance queries.
     */
    explicit TextMeasurer(FontManager& fontManager);

    /** Get segment metrics (cached by segment text + font size). */
    SegmentMetrics getSegmentMetrics(const std::u32string& seg, float fontSize);

    /** Get corrected width (emoji correction = 0 for FreeType). */
    float getCorrectedSegmentWidth(const std::u32string& seg, const SegmentMetrics& metrics) const;

    /** Compute per-grapheme widths for a segment. Returns non-null only if >1 grapheme. */
    std::vector<float> getSegmentGraphemeWidths(
        const std::u32string& seg,
        SegmentMetrics& metrics,
        float fontSize
    );

    /** Compute cumulative prefix widths for a segment. Returns non-null only if >1 grapheme. */
    std::vector<float> getSegmentGraphemePrefixWidths(
        const std::u32string& seg,
        SegmentMetrics& metrics,
        float fontSize
    );

    /** Get the engine profile (hardcoded for non-browser engine). */
    static EngineProfile getEngineProfile();

    /** Get the discretionary hyphen width. */
    float getHyphenWidth(float fontSize);

    /** Get space width. */
    float getSpaceWidth(float fontSize);

    /** Get tab stop advance (8 * space width). */
    float getTabStopAdvance(float fontSize);

    /** Clear all measurement caches. */
    void clearCache();

private:
    FontManager& m_fontManager;

    // Two-level cache: fontSize -> (hash of segment text -> metrics)
    // Using a simple string hash for u32string keys
    struct SegmentKey {
        std::u32string text;
        bool operator==(const SegmentKey& o) const { return text == o.text; }
    };

    struct SegmentKeyHash {
        size_t operator()(const SegmentKey& k) const {
            size_t h = 0;
            for (char32_t c : k.text) {
                h = h * 31 + static_cast<size_t>(c);
            }
            return h;
        }
    };

    std::unordered_map<float, std::unordered_map<SegmentKey, SegmentMetrics, SegmentKeyHash>> m_caches;
};

} // namespace AgenUIEngine

#endif // AGENUI_TEXT_MEASUREMENT_H
