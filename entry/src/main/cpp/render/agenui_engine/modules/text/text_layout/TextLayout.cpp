#include "TextLayout.h"
#include "UnicodeUtils.h"
#include "modules/text/FontManager.h"

#include <cstring>
#include <functional>

namespace AgenUIEngine {

// ============================================================
// Construction
// ============================================================

TextLayoutManager::TextLayoutManager(FontManager& fontManager)
    : m_fontManager(fontManager)
    , m_measurer(fontManager)
    , m_graphemeSegmenter(std::make_unique<GraphemeSegmenter>())
{
}

TextLayoutManager::~TextLayoutManager() = default;

// ============================================================
// Prepare
// ============================================================

std::unique_ptr<PreparedText> TextLayoutManager::prepare(
    const std::u32string& text,
    float fontSize,
    WhiteSpaceMode whiteSpace
) {
    AnalysisProfile profile;
    auto analysis = TextAnalyzer::analyzeText(text, profile, whiteSpace);

    auto prepared = std::make_unique<PreparedText>();
    measureAnalysis(analysis, fontSize, false, *prepared);
    return prepared;
}

std::unique_ptr<PreparedTextWithSegments> TextLayoutManager::prepareWithSegments(
    const std::u32string& text,
    float fontSize,
    WhiteSpaceMode whiteSpace
) {
    AnalysisProfile profile;
    auto analysis = TextAnalyzer::analyzeText(text, profile, whiteSpace);

    auto prepared = std::make_unique<PreparedTextWithSegments>();
    measureAnalysis(analysis, fontSize, true, *prepared);
    return prepared;
}

// ============================================================
// Layout
// ============================================================

LayoutResult TextLayoutManager::layout(
    const PreparedText& prepared,
    float maxWidth,
    float lineHeight
) {
    int32_t lineCount = TextLineBreaker::countPreparedLines(prepared, maxWidth);
    return { lineCount, static_cast<float>(lineCount) * lineHeight };
}

LayoutLinesResult TextLayoutManager::layoutWithLines(
    const PreparedTextWithSegments& prepared,
    float maxWidth,
    float lineHeight
) {
    LayoutLinesResult result;

    if (prepared.widths.empty()) {
        return result;
    }

    result.lines.reserve(16);  // typical line count estimate

    int32_t lineCount = TextLineBreaker::walkPreparedLines(
        prepared, maxWidth,
        [this, &prepared, &result](const InternalLayoutLine& line) {
            LayoutLine layoutLine;
            layoutLine.width = line.width;
            layoutLine.start.segmentIndex = line.startSegmentIndex;
            layoutLine.start.graphemeIndex = line.startGraphemeIndex;
            layoutLine.end.segmentIndex = line.endSegmentIndex;
            layoutLine.end.graphemeIndex = line.endGraphemeIndex;
            layoutLine.text = buildLineTextFromRange(
                prepared,
                line.startSegmentIndex,
                line.startGraphemeIndex,
                line.endSegmentIndex,
                line.endGraphemeIndex
            );
            result.lines.push_back(std::move(layoutLine));
        }
    );

    result.lineCount = lineCount;
    result.height = static_cast<float>(lineCount) * lineHeight;
    return result;
}

int32_t TextLayoutManager::walkLineRanges(
    const PreparedTextWithSegments& prepared,
    float maxWidth,
    std::function<void(const LayoutLineRange&)> onLine
) {
    if (prepared.widths.empty()) return 0;

    return TextLineBreaker::walkPreparedLines(
        prepared, maxWidth,
        [&onLine](const InternalLayoutLine& line) {
            LayoutLineRange range;
            range.width = line.width;
            range.start.segmentIndex = line.startSegmentIndex;
            range.start.graphemeIndex = line.startGraphemeIndex;
            range.end.segmentIndex = line.endSegmentIndex;
            range.end.graphemeIndex = line.endGraphemeIndex;
            onLine(range);
        }
    );
}

bool TextLayoutManager::layoutNextLine(
    const PreparedTextWithSegments& prepared,
    const LayoutCursor& start,
    float maxWidth,
    LayoutLine& outLine
) {
    LineBreakCursor cursor;
    cursor.segmentIndex = start.segmentIndex;
    cursor.graphemeIndex = start.graphemeIndex;

    InternalLayoutLine line;
    if (!TextLineBreaker::layoutNextLineRange(prepared, cursor, maxWidth, line)) {
        return false;
    }

    outLine.width = line.width;
    outLine.start.segmentIndex = line.startSegmentIndex;
    outLine.start.graphemeIndex = line.startGraphemeIndex;
    outLine.end.segmentIndex = line.endSegmentIndex;
    outLine.end.graphemeIndex = line.endGraphemeIndex;
    outLine.text = buildLineTextFromRange(
        prepared,
        line.startSegmentIndex,
        line.startGraphemeIndex,
        line.endSegmentIndex,
        line.endGraphemeIndex
    );
    return true;
}

void TextLayoutManager::clearCache() {
    m_measurer.clearCache();
    m_lineTextCaches.clear();
}

// ============================================================
// measureAnalysis — bridge from analysis to prepared data
// ============================================================

void TextLayoutManager::measureAnalysis(
    const TextAnalysis& analysis,
    float fontSize,
    bool includeSegments,
    PreparedText& outPrepared
) {
    const float hyphenWidth = m_measurer.getHyphenWidth(fontSize);
    const float spaceWidth = m_measurer.getSpaceWidth(fontSize);
    const float tabStopAdvance = spaceWidth * 8.0f;
    const EngineProfile engineProfile = TextMeasurer::getEngineProfile();

    outPrepared.discretionaryHyphenWidth = hyphenWidth;
    outPrepared.tabStopAdvance = tabStopAdvance;

    if (analysis.len == 0) {
        outPrepared.simpleLineWalkFastPath = true;
        return;
    }

    std::vector<float>& widths = outPrepared.widths;
    std::vector<float>& lineEndFitAdvances = outPrepared.lineEndFitAdvances;
    std::vector<float>& lineEndPaintAdvances = outPrepared.lineEndPaintAdvances;
    std::vector<SegmentBreakKind>& kinds = outPrepared.kinds;
    std::vector<std::vector<float>>& breakableWidths = outPrepared.breakableWidths;
    std::vector<std::vector<float>>& breakablePrefixWidths = outPrepared.breakablePrefixWidths;

    bool simpleLineWalkFastPath = (analysis.chunks.size() <= 1);
    std::vector<int32_t> preparedStartByAnalysisIndex(analysis.len);
    std::vector<int32_t> preparedEndByAnalysisIndex(analysis.len);

    PreparedTextWithSegments* preparedWithSegs = includeSegments
        ? static_cast<PreparedTextWithSegments*>(&outPrepared) : nullptr;

    auto pushMeasuredSegment = [&](
        const std::u32string& text,
        float width,
        float lineEndFitAdvance,
        float lineEndPaintAdvance,
        SegmentBreakKind kind,
        int32_t /*start*/,
        const std::vector<float>& breakable,
        const std::vector<float>& breakablePrefix
    ) {
        if (kind != SegmentBreakKind::Text &&
            kind != SegmentBreakKind::Space &&
            kind != SegmentBreakKind::ZeroWidthBreak) {
            simpleLineWalkFastPath = false;
        }
        widths.push_back(width);
        lineEndFitAdvances.push_back(lineEndFitAdvance);
        lineEndPaintAdvances.push_back(lineEndPaintAdvance);
        kinds.push_back(kind);
        breakableWidths.push_back(breakable);
        breakablePrefixWidths.push_back(breakablePrefix);
        if (preparedWithSegs) {
            preparedWithSegs->segments.push_back(text);
        }
    };

    for (int32_t mi = 0; mi < analysis.len; mi++) {
        preparedStartByAnalysisIndex[mi] = static_cast<int32_t>(widths.size());

        const std::u32string& segText = analysis.texts[mi];
        bool segWordLike = analysis.isWordLike[mi];
        SegmentBreakKind segKind = analysis.kinds[mi];

        if (segKind == SegmentBreakKind::SoftHyphen) {
            pushMeasuredSegment(segText, 0, hyphenWidth, hyphenWidth, segKind, 0, {}, {});
            preparedEndByAnalysisIndex[mi] = static_cast<int32_t>(widths.size());
            continue;
        }

        if (segKind == SegmentBreakKind::HardBreak) {
            pushMeasuredSegment(segText, 0, 0, 0, segKind, 0, {}, {});
            preparedEndByAnalysisIndex[mi] = static_cast<int32_t>(widths.size());
            continue;
        }

        if (segKind == SegmentBreakKind::Tab) {
            pushMeasuredSegment(segText, 0, 0, 0, segKind, 0, {}, {});
            preparedEndByAnalysisIndex[mi] = static_cast<int32_t>(widths.size());
            continue;
        }

        SegmentMetrics segMetrics = m_measurer.getSegmentMetrics(segText, fontSize);

        // CJK text: split into per-grapheme units with kinsoku rules
        if (segKind == SegmentBreakKind::Text && segMetrics.containsCJK) {
            auto graphemes = m_graphemeSegmenter->segment(segText);
            std::u32string unitText;
            int32_t unitStart = 0;

            const auto& kinsokuEndSet = TextAnalyzer::getKinsokuEnd();
            const auto& kinsokuStartSet = TextAnalyzer::getKinsokuStart();
            const auto& leftSticky = TextAnalyzer::getLeftStickyPunctuation();

            auto charInVector = [](char32_t c, const std::vector<char32_t>& v) -> bool {
                for (char32_t ch : v) { if (ch == c) return true; }
                return false;
            };

            for (size_t gi = 0; gi < graphemes.size(); gi++) {
                const auto& gs = graphemes[gi];
                const std::u32string& grapheme = gs.text;

                if (unitText.empty()) {
                    unitText = grapheme;
                    unitStart = gs.start;
                    continue;
                }

                bool shouldMerge = false;
                // Kinsoku end: last char of unitText
                char32_t lastUnitChar = unitText.back();
                // Kinsoku start: first char of grapheme
                char32_t firstGraphemeChar = grapheme.front();

                if (charInVector(lastUnitChar, kinsokuEndSet)) {
                    shouldMerge = true;
                } else if (charInVector(firstGraphemeChar, kinsokuStartSet)) {
                    shouldMerge = true;
                } else if (charInVector(firstGraphemeChar, leftSticky)) {
                    shouldMerge = true;
                } else if (engineProfile.carryCJKAfterClosingQuote &&
                           TextAnalyzer::isCJK(grapheme) &&
                           TextAnalyzer::endsWithClosingQuote(unitText)) {
                    shouldMerge = true;
                }

                if (shouldMerge) {
                    unitText += grapheme;
                    continue;
                }

                auto unitMetrics = m_measurer.getSegmentMetrics(unitText, fontSize);
                float w = m_measurer.getCorrectedSegmentWidth(unitText, unitMetrics);
                pushMeasuredSegment(unitText, w, w, w, SegmentBreakKind::Text,
                    analysis.starts[mi] + unitStart, {}, {});

                unitText = grapheme;
                unitStart = gs.start;
            }

            if (!unitText.empty()) {
                auto unitMetrics = m_measurer.getSegmentMetrics(unitText, fontSize);
                float w = m_measurer.getCorrectedSegmentWidth(unitText, unitMetrics);
                pushMeasuredSegment(unitText, w, w, w, SegmentBreakKind::Text,
                    analysis.starts[mi] + unitStart, {}, {});
            }
            preparedEndByAnalysisIndex[mi] = static_cast<int32_t>(widths.size());
            continue;
        }

        // Regular segment
        float w = m_measurer.getCorrectedSegmentWidth(segText, segMetrics);
        float lineEndFitAdvance = 0;
        float lineEndPaintAdvance = 0;

        if (segKind == SegmentBreakKind::Space ||
            segKind == SegmentBreakKind::PreservedSpace ||
            segKind == SegmentBreakKind::ZeroWidthBreak) {
            lineEndFitAdvance = 0;
        } else {
            lineEndFitAdvance = w;
        }

        if (segKind == SegmentBreakKind::Space ||
            segKind == SegmentBreakKind::ZeroWidthBreak) {
            lineEndPaintAdvance = 0;
        } else {
            lineEndPaintAdvance = w;
        }

        // Word-like segments with >1 grapheme get breakable widths
        if (segWordLike && segText.size() > 1) {
            auto gWidths = m_measurer.getSegmentGraphemeWidths(segText, segMetrics, fontSize);
            std::vector<float> gPrefixWidths;
            if (engineProfile.preferPrefixWidthsForBreakableRuns) {
                gPrefixWidths = m_measurer.getSegmentGraphemePrefixWidths(segText, segMetrics, fontSize);
            }
            pushMeasuredSegment(segText, w, lineEndFitAdvance, lineEndPaintAdvance,
                segKind, analysis.starts[mi], gWidths, gPrefixWidths);
        } else {
            pushMeasuredSegment(segText, w, lineEndFitAdvance, lineEndPaintAdvance,
                segKind, analysis.starts[mi], {}, {});
        }
        preparedEndByAnalysisIndex[mi] = static_cast<int32_t>(widths.size());
    }

    // Map analysis chunks to prepared chunks
    outPrepared.chunks.clear();
    for (size_t ci = 0; ci < analysis.chunks.size(); ci++) {
        const auto& chunk = analysis.chunks[ci];
        PreparedLineChunk pc;
        pc.startSegmentIndex =
            chunk.startSegmentIndex < static_cast<int32_t>(preparedStartByAnalysisIndex.size())
            ? preparedStartByAnalysisIndex[chunk.startSegmentIndex]
            : (preparedEndByAnalysisIndex.empty() ? 0 : preparedEndByAnalysisIndex.back());
        pc.endSegmentIndex =
            chunk.endSegmentIndex < static_cast<int32_t>(preparedStartByAnalysisIndex.size())
            ? preparedStartByAnalysisIndex[chunk.endSegmentIndex]
            : (preparedEndByAnalysisIndex.empty() ? 0 : preparedEndByAnalysisIndex.back());
        pc.consumedEndSegmentIndex =
            chunk.consumedEndSegmentIndex < static_cast<int32_t>(preparedStartByAnalysisIndex.size())
            ? preparedStartByAnalysisIndex[chunk.consumedEndSegmentIndex]
            : (preparedEndByAnalysisIndex.empty() ? 0 : preparedEndByAnalysisIndex.back());
        outPrepared.chunks.push_back(pc);
    }

    outPrepared.simpleLineWalkFastPath = simpleLineWalkFastPath;
}

// ============================================================
// Line text building
// ============================================================

const std::vector<std::u32string>& TextLayoutManager::getSegmentGraphemes(
    const PreparedTextWithSegments& prepared,
    int32_t segmentIndex
) {
    auto& segCache = m_lineTextCaches[&prepared];
    auto it = segCache.find(segmentIndex);
    if (it != segCache.end()) return it->second;

    std::vector<std::u32string> graphemes;
    auto gs = m_graphemeSegmenter->segment(prepared.segments[segmentIndex]);
    graphemes.reserve(gs.size());
    for (const auto& g : gs) {
        graphemes.push_back(g.text);
    }
    segCache[segmentIndex] = std::move(graphemes);
    return segCache[segmentIndex];
}

std::u32string TextLayoutManager::buildLineTextFromRange(
    const PreparedTextWithSegments& prepared,
    int32_t startSegmentIndex,
    int32_t startGraphemeIndex,
    int32_t endSegmentIndex,
    int32_t endGraphemeIndex
) {
    std::u32string text;

    // Check if line ends with a soft hyphen (needs discretionary hyphen)
    bool endsWithDiscretionaryHyphen = false;
    if (endSegmentIndex > 0 &&
        endSegmentIndex > startSegmentIndex &&
        prepared.kinds.size() > static_cast<size_t>(endSegmentIndex) - 1) {
        if (prepared.kinds[endSegmentIndex - 1] == SegmentBreakKind::SoftHyphen) {
            endsWithDiscretionaryHyphen = true;
        }
    }

    for (int32_t i = startSegmentIndex; i < endSegmentIndex; i++) {
        if (prepared.kinds[i] == SegmentBreakKind::SoftHyphen ||
            prepared.kinds[i] == SegmentBreakKind::HardBreak) {
            continue;
        }
        if (i == startSegmentIndex && startGraphemeIndex > 0) {
            const auto& graphemes = getSegmentGraphemes(prepared, i);
            for (int32_t g = startGraphemeIndex; g < static_cast<int32_t>(graphemes.size()); g++) {
                text += graphemes[g];
            }
        } else {
            text += prepared.segments[i];
        }
    }

    if (endGraphemeIndex > 0) {
        if (endsWithDiscretionaryHyphen) {
            text += U'-';
        }
        const auto& graphemes = getSegmentGraphemes(prepared, endSegmentIndex);
        int32_t startG = (startSegmentIndex == endSegmentIndex) ? startGraphemeIndex : 0;
        for (int32_t g = startG; g < endGraphemeIndex && g < static_cast<int32_t>(graphemes.size()); g++) {
            text += graphemes[g];
        }
    } else if (endsWithDiscretionaryHyphen) {
        text += U'-';
    }

    return text;
}

// ============================================================
// Multiline text layout cache
// ============================================================

bool TextLayoutManager::prepareAndLayoutCached(
    const std::string& textUtf8,
    uint32_t fontSize,
    float maxWidth,
    float lineHeight,
    PreparedTextWithSegments*& outPrepared,
    LayoutLinesResult*& outLayout
) {
    // Compute cache key from (text_utf8, fontSize)
    size_t key = std::hash<std::string>{}(textUtf8);
    key ^= std::hash<uint32_t>{}(fontSize) + 0x9e3779b9 + (key << 6) + (key >> 2);

    auto it = m_multilineCache.find(key);
    if (it != m_multilineCache.end()) {
        MultilineCacheEntry& entry = it->second;

        // Verify text content match (handle hash collision)
        if (entry.text == textUtf8 && entry.fontSize == fontSize) {
            // Level 1 cache hit: reuse PreparedTextWithSegments
            outPrepared = entry.prepared.get();

            if (entry.maxWidth == maxWidth && entry.lineHeight == lineHeight) {
                // Level 2 cache hit: reuse LayoutLinesResult too
                outLayout = &entry.layoutResult;
                return true;
            }

            // Layout params changed: re-run layoutWithLines only (cheap)
            entry.maxWidth = maxWidth;
            entry.lineHeight = lineHeight;
            entry.layoutResult = layoutWithLines(*entry.prepared, maxWidth, lineHeight);
            outLayout = &entry.layoutResult;
            return true;
        }
    }

    // Cache miss: full prepare + layout
    MultilineCacheEntry entry;
    entry.text = textUtf8;

    // UTF-8 → UTF-32 decode
    entry.text32.reserve(textUtf8.size());
    size_t i = 0;
    while (i < textUtf8.size()) {
        char32_t cp = 0;
        unsigned char c = static_cast<unsigned char>(textUtf8[i]);
        if (c < 0x80) {
            cp = c; i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = (c & 0x1F) << 6;
            cp |= (static_cast<unsigned char>(textUtf8[i+1]) & 0x3F); i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = (c & 0x0F) << 12;
            cp |= (static_cast<unsigned char>(textUtf8[i+1]) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(textUtf8[i+2]) & 0x3F); i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = (c & 0x07) << 18;
            cp |= (static_cast<unsigned char>(textUtf8[i+1]) & 0x3F) << 12;
            cp |= (static_cast<unsigned char>(textUtf8[i+2]) & 0x3F) << 6;
            cp |= (static_cast<unsigned char>(textUtf8[i+3]) & 0x3F); i += 4;
        } else {
            i++; continue;
        }
        entry.text32.push_back(cp);
    }

    entry.fontSize = fontSize;
    entry.maxWidth = maxWidth;
    entry.lineHeight = lineHeight;
    entry.prepared = prepareWithSegments(entry.text32, static_cast<float>(fontSize));
    if (!entry.prepared) {
        return false;
    }
    entry.layoutResult = layoutWithLines(*entry.prepared, maxWidth, lineHeight);

    outPrepared = entry.prepared.get();
    outLayout = &m_multilineCache[key].layoutResult;

    m_multilineCache[key] = std::move(entry);
    // After move, outPrepared/outLayout point into the map entry
    outPrepared = m_multilineCache[key].prepared.get();
    outLayout = &m_multilineCache[key].layoutResult;

    return true;
}

void TextLayoutManager::clearMultilineCache() {
    m_multilineCache.clear();
}

} // namespace AgenUIEngine
