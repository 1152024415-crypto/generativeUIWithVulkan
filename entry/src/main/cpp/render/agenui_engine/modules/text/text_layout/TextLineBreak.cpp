#include "TextLineBreak.h"
#include "TextMeasurement.h"

#include <cmath>

namespace AgenUIEngine {

// ============================================================
// Anonymous namespace: free helper functions (port of TS module-level functions)
// ============================================================

namespace {

bool canBreakAfter(SegmentBreakKind kind) {
    return kind == SegmentBreakKind::Space ||
           kind == SegmentBreakKind::PreservedSpace ||
           kind == SegmentBreakKind::Tab ||
           kind == SegmentBreakKind::ZeroWidthBreak ||
           kind == SegmentBreakKind::SoftHyphen;
}

bool isSimpleCollapsibleSpace(SegmentBreakKind kind) {
    return kind == SegmentBreakKind::Space;
}

float getTabAdvance(float lineWidth, float tabStopAdvance) {
    if (tabStopAdvance <= 0) return 0.0f;

    const float remainder = std::fmod(lineWidth, tabStopAdvance);
    if (std::fabs(remainder) <= 1e-6f) return tabStopAdvance;
    return tabStopAdvance - remainder;
}

float getBreakableAdvance(
    const std::vector<float>& graphemeWidths,
    const std::vector<float>& graphemePrefixWidths, // empty = null
    int32_t graphemeIndex,
    bool preferPrefixWidths
) {
    if (!preferPrefixWidths || graphemePrefixWidths.empty()) {
        return graphemeWidths[graphemeIndex];
    }
    return graphemePrefixWidths[graphemeIndex] -
           (graphemeIndex > 0 ? graphemePrefixWidths[graphemeIndex - 1] : 0.0f);
}

struct SoftHyphenFitResult {
    int32_t fitCount = 0;
    float fittedWidth = 0.0f;
};

SoftHyphenFitResult fitSoftHyphenBreak(
    const std::vector<float>& graphemeWidths,
    float initialWidth,
    float maxWidth,
    float lineFitEpsilon,
    float discretionaryHyphenWidth,
    bool cumulativeWidths
) {
    int32_t fitCount = 0;
    float fittedWidth = initialWidth;

    while (fitCount < static_cast<int32_t>(graphemeWidths.size())) {
        const float nextWidth = cumulativeWidths
            ? initialWidth + graphemeWidths[fitCount]
            : fittedWidth + graphemeWidths[fitCount];
        const float nextLineWidth = (fitCount + 1 < static_cast<int32_t>(graphemeWidths.size()))
            ? nextWidth + discretionaryHyphenWidth
            : nextWidth;
        if (nextLineWidth > maxWidth + lineFitEpsilon) break;
        fittedWidth = nextWidth;
        fitCount++;
    }

    return { fitCount, fittedWidth };
}

int32_t findChunkIndexForStart(const PreparedLineBreakData& prepared, int32_t segmentIndex) {
    for (int32_t i = 0; i < static_cast<int32_t>(prepared.chunks.size()); i++) {
        const auto& chunk = prepared.chunks[i];
        if (segmentIndex < chunk.consumedEndSegmentIndex) return i;
    }
    return -1;
}

// Helper to check if a breakable vector is non-null (non-empty in C++ representation)
bool breakableNotNull(const std::vector<float>& v) {
    return !v.empty();
}

} // anonymous namespace

// ============================================================
// TextLineBreaker — static method implementations
// ============================================================

bool TextLineBreaker::normalizeLineStart(
    const PreparedLineBreakData& prepared,
    LineBreakCursor& start
) {
    int32_t segmentIndex = start.segmentIndex;
    const int32_t graphemeIndex = start.graphemeIndex;

    if (segmentIndex >= static_cast<int32_t>(prepared.widths.size())) return false;
    if (graphemeIndex > 0) return true;

    const int32_t chunkIndex = findChunkIndexForStart(prepared, segmentIndex);
    if (chunkIndex < 0) return false;

    const auto& chunk = prepared.chunks[chunkIndex];
    if (chunk.startSegmentIndex == chunk.endSegmentIndex && segmentIndex == chunk.startSegmentIndex) {
        start.segmentIndex = segmentIndex;
        start.graphemeIndex = 0;
        return true;
    }

    if (segmentIndex < chunk.startSegmentIndex) segmentIndex = chunk.startSegmentIndex;
    while (segmentIndex < chunk.endSegmentIndex) {
        const auto kind = prepared.kinds[segmentIndex];
        if (kind != SegmentBreakKind::Space &&
            kind != SegmentBreakKind::ZeroWidthBreak &&
            kind != SegmentBreakKind::SoftHyphen) {
            start.segmentIndex = segmentIndex;
            start.graphemeIndex = 0;
            return true;
        }
        segmentIndex++;
    }

    if (chunk.consumedEndSegmentIndex >= static_cast<int32_t>(prepared.widths.size())) return false;
    start.segmentIndex = chunk.consumedEndSegmentIndex;
    start.graphemeIndex = 0;
    return true;
}

// ============================================================
// countPreparedLines / countPreparedLinesSimple
// ============================================================

int32_t TextLineBreaker::countPreparedLines(
    const PreparedLineBreakData& prepared,
    float maxWidth
) {
    if (prepared.simpleLineWalkFastPath) {
        return countPreparedLinesSimple(prepared, maxWidth);
    }
    return walkPreparedLines(prepared, maxWidth);
}

int32_t TextLineBreaker::countPreparedLinesSimple(
    const PreparedLineBreakData& prepared,
    float maxWidth
) {
    const auto& widths  = prepared.widths;
    const auto& kinds   = prepared.kinds;
    const auto& breakableWidths  = prepared.breakableWidths;
    const auto& breakablePrefixWidths = prepared.breakablePrefixWidths;

    if (widths.empty()) return 0;

    const EngineProfile engineProfile = TextMeasurer::getEngineProfile();
    const float lineFitEpsilon = engineProfile.lineFitEpsilon;

    int32_t lineCount = 0;
    float lineW = 0.0f;
    bool hasContent = false;

    // Local struct to capture closure variables by reference, porting the
    // placeOnFreshLine nested function from TS.
    struct PlaceOnFreshLineFn {
        const std::vector<float>& widths;
        const std::vector<std::vector<float>>& breakableWidths;
        const std::vector<std::vector<float>>& breakablePrefixWidths;
        const EngineProfile& engineProfile;
        const float lineFitEpsilon;
        const float maxWidth;
        int32_t& lineCount;
        float& lineW;
        bool& hasContent;

        void operator()(int32_t segmentIndex) {
            const float w = widths[segmentIndex];
            if (w > maxWidth && breakableNotNull(breakableWidths[segmentIndex])) {
                const auto& gWidths = breakableWidths[segmentIndex];
                const auto& gPrefixWidths = breakablePrefixWidths[segmentIndex];
                lineW = 0.0f;
                for (int32_t g = 0; g < static_cast<int32_t>(gWidths.size()); g++) {
                    const float gw = getBreakableAdvance(
                        gWidths, gPrefixWidths, g,
                        engineProfile.preferPrefixWidthsForBreakableRuns
                    );
                    if (lineW > 0.0f && lineW + gw > maxWidth + lineFitEpsilon) {
                        lineCount++;
                        lineW = gw;
                    } else {
                        if (lineW == 0.0f) lineCount++;
                        lineW += gw;
                    }
                }
            } else {
                lineW = w;
                lineCount++;
            }
            hasContent = true;
        }
    };

    PlaceOnFreshLineFn placeOnFreshLine{
        widths, breakableWidths, breakablePrefixWidths,
        engineProfile, lineFitEpsilon, maxWidth,
        lineCount, lineW, hasContent
    };

    for (int32_t i = 0; i < static_cast<int32_t>(widths.size()); i++) {
        const float w = widths[i];
        const auto kind = kinds[i];

        if (!hasContent) {
            placeOnFreshLine(i);
            continue;
        }

        const float newW = lineW + w;
        if (newW > maxWidth + lineFitEpsilon) {
            if (isSimpleCollapsibleSpace(kind)) continue;
            lineW = 0.0f;
            hasContent = false;
            placeOnFreshLine(i);
            continue;
        }

        lineW = newW;
    }

    if (!hasContent) return lineCount + 1;
    return lineCount;
}

// ============================================================
// walkPreparedLinesSimple
// ============================================================

int32_t TextLineBreaker::walkPreparedLinesSimple(
    const PreparedLineBreakData& prepared,
    float maxWidth,
    std::function<void(const InternalLayoutLine&)> onLine
) {
    const auto& widths  = prepared.widths;
    const auto& kinds   = prepared.kinds;
    const auto& breakableWidths  = prepared.breakableWidths;
    const auto& breakablePrefixWidths = prepared.breakablePrefixWidths;

    if (widths.empty()) return 0;

    const EngineProfile engineProfile = TextMeasurer::getEngineProfile();
    const float lineFitEpsilon = engineProfile.lineFitEpsilon;

    int32_t lineCount = 0;
    float lineW = 0.0f;
    bool hasContent = false;
    int32_t lineStartSegmentIndex = 0;
    int32_t lineStartGraphemeIndex = 0;
    int32_t lineEndSegmentIndex = 0;
    int32_t lineEndGraphemeIndex = 0;
    int32_t pendingBreakSegmentIndex = -1;
    float pendingBreakPaintWidth = 0.0f;

    // Nested function helpers as a struct with references
    struct Helpers {
        const PreparedLineBreakData& prepared;
        const EngineProfile& engineProfile;
        const float lineFitEpsilon;
        const float maxWidth;

        int32_t& lineCount;
        float& lineW;
        bool& hasContent;
        int32_t& lineStartSegmentIndex;
        int32_t& lineStartGraphemeIndex;
        int32_t& lineEndSegmentIndex;
        int32_t& lineEndGraphemeIndex;
        int32_t& pendingBreakSegmentIndex;
        float& pendingBreakPaintWidth;
        std::function<void(const InternalLayoutLine&)>& onLine;

        void clearPendingBreak() {
            pendingBreakSegmentIndex = -1;
            pendingBreakPaintWidth = 0.0f;
        }

        void emitCurrentLine(
            int32_t endSegIdx = -1,
            int32_t endGraphIdx = -1,
            float width = -1.0f
        ) {
            if (endSegIdx < 0) endSegIdx = lineEndSegmentIndex;
            if (endGraphIdx < 0) endGraphIdx = lineEndGraphemeIndex;
            if (width < 0.0f) width = lineW;

            lineCount++;
            if (onLine) {
                InternalLayoutLine line;
                line.startSegmentIndex = lineStartSegmentIndex;
                line.startGraphemeIndex = lineStartGraphemeIndex;
                line.endSegmentIndex = endSegIdx;
                line.endGraphemeIndex = endGraphIdx;
                line.width = width;
                onLine(line);
            }
            lineW = 0.0f;
            hasContent = false;
            clearPendingBreak();
        }

        void startLineAtSegment(int32_t segmentIndex, float width) {
            hasContent = true;
            lineStartSegmentIndex = segmentIndex;
            lineStartGraphemeIndex = 0;
            lineEndSegmentIndex = segmentIndex + 1;
            lineEndGraphemeIndex = 0;
            lineW = width;
        }

        void startLineAtGrapheme(int32_t segmentIndex, int32_t graphemeIndex, float width) {
            hasContent = true;
            lineStartSegmentIndex = segmentIndex;
            lineStartGraphemeIndex = graphemeIndex;
            lineEndSegmentIndex = segmentIndex;
            lineEndGraphemeIndex = graphemeIndex + 1;
            lineW = width;
        }

        void appendWholeSegment(int32_t segmentIndex, float width) {
            if (!hasContent) {
                startLineAtSegment(segmentIndex, width);
                return;
            }
            lineW += width;
            lineEndSegmentIndex = segmentIndex + 1;
            lineEndGraphemeIndex = 0;
        }

        void updatePendingBreak(int32_t segmentIndex, float segmentWidth) {
            if (!canBreakAfter(prepared.kinds[segmentIndex])) return;
            pendingBreakSegmentIndex = segmentIndex + 1;
            pendingBreakPaintWidth = lineW - segmentWidth;
        }

        void appendBreakableSegment(int32_t segmentIndex) {
            appendBreakableSegmentFrom(segmentIndex, 0);
        }

        void appendBreakableSegmentFrom(int32_t segmentIndex, int32_t startGraphemeIndex) {
            const auto& gWidths = prepared.breakableWidths[segmentIndex];
            const auto& gPrefixWidths = prepared.breakablePrefixWidths[segmentIndex];
            for (int32_t g = startGraphemeIndex; g < static_cast<int32_t>(gWidths.size()); g++) {
                const float gw = getBreakableAdvance(
                    gWidths, gPrefixWidths, g,
                    engineProfile.preferPrefixWidthsForBreakableRuns
                );

                if (!hasContent) {
                    startLineAtGrapheme(segmentIndex, g, gw);
                    continue;
                }

                if (lineW + gw > maxWidth + lineFitEpsilon) {
                    emitCurrentLine();
                    startLineAtGrapheme(segmentIndex, g, gw);
                } else {
                    lineW += gw;
                    lineEndSegmentIndex = segmentIndex;
                    lineEndGraphemeIndex = g + 1;
                }
            }

            if (hasContent && lineEndSegmentIndex == segmentIndex &&
                lineEndGraphemeIndex == static_cast<int32_t>(gWidths.size())) {
                lineEndSegmentIndex = segmentIndex + 1;
                lineEndGraphemeIndex = 0;
            }
        }
    };

    Helpers h{
        prepared, engineProfile, lineFitEpsilon, maxWidth,
        lineCount, lineW, hasContent,
        lineStartSegmentIndex, lineStartGraphemeIndex,
        lineEndSegmentIndex, lineEndGraphemeIndex,
        pendingBreakSegmentIndex, pendingBreakPaintWidth,
        onLine
    };

    int32_t i = 0;
    while (i < static_cast<int32_t>(widths.size())) {
        const float w = widths[i];
        const auto kind = kinds[i];

        if (!hasContent) {
            if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                h.appendBreakableSegment(i);
            } else {
                h.startLineAtSegment(i, w);
            }
            h.updatePendingBreak(i, w);
            i++;
            continue;
        }

        const float newW = lineW + w;
        if (newW > maxWidth + lineFitEpsilon) {
            if (canBreakAfter(kind)) {
                h.appendWholeSegment(i, w);
                h.emitCurrentLine(i + 1, 0, lineW - w);
                i++;
                continue;
            }

            if (pendingBreakSegmentIndex >= 0) {
                h.emitCurrentLine(pendingBreakSegmentIndex, 0, pendingBreakPaintWidth);
                continue;
            }

            if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                h.emitCurrentLine();
                h.appendBreakableSegment(i);
                i++;
                continue;
            }

            h.emitCurrentLine();
            continue;
        }

        h.appendWholeSegment(i, w);
        h.updatePendingBreak(i, w);
        i++;
    }

    if (hasContent) h.emitCurrentLine();
    return lineCount;
}

// ============================================================
// walkPreparedLines (full path with chunks/tabs/soft-hyphen)
// ============================================================

int32_t TextLineBreaker::walkPreparedLines(
    const PreparedLineBreakData& prepared,
    float maxWidth,
    std::function<void(const InternalLayoutLine&)> onLine
) {
    if (prepared.simpleLineWalkFastPath) {
        return walkPreparedLinesSimple(prepared, maxWidth, std::move(onLine));
    }

    const auto& widths  = prepared.widths;
    const auto& lineEndFitAdvances   = prepared.lineEndFitAdvances;
    const auto& lineEndPaintAdvances = prepared.lineEndPaintAdvances;
    const auto& kinds   = prepared.kinds;
    const auto& breakableWidths  = prepared.breakableWidths;
    const auto& breakablePrefixWidths = prepared.breakablePrefixWidths;
    const float discretionaryHyphenWidth = prepared.discretionaryHyphenWidth;
    const float tabStopAdvance = prepared.tabStopAdvance;
    const auto& chunks  = prepared.chunks;

    if (widths.empty() || chunks.empty()) return 0;

    const EngineProfile engineProfile = TextMeasurer::getEngineProfile();
    const float lineFitEpsilon = engineProfile.lineFitEpsilon;

    int32_t lineCount = 0;
    float lineW = 0.0f;
    bool hasContent = false;
    int32_t lineStartSegmentIndex = 0;
    int32_t lineStartGraphemeIndex = 0;
    int32_t lineEndSegmentIndex = 0;
    int32_t lineEndGraphemeIndex = 0;
    int32_t pendingBreakSegmentIndex = -1;
    float pendingBreakFitWidth = 0.0f;
    float pendingBreakPaintWidth = 0.0f;
    SegmentBreakKind pendingBreakKind = SegmentBreakKind::Text;
    bool hasPendingBreakKind = false;

    // Helpers struct capturing all mutable state by reference
    struct Helpers {
        const PreparedLineBreakData& prepared;
        const EngineProfile& engineProfile;
        const float lineFitEpsilon;
        const float maxWidth;
        const float discretionaryHyphenWidth;
        const float tabStopAdvance;

        int32_t& lineCount;
        float& lineW;
        bool& hasContent;
        int32_t& lineStartSegmentIndex;
        int32_t& lineStartGraphemeIndex;
        int32_t& lineEndSegmentIndex;
        int32_t& lineEndGraphemeIndex;
        int32_t& pendingBreakSegmentIndex;
        float& pendingBreakFitWidth;
        float& pendingBreakPaintWidth;
        SegmentBreakKind& pendingBreakKind;
        bool& hasPendingBreakKind;
        std::function<void(const InternalLayoutLine&)>& onLine;

        void clearPendingBreak() {
            pendingBreakSegmentIndex = -1;
            pendingBreakFitWidth = 0.0f;
            pendingBreakPaintWidth = 0.0f;
            hasPendingBreakKind = false;
        }

        void emitCurrentLine(
            int32_t endSegIdx = -1,
            int32_t endGraphIdx = -1,
            float width = -1.0f
        ) {
            if (endSegIdx < 0) endSegIdx = lineEndSegmentIndex;
            if (endGraphIdx < 0) endGraphIdx = lineEndGraphemeIndex;
            if (width < 0.0f) width = lineW;

            lineCount++;
            if (onLine) {
                InternalLayoutLine line;
                line.startSegmentIndex = lineStartSegmentIndex;
                line.startGraphemeIndex = lineStartGraphemeIndex;
                line.endSegmentIndex = endSegIdx;
                line.endGraphemeIndex = endGraphIdx;
                line.width = width;
                onLine(line);
            }
            lineW = 0.0f;
            hasContent = false;
            clearPendingBreak();
        }

        void startLineAtSegment(int32_t segmentIndex, float width) {
            hasContent = true;
            lineStartSegmentIndex = segmentIndex;
            lineStartGraphemeIndex = 0;
            lineEndSegmentIndex = segmentIndex + 1;
            lineEndGraphemeIndex = 0;
            lineW = width;
        }

        void startLineAtGrapheme(int32_t segmentIndex, int32_t graphemeIndex, float width) {
            hasContent = true;
            lineStartSegmentIndex = segmentIndex;
            lineStartGraphemeIndex = graphemeIndex;
            lineEndSegmentIndex = segmentIndex;
            lineEndGraphemeIndex = graphemeIndex + 1;
            lineW = width;
        }

        void appendWholeSegment(int32_t segmentIndex, float width) {
            if (!hasContent) {
                startLineAtSegment(segmentIndex, width);
                return;
            }
            lineW += width;
            lineEndSegmentIndex = segmentIndex + 1;
            lineEndGraphemeIndex = 0;
        }

        void updatePendingBreakForWholeSegment(int32_t segmentIndex, float segmentWidth) {
            if (!canBreakAfter(prepared.kinds[segmentIndex])) return;
            const auto kind = prepared.kinds[segmentIndex];
            const float fitAdvance = (kind == SegmentBreakKind::Tab)
                ? 0.0f : prepared.lineEndFitAdvances[segmentIndex];
            const float paintAdvance = (kind == SegmentBreakKind::Tab)
                ? segmentWidth : prepared.lineEndPaintAdvances[segmentIndex];
            pendingBreakSegmentIndex = segmentIndex + 1;
            pendingBreakFitWidth = lineW - segmentWidth + fitAdvance;
            pendingBreakPaintWidth = lineW - segmentWidth + paintAdvance;
            pendingBreakKind = kind;
            hasPendingBreakKind = true;
        }

        void appendBreakableSegment(int32_t segmentIndex) {
            appendBreakableSegmentFrom(segmentIndex, 0);
        }

        void appendBreakableSegmentFrom(int32_t segmentIndex, int32_t startGraphemeIndex) {
            const auto& gWidths = prepared.breakableWidths[segmentIndex];
            const auto& gPrefixWidths = prepared.breakablePrefixWidths[segmentIndex];
            for (int32_t g = startGraphemeIndex; g < static_cast<int32_t>(gWidths.size()); g++) {
                const float gw = getBreakableAdvance(
                    gWidths, gPrefixWidths, g,
                    engineProfile.preferPrefixWidthsForBreakableRuns
                );

                if (!hasContent) {
                    startLineAtGrapheme(segmentIndex, g, gw);
                    continue;
                }

                if (lineW + gw > maxWidth + lineFitEpsilon) {
                    emitCurrentLine();
                    startLineAtGrapheme(segmentIndex, g, gw);
                } else {
                    lineW += gw;
                    lineEndSegmentIndex = segmentIndex;
                    lineEndGraphemeIndex = g + 1;
                }
            }

            if (hasContent && lineEndSegmentIndex == segmentIndex &&
                lineEndGraphemeIndex == static_cast<int32_t>(gWidths.size())) {
                lineEndSegmentIndex = segmentIndex + 1;
                lineEndGraphemeIndex = 0;
            }
        }

        bool continueSoftHyphenBreakableSegment(int32_t segmentIndex) {
            if (!hasPendingBreakKind || pendingBreakKind != SegmentBreakKind::SoftHyphen) return false;
            const auto& gWidths = prepared.breakableWidths[segmentIndex];
            if (gWidths.empty()) return false;

            // Determine fitWidths: use prefix widths if profile prefers them
            const std::vector<float>* fitWidths = &gWidths;
            std::vector<float> fallback; // unused, but needed for pointer stability
            const std::vector<float>* gPrefixWidths = nullptr;
            bool usesPrefixWidths = false;

            if (engineProfile.preferPrefixWidthsForBreakableRuns) {
                gPrefixWidths = &prepared.breakablePrefixWidths[segmentIndex];
                if (!gPrefixWidths->empty()) {
                    fitWidths = gPrefixWidths;
                    usesPrefixWidths = true;
                }
            }

            const auto [fitCount, fittedWidth] = fitSoftHyphenBreak(
                *fitWidths,
                lineW,
                maxWidth,
                lineFitEpsilon,
                discretionaryHyphenWidth,
                usesPrefixWidths
            );
            if (fitCount == 0) return false;

            lineW = fittedWidth;
            lineEndSegmentIndex = segmentIndex;
            lineEndGraphemeIndex = fitCount;
            clearPendingBreak();

            if (fitCount == static_cast<int32_t>(gWidths.size())) {
                lineEndSegmentIndex = segmentIndex + 1;
                lineEndGraphemeIndex = 0;
                return true;
            }

            emitCurrentLine(
                segmentIndex,
                fitCount,
                fittedWidth + discretionaryHyphenWidth
            );
            appendBreakableSegmentFrom(segmentIndex, fitCount);
            return true;
        }

        void emitEmptyChunk(const PreparedLineChunk& chunk) {
            lineCount++;
            if (onLine) {
                InternalLayoutLine line;
                line.startSegmentIndex = chunk.startSegmentIndex;
                line.startGraphemeIndex = 0;
                line.endSegmentIndex = chunk.consumedEndSegmentIndex;
                line.endGraphemeIndex = 0;
                line.width = 0.0f;
                onLine(line);
            }
            clearPendingBreak();
        }
    };

    Helpers h{
        prepared, engineProfile, lineFitEpsilon, maxWidth,
        discretionaryHyphenWidth, tabStopAdvance,
        lineCount, lineW, hasContent,
        lineStartSegmentIndex, lineStartGraphemeIndex,
        lineEndSegmentIndex, lineEndGraphemeIndex,
        pendingBreakSegmentIndex, pendingBreakFitWidth,
        pendingBreakPaintWidth, pendingBreakKind, hasPendingBreakKind,
        onLine
    };

    for (int32_t chunkIndex = 0; chunkIndex < static_cast<int32_t>(chunks.size()); chunkIndex++) {
        const auto& chunk = chunks[chunkIndex];
        if (chunk.startSegmentIndex == chunk.endSegmentIndex) {
            h.emitEmptyChunk(chunk);
            continue;
        }

        hasContent = false;
        lineW = 0.0f;
        lineStartSegmentIndex = chunk.startSegmentIndex;
        lineStartGraphemeIndex = 0;
        lineEndSegmentIndex = chunk.startSegmentIndex;
        lineEndGraphemeIndex = 0;
        h.clearPendingBreak();

        int32_t i = chunk.startSegmentIndex;
        while (i < chunk.endSegmentIndex) {
            const auto kind = kinds[i];
            const float w = (kind == SegmentBreakKind::Tab)
                ? getTabAdvance(lineW, tabStopAdvance)
                : widths[i];

            if (kind == SegmentBreakKind::SoftHyphen) {
                if (hasContent) {
                    lineEndSegmentIndex = i + 1;
                    lineEndGraphemeIndex = 0;
                    pendingBreakSegmentIndex = i + 1;
                    pendingBreakFitWidth = lineW + discretionaryHyphenWidth;
                    pendingBreakPaintWidth = lineW + discretionaryHyphenWidth;
                    pendingBreakKind = kind;
                    hasPendingBreakKind = true;
                }
                i++;
                continue;
            }

            if (!hasContent) {
                if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                    h.appendBreakableSegment(i);
                } else {
                    h.startLineAtSegment(i, w);
                }
                h.updatePendingBreakForWholeSegment(i, w);
                i++;
                continue;
            }

            const float newW = lineW + w;
            if (newW > maxWidth + lineFitEpsilon) {
                const float currentBreakFitWidth = lineW +
                    (kind == SegmentBreakKind::Tab ? 0.0f : lineEndFitAdvances[i]);
                const float currentBreakPaintWidth = lineW +
                    (kind == SegmentBreakKind::Tab ? w : lineEndPaintAdvances[i]);

                if (hasPendingBreakKind &&
                    pendingBreakKind == SegmentBreakKind::SoftHyphen &&
                    engineProfile.preferEarlySoftHyphenBreak &&
                    pendingBreakFitWidth <= maxWidth + lineFitEpsilon) {
                    h.emitCurrentLine(pendingBreakSegmentIndex, 0, pendingBreakPaintWidth);
                    continue;
                }

                if (hasPendingBreakKind &&
                    pendingBreakKind == SegmentBreakKind::SoftHyphen &&
                    h.continueSoftHyphenBreakableSegment(i)) {
                    i++;
                    continue;
                }

                if (canBreakAfter(kind) && currentBreakFitWidth <= maxWidth + lineFitEpsilon) {
                    h.appendWholeSegment(i, w);
                    h.emitCurrentLine(i + 1, 0, currentBreakPaintWidth);
                    i++;
                    continue;
                }

                if (pendingBreakSegmentIndex >= 0 && pendingBreakFitWidth <= maxWidth + lineFitEpsilon) {
                    h.emitCurrentLine(pendingBreakSegmentIndex, 0, pendingBreakPaintWidth);
                    continue;
                }

                if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                    h.emitCurrentLine();
                    h.appendBreakableSegment(i);
                    i++;
                    continue;
                }

                h.emitCurrentLine();
                continue;
            }

            h.appendWholeSegment(i, w);
            h.updatePendingBreakForWholeSegment(i, w);
            i++;
        }

        if (hasContent) {
            const float finalPaintWidth =
                (pendingBreakSegmentIndex == chunk.consumedEndSegmentIndex)
                    ? pendingBreakPaintWidth
                    : lineW;
            h.emitCurrentLine(chunk.consumedEndSegmentIndex, 0, finalPaintWidth);
        }
    }

    return lineCount;
}

// ============================================================
// layoutNextLineRange
// ============================================================

bool TextLineBreaker::layoutNextLineRange(
    const PreparedLineBreakData& prepared,
    const LineBreakCursor& start,
    float maxWidth,
    InternalLayoutLine& outLine
) {
    LineBreakCursor normalizedStart = start;
    if (!normalizeLineStart(prepared, normalizedStart)) return false;

    if (prepared.simpleLineWalkFastPath) {
        return layoutNextLineRangeSimple(prepared, normalizedStart, maxWidth, outLine);
    }

    const int32_t chunkIndex = findChunkIndexForStart(prepared, normalizedStart.segmentIndex);
    if (chunkIndex < 0) return false;

    const auto& chunk = prepared.chunks[chunkIndex];
    if (chunk.startSegmentIndex == chunk.endSegmentIndex) {
        outLine.startSegmentIndex = chunk.startSegmentIndex;
        outLine.startGraphemeIndex = 0;
        outLine.endSegmentIndex = chunk.consumedEndSegmentIndex;
        outLine.endGraphemeIndex = 0;
        outLine.width = 0.0f;
        return true;
    }

    const auto& widths  = prepared.widths;
    const auto& lineEndFitAdvances   = prepared.lineEndFitAdvances;
    const auto& lineEndPaintAdvances = prepared.lineEndPaintAdvances;
    const auto& kinds   = prepared.kinds;
    const auto& breakableWidths  = prepared.breakableWidths;
    const auto& breakablePrefixWidths = prepared.breakablePrefixWidths;
    const float discretionaryHyphenWidth = prepared.discretionaryHyphenWidth;
    const float tabStopAdvance = prepared.tabStopAdvance;

    const EngineProfile engineProfile = TextMeasurer::getEngineProfile();
    const float lineFitEpsilon = engineProfile.lineFitEpsilon;

    float lineW = 0.0f;
    bool hasContent = false;
    const int32_t lineStartSegIdx = normalizedStart.segmentIndex;
    const int32_t lineStartGraphIdx = normalizedStart.graphemeIndex;
    int32_t lineEndSegIdx = lineStartSegIdx;
    int32_t lineEndGraphIdx = lineStartGraphIdx;
    int32_t pendingBreakSegIdx = -1;
    float pendingBreakFitW = 0.0f;
    float pendingBreakPaintW = 0.0f;
    SegmentBreakKind pendingBrkKind = SegmentBreakKind::Text;
    bool hasPendingBrkKind = false;

    // finishLine: returns true if hasContent and fills outLine
    auto finishLine = [&](int32_t endSegIdx, int32_t endGraphIdx, float width) -> bool {
        if (!hasContent) return false;
        outLine.startSegmentIndex = lineStartSegIdx;
        outLine.startGraphemeIndex = lineStartGraphIdx;
        outLine.endSegmentIndex = endSegIdx;
        outLine.endGraphemeIndex = endGraphIdx;
        outLine.width = width;
        return true;
    };

    auto clearPendingBreak = [&]() {
        pendingBreakSegIdx = -1;
        pendingBreakFitW = 0.0f;
        pendingBreakPaintW = 0.0f;
        hasPendingBrkKind = false;
    };

    auto startLineAtSegment = [&](int32_t segmentIndex, float width) {
        hasContent = true;
        lineEndSegIdx = segmentIndex + 1;
        lineEndGraphIdx = 0;
        lineW = width;
    };

    auto startLineAtGrapheme = [&](int32_t segmentIndex, int32_t graphemeIndex, float width) {
        hasContent = true;
        lineEndSegIdx = segmentIndex;
        lineEndGraphIdx = graphemeIndex + 1;
        lineW = width;
    };

    auto appendWholeSegment = [&](int32_t segmentIndex, float width) {
        if (!hasContent) {
            startLineAtSegment(segmentIndex, width);
            return;
        }
        lineW += width;
        lineEndSegIdx = segmentIndex + 1;
        lineEndGraphIdx = 0;
    };

    auto updatePendingBreakForWholeSegment = [&](int32_t segmentIndex, float segmentWidth) {
        if (!canBreakAfter(kinds[segmentIndex])) return;
        const auto kind = kinds[segmentIndex];
        const float fitAdvance = (kind == SegmentBreakKind::Tab)
            ? 0.0f : lineEndFitAdvances[segmentIndex];
        const float paintAdvance = (kind == SegmentBreakKind::Tab)
            ? segmentWidth : lineEndPaintAdvances[segmentIndex];
        pendingBreakSegIdx = segmentIndex + 1;
        pendingBreakFitW = lineW - segmentWidth + fitAdvance;
        pendingBreakPaintW = lineW - segmentWidth + paintAdvance;
        pendingBrkKind = kind;
        hasPendingBrkKind = true;
    };

    // appendBreakableSegmentFrom: returns true if line finished early
    auto appendBreakableSegmentFrom = [&](int32_t segmentIndex, int32_t startGraphIdx2) -> bool {
        const auto& gWidths = breakableWidths[segmentIndex];
        const auto& gPrefixWidths = breakablePrefixWidths[segmentIndex];
        for (int32_t g = startGraphIdx2; g < static_cast<int32_t>(gWidths.size()); g++) {
            const float gw = getBreakableAdvance(
                gWidths, gPrefixWidths, g,
                engineProfile.preferPrefixWidthsForBreakableRuns
            );

            if (!hasContent) {
                startLineAtGrapheme(segmentIndex, g, gw);
                continue;
            }

            if (lineW + gw > maxWidth + lineFitEpsilon) {
                return true; // signal to finishLine with defaults
            }

            lineW += gw;
            lineEndSegIdx = segmentIndex;
            lineEndGraphIdx = g + 1;
        }

        if (hasContent && lineEndSegIdx == segmentIndex &&
            lineEndGraphIdx == static_cast<int32_t>(gWidths.size())) {
            lineEndSegIdx = segmentIndex + 1;
            lineEndGraphIdx = 0;
        }
        return false;
    };

    // maybeFinishAtSoftHyphen: returns true if line finished and fills outLine
    auto maybeFinishAtSoftHyphen = [&](int32_t segmentIndex) -> bool {
        if (!hasPendingBrkKind || pendingBrkKind != SegmentBreakKind::SoftHyphen || pendingBreakSegIdx < 0)
            return false;

        const auto& gWidths = breakableWidths[segmentIndex];
        if (!gWidths.empty()) {
            // Determine fitWidths
            const std::vector<float>* fitWidths = &gWidths;
            bool usesPrefixWidths = false;
            const std::vector<float>* gPrefixWidths = nullptr;

            if (engineProfile.preferPrefixWidthsForBreakableRuns) {
                gPrefixWidths = &breakablePrefixWidths[segmentIndex];
                if (!gPrefixWidths->empty()) {
                    fitWidths = gPrefixWidths;
                    usesPrefixWidths = true;
                }
            }

            const auto [fitCount, fittedWidth] = fitSoftHyphenBreak(
                *fitWidths,
                lineW,
                maxWidth,
                lineFitEpsilon,
                discretionaryHyphenWidth,
                usesPrefixWidths
            );

            if (fitCount == static_cast<int32_t>(gWidths.size())) {
                lineW = fittedWidth;
                lineEndSegIdx = segmentIndex + 1;
                lineEndGraphIdx = 0;
                clearPendingBreak();
                return false;
            }

            if (fitCount > 0) {
                return finishLine(segmentIndex, fitCount, fittedWidth + discretionaryHyphenWidth);
            }
        }

        if (pendingBreakFitW <= maxWidth + lineFitEpsilon) {
            return finishLine(pendingBreakSegIdx, 0, pendingBreakPaintW);
        }

        return false;
    };

    for (int32_t i = normalizedStart.segmentIndex; i < chunk.endSegmentIndex; i++) {
        const auto kind = kinds[i];
        const int32_t segStartGraphIdx = (i == normalizedStart.segmentIndex)
            ? normalizedStart.graphemeIndex : 0;
        const float w = (kind == SegmentBreakKind::Tab)
            ? getTabAdvance(lineW, tabStopAdvance)
            : widths[i];

        if (kind == SegmentBreakKind::SoftHyphen && segStartGraphIdx == 0) {
            if (hasContent) {
                lineEndSegIdx = i + 1;
                lineEndGraphIdx = 0;
                pendingBreakSegIdx = i + 1;
                pendingBreakFitW = lineW + discretionaryHyphenWidth;
                pendingBreakPaintW = lineW + discretionaryHyphenWidth;
                pendingBrkKind = kind;
                hasPendingBrkKind = true;
            }
            continue;
        }

        if (!hasContent) {
            if (segStartGraphIdx > 0) {
                if (appendBreakableSegmentFrom(i, segStartGraphIdx)) {
                    return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
                }
            } else if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                if (appendBreakableSegmentFrom(i, 0)) {
                    return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
                }
            } else {
                startLineAtSegment(i, w);
            }
            updatePendingBreakForWholeSegment(i, w);
            continue;
        }

        const float newW = lineW + w;
        if (newW > maxWidth + lineFitEpsilon) {
            const float currentBreakFitWidth = lineW +
                (kind == SegmentBreakKind::Tab ? 0.0f : lineEndFitAdvances[i]);
            const float currentBreakPaintWidth = lineW +
                (kind == SegmentBreakKind::Tab ? w : lineEndPaintAdvances[i]);

            if (hasPendingBrkKind &&
                pendingBrkKind == SegmentBreakKind::SoftHyphen &&
                engineProfile.preferEarlySoftHyphenBreak &&
                pendingBreakFitW <= maxWidth + lineFitEpsilon) {
                return finishLine(pendingBreakSegIdx, 0, pendingBreakPaintW);
            }

            if (bool softResult = maybeFinishAtSoftHyphen(i)) {
                return softResult;
            }

            if (canBreakAfter(kind) && currentBreakFitWidth <= maxWidth + lineFitEpsilon) {
                appendWholeSegment(i, w);
                return finishLine(i + 1, 0, currentBreakPaintWidth);
            }

            if (pendingBreakSegIdx >= 0 && pendingBreakFitW <= maxWidth + lineFitEpsilon) {
                return finishLine(pendingBreakSegIdx, 0, pendingBreakPaintW);
            }

            if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                if (bool currentLineResult = finishLine(lineEndSegIdx, lineEndGraphIdx, lineW)) {
                    return currentLineResult;
                }
                if (appendBreakableSegmentFrom(i, 0)) {
                    return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
                }
            }

            return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
        }

        appendWholeSegment(i, w);
        updatePendingBreakForWholeSegment(i, w);
    }

    if (pendingBreakSegIdx == chunk.consumedEndSegmentIndex && lineEndGraphIdx == 0) {
        return finishLine(chunk.consumedEndSegmentIndex, 0, pendingBreakPaintW);
    }

    return finishLine(chunk.consumedEndSegmentIndex, 0, lineW);
}

// ============================================================
// layoutNextLineRangeSimple
// ============================================================

bool TextLineBreaker::layoutNextLineRangeSimple(
    const PreparedLineBreakData& prepared,
    const LineBreakCursor& normalizedStart,
    float maxWidth,
    InternalLayoutLine& outLine
) {
    const auto& widths  = prepared.widths;
    const auto& kinds   = prepared.kinds;
    const auto& breakableWidths  = prepared.breakableWidths;
    const auto& breakablePrefixWidths = prepared.breakablePrefixWidths;

    const EngineProfile engineProfile = TextMeasurer::getEngineProfile();
    const float lineFitEpsilon = engineProfile.lineFitEpsilon;

    float lineW = 0.0f;
    bool hasContent = false;
    const int32_t lineStartSegIdx = normalizedStart.segmentIndex;
    const int32_t lineStartGraphIdx = normalizedStart.graphemeIndex;
    int32_t lineEndSegIdx = lineStartSegIdx;
    int32_t lineEndGraphIdx = lineStartGraphIdx;
    int32_t pendingBreakSegIdx = -1;
    float pendingBreakPaintW = 0.0f;

    auto finishLine = [&](int32_t endSegIdx, int32_t endGraphIdx, float width) -> bool {
        if (!hasContent) return false;
        outLine.startSegmentIndex = lineStartSegIdx;
        outLine.startGraphemeIndex = lineStartGraphIdx;
        outLine.endSegmentIndex = endSegIdx;
        outLine.endGraphemeIndex = endGraphIdx;
        outLine.width = width;
        return true;
    };

    auto startLineAtSegment = [&](int32_t segmentIndex, float width) {
        hasContent = true;
        lineEndSegIdx = segmentIndex + 1;
        lineEndGraphIdx = 0;
        lineW = width;
    };

    auto startLineAtGrapheme = [&](int32_t segmentIndex, int32_t graphemeIndex, float width) {
        hasContent = true;
        lineEndSegIdx = segmentIndex;
        lineEndGraphIdx = graphemeIndex + 1;
        lineW = width;
    };

    auto appendWholeSegment = [&](int32_t segmentIndex, float width) {
        if (!hasContent) {
            startLineAtSegment(segmentIndex, width);
            return;
        }
        lineW += width;
        lineEndSegIdx = segmentIndex + 1;
        lineEndGraphIdx = 0;
    };

    auto updatePendingBreak = [&](int32_t segmentIndex, float segmentWidth) {
        if (!canBreakAfter(kinds[segmentIndex])) return;
        pendingBreakSegIdx = segmentIndex + 1;
        pendingBreakPaintW = lineW - segmentWidth;
    };

    auto appendBreakableSegmentFrom = [&](int32_t segmentIndex, int32_t startGraphemeIndex) -> bool {
        const auto& gWidths = breakableWidths[segmentIndex];
        const auto& gPrefixWidths = breakablePrefixWidths[segmentIndex];
        for (int32_t g = startGraphemeIndex; g < static_cast<int32_t>(gWidths.size()); g++) {
            const float gw = getBreakableAdvance(
                gWidths, gPrefixWidths, g,
                engineProfile.preferPrefixWidthsForBreakableRuns
            );

            if (!hasContent) {
                startLineAtGrapheme(segmentIndex, g, gw);
                continue;
            }

            if (lineW + gw > maxWidth + lineFitEpsilon) {
                return true; // signal: finish line with defaults
            }

            lineW += gw;
            lineEndSegIdx = segmentIndex;
            lineEndGraphIdx = g + 1;
        }

        if (hasContent && lineEndSegIdx == segmentIndex &&
            lineEndGraphIdx == static_cast<int32_t>(gWidths.size())) {
            lineEndSegIdx = segmentIndex + 1;
            lineEndGraphIdx = 0;
        }
        return false;
    };

    for (int32_t i = normalizedStart.segmentIndex; i < static_cast<int32_t>(widths.size()); i++) {
        const float w = widths[i];
        const auto kind = kinds[i];
        const int32_t segStartGraphIdx = (i == normalizedStart.segmentIndex)
            ? normalizedStart.graphemeIndex : 0;

        if (!hasContent) {
            if (segStartGraphIdx > 0) {
                if (appendBreakableSegmentFrom(i, segStartGraphIdx)) {
                    return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
                }
            } else if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                if (appendBreakableSegmentFrom(i, 0)) {
                    return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
                }
            } else {
                startLineAtSegment(i, w);
            }
            updatePendingBreak(i, w);
            continue;
        }

        const float newW = lineW + w;
        if (newW > maxWidth + lineFitEpsilon) {
            if (canBreakAfter(kind)) {
                appendWholeSegment(i, w);
                return finishLine(i + 1, 0, lineW - w);
            }

            if (pendingBreakSegIdx >= 0) {
                return finishLine(pendingBreakSegIdx, 0, pendingBreakPaintW);
            }

            if (w > maxWidth && breakableNotNull(breakableWidths[i])) {
                if (bool currentLineResult = finishLine(lineEndSegIdx, lineEndGraphIdx, lineW)) {
                    return currentLineResult;
                }
                if (appendBreakableSegmentFrom(i, 0)) {
                    return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
                }
            }

            return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
        }

        appendWholeSegment(i, w);
        updatePendingBreak(i, w);
    }

    return finishLine(lineEndSegIdx, lineEndGraphIdx, lineW);
}

} // namespace AgenUIEngine
