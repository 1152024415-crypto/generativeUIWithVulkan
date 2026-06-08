#include "TextMeasurement.h"
#include "UnicodeUtils.h"
#include "TextAnalysis.h"
#include "modules/text/FontManager.h"

#include <cmath>
#include <algorithm>

namespace AgenUIEngine {

// ============================================================
// Engine profile (hardcoded for non-browser engine)
// ============================================================

EngineProfile TextMeasurer::getEngineProfile() {
    static const EngineProfile profile = {
        0.005f,   // lineFitEpsilon
        false,    // carryCJKAfterClosingQuote
        false,    // preferPrefixWidthsForBreakableRuns
        false     // preferEarlySoftHyphenBreak
    };
    return profile;
}

// ============================================================
// TextMeasurer
// ============================================================

TextMeasurer::TextMeasurer(FontManager& fontManager)
    : m_fontManager(fontManager)
{
}

SegmentMetrics TextMeasurer::getSegmentMetrics(const std::u32string& seg, float fontSize) {
    // Look up in cache
    auto& fontCache = m_caches[fontSize];
    SegmentKey key{seg};
    auto it = fontCache.find(key);
    if (it != fontCache.end()) {
        return it->second;
    }

    // Measure using FontManager
    SegmentMetrics metrics;
    metrics.width = m_fontManager.measureTextAdvance(seg, fontSize);
    metrics.containsCJK = TextAnalyzer::isCJK(seg);

    // Cache and return
    fontCache[key] = metrics;
    return metrics;
}

float TextMeasurer::getCorrectedSegmentWidth(
    const std::u32string& seg,
    const SegmentMetrics& metrics
) const {
    // FreeType has no canvas/DOM emoji inflation issue, so no correction needed
    return metrics.width;
}

std::vector<float> TextMeasurer::getSegmentGraphemeWidths(
    const std::u32string& seg,
    SegmentMetrics& metrics,
    float fontSize
) {
    if (!metrics.graphemeWidths.empty()) return metrics.graphemeWidths;

    GraphemeSegmenter segmenter;
    auto graphemes = segmenter.segment(seg);

    if (graphemes.size() <= 1) return {};

    std::vector<float> widths;
    widths.reserve(graphemes.size());

    for (const auto& gs : graphemes) {
        auto gm = getSegmentMetrics(gs.text, fontSize);
        widths.push_back(getCorrectedSegmentWidth(gs.text, gm));
    }

    metrics.graphemeWidths = std::move(widths);
    return metrics.graphemeWidths;
}

std::vector<float> TextMeasurer::getSegmentGraphemePrefixWidths(
    const std::u32string& seg,
    SegmentMetrics& metrics,
    float fontSize
) {
    if (!metrics.graphemePrefixWidths.empty()) return metrics.graphemePrefixWidths;

    GraphemeSegmenter segmenter;
    auto graphemes = segmenter.segment(seg);

    if (graphemes.size() <= 1) return {};

    std::vector<float> prefixWidths;
    prefixWidths.reserve(graphemes.size());

    std::u32string prefix;
    for (const auto& gs : graphemes) {
        prefix += gs.text;
        auto pm = getSegmentMetrics(prefix, fontSize);
        prefixWidths.push_back(getCorrectedSegmentWidth(prefix, pm));
    }

    metrics.graphemePrefixWidths = std::move(prefixWidths);
    return metrics.graphemePrefixWidths;
}

float TextMeasurer::getHyphenWidth(float fontSize) {
    std::u32string hyphen(1, U'-');
    auto m = getSegmentMetrics(hyphen, fontSize);
    return getCorrectedSegmentWidth(hyphen, m);
}

float TextMeasurer::getSpaceWidth(float fontSize) {
    return m_fontManager.getSpaceWidth(fontSize);
}

float TextMeasurer::getTabStopAdvance(float fontSize) {
    return getSpaceWidth(fontSize) * 8.0f;
}

void TextMeasurer::clearCache() {
    m_caches.clear();
}

} // namespace AgenUIEngine
