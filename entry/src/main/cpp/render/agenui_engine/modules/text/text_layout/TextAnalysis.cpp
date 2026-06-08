#include "TextAnalysis.h"
#include "UnicodeUtils.h"

#include <unordered_set>
#include <algorithm>

namespace AgenUIEngine {

// ============================================================
// Static character-set tables
// ============================================================

static const std::unordered_set<char32_t> kinsokuStartSet = {
    0xFF0C, 0xFF0E, 0xFF01, 0xFF1A, 0xFF1B, 0xFF1F,
    0x3001, 0x3002, 0x30FB,
    0xFF09,
    0x3015, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011,
    0x3017, 0x3019, 0x301B,
    0x30FC,
    0x3005, 0x303B,
    0x309D, 0x309E,
    0x30FD, 0x30FE,
};

static const std::unordered_set<char32_t> kinsokuEndSet = {
    0x0022, 0x0028, 0x005B, 0x007B,
    0x201C, 0x2018, 0x00AB, 0x2039,
    0xFF08,
    0x3014, 0x3008, 0x300A, 0x300C, 0x300E, 0x3010,
    0x3016, 0x3018, 0x301A,
};

static const std::unordered_set<char32_t> forwardStickyGlueSet = {
    0x0027, 0x2019,
};

static const std::unordered_set<char32_t> leftStickyPunctuationSet = {
    0x002E, 0x002C, 0x0021, 0x003F, 0x003A, 0x003B,
    0x060C, 0x061B, 0x061F,
    0x0964, 0x0965,
    0x104A, 0x104B, 0x104C, 0x104D, 0x104F,
    0x0029, 0x005D, 0x007D,
    0x0025,
    0x0022,
    0x201D, 0x2019, 0x00BB, 0x203A,
    0x2026,
};

static const std::unordered_set<char32_t> arabicNoSpaceTrailingPunctuationSet = {
    0x003A, 0x002E, 0x060C, 0x061B,
};

static const std::unordered_set<char32_t> myanmarMedialGlueSet = {
    0x104F,
};

static const std::unordered_set<char32_t> closingQuoteCharsSet = {
    0x201D, 0x2019, 0x00BB, 0x203A,
    0x300D, 0x300F, 0x3011, 0x300B, 0x3009, 0x3015, 0xFF09,
};

static const std::unordered_set<char32_t> numericJoinerCharsSet = {
    0x003A, 0x002D, 0x002F, 0x00D7, 0x002C, 0x002E, 0x002B,
    0x2013, 0x2014,
};

// ============================================================
// Internal helper: WhiteSpaceProfile
// ============================================================

struct WhiteSpaceProfile {
    WhiteSpaceMode mode;
    bool preserveOrdinarySpaces;
    bool preserveHardBreaks;
};

static WhiteSpaceProfile getWhiteSpaceProfile(WhiteSpaceMode whiteSpace) {
    if (whiteSpace == WhiteSpaceMode::PreWrap) {
        return { WhiteSpaceMode::PreWrap, true, true };
    }
    return { WhiteSpaceMode::Normal, false, false };
}

// ============================================================
// Whitespace normalization
// ============================================================

static std::u32string normalizeWhitespaceNormal(const std::u32string& text) {
    // Check if normalization is needed: any tab/newline/formfeed, or
    // two-or-more consecutive spaces, or leading/trailing space.
    bool needsNorm = false;
    int32_t len = static_cast<int32_t>(text.size());

    // Check for tab, newline, carriage return, form feed
    for (int32_t i = 0; i < len; i++) {
        char32_t c = text[i];
        if (c == 0x09 || c == 0x0A || c == 0x0D || c == 0x0C) {
            needsNorm = true;
            break;
        }
    }

    // Check for two-or-more consecutive spaces
    if (!needsNorm) {
        bool inSpaces = false;
        for (int32_t i = 0; i < len; i++) {
            if (text[i] == 0x20) {
                if (inSpaces) { needsNorm = true; break; }
                inSpaces = true;
            } else {
                inSpaces = false;
            }
        }
    }

    // Check for leading space
    if (!needsNorm && len > 0 && text[0] == 0x20) {
        needsNorm = true;
    }

    // Check for trailing space
    if (!needsNorm && len > 0 && text[len - 1] == 0x20) {
        needsNorm = true;
    }

    if (!needsNorm) return text;

    // Replace runs of collapsible whitespace (space, tab, LF, CR, FF) with single space
    std::u32string result;
    result.reserve(len);
    bool inWS = false;
    for (int32_t i = 0; i < len; i++) {
        char32_t c = text[i];
        bool isWS = (c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D || c == 0x0C);
        if (isWS) {
            if (!inWS) {
                result += 0x20;
                inWS = true;
            }
        } else {
            result += c;
            inWS = false;
        }
    }

    // Strip leading space
    if (!result.empty() && result[0] == 0x20) {
        result.erase(result.begin());
    }
    // Strip trailing space
    if (!result.empty() && result.back() == 0x20) {
        result.pop_back();
    }

    return result;
}

static std::u32string normalizeWhitespacePreWrap(const std::u32string& text) {
    // Replace \r\n with \n, then replace remaining \r and \f with \n
    std::u32string result;
    result.reserve(text.size());
    int32_t len = static_cast<int32_t>(text.size());
    for (int32_t i = 0; i < len; i++) {
        if (text[i] == 0x0D) {
            if (i + 1 < len && text[i + 1] == 0x0A) {
                result += 0x0A;
                i++; // skip the \n after \r
            } else {
                result += 0x0A;
            }
        } else if (text[i] == 0x0C) {
            result += 0x0A;
        } else {
            result += text[i];
        }
    }
    return result;
}

// ============================================================
// Character property helpers
// ============================================================

static bool containsArabicScript(const std::u32string& text) {
    for (char32_t c : text) {
        if (isArabicScript(c)) return true;
    }
    return false;
}

static bool isEscapedQuoteClusterSegment(const std::u32string& segment) {
    bool sawQuote = false;
    for (char32_t ch : segment) {
        if (ch == 0x5C || isCombiningMark(ch)) continue; // backslash or combining mark
        if (kinsokuEndSet.count(ch) || leftStickyPunctuationSet.count(ch) || forwardStickyGlueSet.count(ch)) {
            sawQuote = true;
            continue;
        }
        return false;
    }
    return sawQuote;
}

static bool isLeftStickyPunctuationSegment(const std::u32string& segment) {
    if (isEscapedQuoteClusterSegment(segment)) return true;
    bool sawPunctuation = false;
    for (char32_t ch : segment) {
        if (leftStickyPunctuationSet.count(ch)) {
            sawPunctuation = true;
            continue;
        }
        if (sawPunctuation && isCombiningMark(ch)) continue;
        return false;
    }
    return sawPunctuation;
}

static bool isCJKLineStartProhibitedSegment(const std::u32string& segment) {
    for (char32_t ch : segment) {
        if (!kinsokuStartSet.count(ch) && !leftStickyPunctuationSet.count(ch)) return false;
    }
    return !segment.empty();
}

static bool isForwardStickyClusterSegment(const std::u32string& segment) {
    if (isEscapedQuoteClusterSegment(segment)) return true;
    for (char32_t ch : segment) {
        if (!kinsokuEndSet.count(ch) && !forwardStickyGlueSet.count(ch) && !isCombiningMark(ch)) return false;
    }
    return !segment.empty();
}

struct HeadTail {
    std::u32string head;
    std::u32string tail;
};

static HeadTail* splitTrailingForwardStickyCluster(const std::u32string& text, HeadTail& out) {
    int32_t splitIndex = static_cast<int32_t>(text.size());

    while (splitIndex > 0) {
        char32_t ch = text[splitIndex - 1];
        if (isCombiningMark(ch)) {
            splitIndex--;
            continue;
        }
        if (kinsokuEndSet.count(ch) || forwardStickyGlueSet.count(ch)) {
            splitIndex--;
            continue;
        }
        break;
    }

    if (splitIndex <= 0 || splitIndex == static_cast<int32_t>(text.size())) return nullptr;

    out.head = text.substr(0, splitIndex);
    out.tail = text.substr(splitIndex);
    return &out;
}

static bool isRepeatedSingleCharRun(const std::u32string& segment, char32_t ch) {
    if (segment.empty()) return false;
    for (char32_t c : segment) {
        if (c != ch) return false;
    }
    return true;
}

static bool endsWithArabicNoSpacePunctuation(const std::u32string& segment) {
    if (!containsArabicScript(segment) || segment.empty()) return false;
    return arabicNoSpaceTrailingPunctuationSet.count(segment.back()) != 0;
}

static bool endsWithMyanmarMedialGlue(const std::u32string& segment) {
    if (segment.empty()) return false;
    return myanmarMedialGlueSet.count(segment.back()) != 0;
}

struct SpaceMarks {
    std::u32string space;
    std::u32string marks;
};

static SpaceMarks* splitLeadingSpaceAndMarks(const std::u32string& segment, SpaceMarks& out) {
    if (segment.size() < 2 || segment[0] != 0x20) return nullptr;
    std::u32string marks = segment.substr(1);
    if (marks.empty()) return nullptr;
    for (char32_t c : marks) {
        if (!isCombiningMark(c)) return nullptr;
    }
    out.space = std::u32string(1, 0x20);
    out.marks = std::move(marks);
    return &out;
}

// ============================================================
// Segment break classification
// ============================================================

static SegmentBreakKind classifySegmentBreakChar(char32_t ch, const WhiteSpaceProfile& wsp) {
    if (wsp.preserveOrdinarySpaces || wsp.preserveHardBreaks) {
        if (ch == 0x20) return SegmentBreakKind::PreservedSpace;
        if (ch == 0x09) return SegmentBreakKind::Tab;
        if (wsp.preserveHardBreaks && ch == 0x0A) return SegmentBreakKind::HardBreak;
    }
    if (ch == 0x20) return SegmentBreakKind::Space;
    if (ch == 0x00A0 || ch == 0x202F || ch == 0x2060 || ch == 0xFEFF) {
        return SegmentBreakKind::Glue;
    }
    if (ch == 0x200B) return SegmentBreakKind::ZeroWidthBreak;
    if (ch == 0x00AD) return SegmentBreakKind::SoftHyphen;
    return SegmentBreakKind::Text;
}

static std::vector<SegmentationPiece> splitSegmentByBreakKind(
    const std::u32string& segment,
    bool isWordLike,
    int32_t start,
    const WhiteSpaceProfile& wsp)
{
    std::vector<SegmentationPiece> pieces;
    SegmentBreakKind currentKind = SegmentBreakKind::Text; // sentinel, we track via currentKindSet
    bool currentKindSet = false;
    std::u32string currentText;
    int32_t currentStart = start;
    bool currentWordLike = false;
    int32_t offset = 0;

    for (char32_t ch : segment) {
        SegmentBreakKind kind = classifySegmentBreakChar(ch, wsp);
        bool wordLike = (kind == SegmentBreakKind::Text) && isWordLike;

        if (currentKindSet && kind == currentKind && wordLike == currentWordLike) {
            currentText += ch;
            offset++;
            continue;
        }

        if (currentKindSet) {
            pieces.push_back({ currentText, currentWordLike, currentKind, currentStart });
        }

        currentKind = kind;
        currentText = std::u32string(1, ch);
        currentStart = start + offset;
        currentWordLike = wordLike;
        currentKindSet = true;
        offset++;
    }

    if (currentKindSet) {
        pieces.push_back({ currentText, currentWordLike, currentKind, currentStart });
    }

    return pieces;
}

static bool isTextRunBoundary(SegmentBreakKind kind) {
    return kind == SegmentBreakKind::Space ||
           kind == SegmentBreakKind::PreservedSpace ||
           kind == SegmentBreakKind::ZeroWidthBreak ||
           kind == SegmentBreakKind::HardBreak;
}

// ============================================================
// URL detection helpers
// ============================================================

static bool isUrlSchemeSegment(const std::u32string& text) {
    // Matches: ^[A-Za-z][A-Za-z0-9+.-]*:$
    if (text.empty()) return false;
    char32_t first = text[0];
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z'))) return false;
    int32_t len = static_cast<int32_t>(text.size());
    for (int32_t i = 1; i < len - 1; i++) {
        char32_t c = text[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.') {
            continue;
        }
        return false;
    }
    return len >= 2 && text.back() == ':';
}

static bool startsWith(const std::u32string& text, const char* prefix) {
    int32_t i = 0;
    while (prefix[i] != '\0') {
        if (i >= static_cast<int32_t>(text.size())) return false;
        if (text[i] != static_cast<char32_t>(prefix[i])) return false;
        i++;
    }
    return true;
}

static bool containsChar32(const std::u32string& text, char32_t target) {
    for (char32_t c : text) {
        if (c == target) return true;
    }
    return false;
}

static bool isUrlLikeRunStart(const MergedSegmentation& seg, int32_t index) {
    const std::u32string& text = seg.texts[index];
    if (startsWith(text, "www.")) return true;
    if (isUrlSchemeSegment(text) &&
        index + 1 < seg.len &&
        seg.kinds[index + 1] == SegmentBreakKind::Text &&
        seg.texts[index + 1] == U"//") {
        return true;
    }
    return false;
}

static bool isUrlQueryBoundarySegment(const std::u32string& text) {
    if (!containsChar32(text, '?')) return false;
    // Check if text contains "://" or starts with "www."
    // Check for ://
    for (size_t i = 0; i + 2 < text.size(); i++) {
        if (text[i] == ':' && text[i + 1] == '/' && text[i + 2] == '/') return true;
    }
    if (startsWith(text, "www.")) return true;
    return false;
}

// ============================================================
// Numeric helpers
// ============================================================

static bool segmentContainsDecimalDigit(const std::u32string& text) {
    for (char32_t ch : text) {
        if (isDecimalDigit(ch)) return true;
    }
    return false;
}

static bool isNumericRunSegment(const std::u32string& text) {
    if (text.empty()) return false;
    for (char32_t ch : text) {
        if (isDecimalDigit(ch) || numericJoinerCharsSet.count(ch)) continue;
        return false;
    }
    return true;
}

// ============================================================
// Ascii punctuation chain helpers
// ============================================================

static bool isAsciiPunctuationChainSegment(const std::u32string& text) {
    // ^[A-Za-z0-9_]+[,:;]*$
    if (text.empty()) return false;
    int32_t len = static_cast<int32_t>(text.size());
    int32_t i = 0;
    // Must start with at least one alphanumeric/underscore
    char32_t first = text[0];
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') ||
          (first >= '0' && first <= '9') || first == '_')) {
        return false;
    }
    while (i < len) {
        char32_t c = text[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_') {
            i++;
            continue;
        }
        break;
    }
    // Remaining must be , : ;
    while (i < len) {
        char32_t c = text[i];
        if (c == ',' || c == ':' || c == ';') {
            i++;
            continue;
        }
        return false;
    }
    return true;
}

static bool hasAsciiPunctuationChainTrailingJoiners(const std::u32string& text) {
    // /[,:;]+$/
    if (text.empty()) return false;
    char32_t last = text.back();
    return last == ',' || last == ':' || last == ';';
}

// ============================================================
// Merge / transform pipeline functions
// ============================================================

static MergedSegmentation mergeUrlLikeRuns(const MergedSegmentation& segmentation) {
    std::vector<std::u32string> texts = segmentation.texts;
    std::vector<bool> isWordLike = segmentation.isWordLike;
    std::vector<SegmentBreakKind> kinds = segmentation.kinds;
    std::vector<int32_t> starts = segmentation.starts;

    for (int32_t i = 0; i < segmentation.len; i++) {
        if (kinds[i] != SegmentBreakKind::Text || !isUrlLikeRunStart(segmentation, i)) continue;

        int32_t j = i + 1;
        while (j < segmentation.len && !isTextRunBoundary(kinds[j])) {
            texts[i] += texts[j];
            isWordLike[i] = true;
            bool endsQueryPrefix = containsChar32(texts[j], '?');
            kinds[j] = SegmentBreakKind::Text;
            texts[j] = U"";
            j++;
            if (endsQueryPrefix) break;
        }
    }

    int32_t compactLen = 0;
    for (int32_t read = 0; read < static_cast<int32_t>(texts.size()); read++) {
        const std::u32string& text = texts[read];
        if (text.empty()) continue;
        if (compactLen != read) {
            texts[compactLen] = text;
            isWordLike[compactLen] = isWordLike[read];
            kinds[compactLen] = kinds[read];
            starts[compactLen] = starts[read];
        }
        compactLen++;
    }

    texts.resize(compactLen);
    isWordLike.resize(compactLen);
    kinds.resize(compactLen);
    starts.resize(compactLen);

    return { compactLen, texts, isWordLike, kinds, starts };
}

static MergedSegmentation mergeUrlQueryRuns(const MergedSegmentation& segmentation) {
    std::vector<std::u32string> texts;
    std::vector<bool> isWordLike;
    std::vector<SegmentBreakKind> kinds;
    std::vector<int32_t> starts;

    for (int32_t i = 0; i < segmentation.len; i++) {
        const std::u32string& text = segmentation.texts[i];
        texts.push_back(text);
        isWordLike.push_back(segmentation.isWordLike[i]);
        kinds.push_back(segmentation.kinds[i]);
        starts.push_back(segmentation.starts[i]);

        if (!isUrlQueryBoundarySegment(text)) continue;

        int32_t nextIndex = i + 1;
        if (nextIndex >= segmentation.len || isTextRunBoundary(segmentation.kinds[nextIndex])) {
            continue;
        }

        std::u32string queryText;
        int32_t queryStart = segmentation.starts[nextIndex];
        int32_t j = nextIndex;
        while (j < segmentation.len && !isTextRunBoundary(segmentation.kinds[j])) {
            queryText += segmentation.texts[j];
            j++;
        }

        if (!queryText.empty()) {
            texts.push_back(queryText);
            isWordLike.push_back(true);
            kinds.push_back(SegmentBreakKind::Text);
            starts.push_back(queryStart);
            i = j - 1;
        }
    }

    return { static_cast<int32_t>(texts.size()), texts, isWordLike, kinds, starts };
}

static MergedSegmentation mergeNumericRuns(const MergedSegmentation& segmentation) {
    std::vector<std::u32string> texts;
    std::vector<bool> isWordLike;
    std::vector<SegmentBreakKind> kinds;
    std::vector<int32_t> starts;

    for (int32_t i = 0; i < segmentation.len; i++) {
        const std::u32string& text = segmentation.texts[i];
        SegmentBreakKind kind = segmentation.kinds[i];

        if (kind == SegmentBreakKind::Text && isNumericRunSegment(text) && segmentContainsDecimalDigit(text)) {
            std::u32string mergedText = text;
            int32_t j = i + 1;
            while (j < segmentation.len &&
                   segmentation.kinds[j] == SegmentBreakKind::Text &&
                   isNumericRunSegment(segmentation.texts[j])) {
                mergedText += segmentation.texts[j];
                j++;
            }

            texts.push_back(mergedText);
            isWordLike.push_back(true);
            kinds.push_back(SegmentBreakKind::Text);
            starts.push_back(segmentation.starts[i]);
            i = j - 1;
            continue;
        }

        texts.push_back(text);
        isWordLike.push_back(segmentation.isWordLike[i]);
        kinds.push_back(kind);
        starts.push_back(segmentation.starts[i]);
    }

    return { static_cast<int32_t>(texts.size()), texts, isWordLike, kinds, starts };
}

static std::vector<std::u32string> splitOnChar(const std::u32string& text, char32_t delim) {
    std::vector<std::u32string> parts;
    std::u32string current;
    for (char32_t c : text) {
        if (c == delim) {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

static MergedSegmentation splitHyphenatedNumericRuns(const MergedSegmentation& segmentation) {
    std::vector<std::u32string> texts;
    std::vector<bool> isWordLike;
    std::vector<SegmentBreakKind> kinds;
    std::vector<int32_t> starts;

    for (int32_t i = 0; i < segmentation.len; i++) {
        const std::u32string& text = segmentation.texts[i];
        if (segmentation.kinds[i] == SegmentBreakKind::Text && containsChar32(text, '-')) {
            std::vector<std::u32string> parts = splitOnChar(text, '-');
            bool shouldSplit = parts.size() > 1;
            if (shouldSplit) {
                for (size_t j = 0; j < parts.size(); j++) {
                    const std::u32string& part = parts[j];
                    if (part.empty() || !segmentContainsDecimalDigit(part) || !isNumericRunSegment(part)) {
                        shouldSplit = false;
                        break;
                    }
                }
            }

            if (shouldSplit) {
                int32_t offset = 0;
                for (size_t j = 0; j < parts.size(); j++) {
                    const std::u32string& part = parts[j];
                    std::u32string splitText = part;
                    if (j < parts.size() - 1) {
                        splitText += '-';
                    }
                    texts.push_back(splitText);
                    isWordLike.push_back(true);
                    kinds.push_back(SegmentBreakKind::Text);
                    starts.push_back(segmentation.starts[i] + offset);
                    offset += static_cast<int32_t>(splitText.size());
                }
                continue;
            }
        }

        texts.push_back(text);
        isWordLike.push_back(segmentation.isWordLike[i]);
        kinds.push_back(segmentation.kinds[i]);
        starts.push_back(segmentation.starts[i]);
    }

    return { static_cast<int32_t>(texts.size()), texts, isWordLike, kinds, starts };
}

static MergedSegmentation mergeAsciiPunctuationChains(const MergedSegmentation& segmentation) {
    std::vector<std::u32string> texts;
    std::vector<bool> isWordLike;
    std::vector<SegmentBreakKind> kinds;
    std::vector<int32_t> starts;

    for (int32_t i = 0; i < segmentation.len; i++) {
        const std::u32string& text = segmentation.texts[i];
        SegmentBreakKind kind = segmentation.kinds[i];
        bool wordLike = segmentation.isWordLike[i];

        if (kind == SegmentBreakKind::Text && wordLike && isAsciiPunctuationChainSegment(text)) {
            std::u32string mergedText = text;
            int32_t j = i + 1;

            while (hasAsciiPunctuationChainTrailingJoiners(mergedText) &&
                   j < segmentation.len &&
                   segmentation.kinds[j] == SegmentBreakKind::Text &&
                   segmentation.isWordLike[j] &&
                   isAsciiPunctuationChainSegment(segmentation.texts[j])) {
                mergedText += segmentation.texts[j];
                j++;
            }

            texts.push_back(mergedText);
            isWordLike.push_back(true);
            kinds.push_back(SegmentBreakKind::Text);
            starts.push_back(segmentation.starts[i]);
            i = j - 1;
            continue;
        }

        texts.push_back(text);
        isWordLike.push_back(wordLike);
        kinds.push_back(kind);
        starts.push_back(segmentation.starts[i]);
    }

    return { static_cast<int32_t>(texts.size()), texts, isWordLike, kinds, starts };
}

static MergedSegmentation mergeGlueConnectedTextRuns(const MergedSegmentation& segmentation) {
    std::vector<std::u32string> texts;
    std::vector<bool> isWordLike;
    std::vector<SegmentBreakKind> kinds;
    std::vector<int32_t> starts;

    int32_t read = 0;
    while (read < segmentation.len) {
        std::u32string text = segmentation.texts[read];
        bool wordLike = segmentation.isWordLike[read];
        SegmentBreakKind kind = segmentation.kinds[read];
        int32_t start = segmentation.starts[read];

        if (kind == SegmentBreakKind::Glue) {
            std::u32string glueText = text;
            int32_t glueStart = start;
            read++;
            while (read < segmentation.len && segmentation.kinds[read] == SegmentBreakKind::Glue) {
                glueText += segmentation.texts[read];
                read++;
            }

            if (read < segmentation.len && segmentation.kinds[read] == SegmentBreakKind::Text) {
                text = glueText + segmentation.texts[read];
                wordLike = segmentation.isWordLike[read];
                kind = SegmentBreakKind::Text;
                start = glueStart;
                read++;
            } else {
                texts.push_back(glueText);
                isWordLike.push_back(false);
                kinds.push_back(SegmentBreakKind::Glue);
                starts.push_back(glueStart);
                continue;
            }
        } else {
            read++;
        }

        if (kind == SegmentBreakKind::Text) {
            while (read < segmentation.len && segmentation.kinds[read] == SegmentBreakKind::Glue) {
                std::u32string glueText;
                while (read < segmentation.len && segmentation.kinds[read] == SegmentBreakKind::Glue) {
                    glueText += segmentation.texts[read];
                    read++;
                }

                if (read < segmentation.len && segmentation.kinds[read] == SegmentBreakKind::Text) {
                    text += glueText + segmentation.texts[read];
                    wordLike = wordLike || segmentation.isWordLike[read];
                    read++;
                    continue;
                }

                text += glueText;
            }
        }

        texts.push_back(text);
        isWordLike.push_back(wordLike);
        kinds.push_back(kind);
        starts.push_back(start);
    }

    return { static_cast<int32_t>(texts.size()), texts, isWordLike, kinds, starts };
}

static MergedSegmentation carryTrailingForwardStickyAcrossCJKBoundary(const MergedSegmentation& segmentation) {
    std::vector<std::u32string> texts = segmentation.texts;
    std::vector<bool> isWordLike = segmentation.isWordLike;
    std::vector<SegmentBreakKind> kinds = segmentation.kinds;
    std::vector<int32_t> starts = segmentation.starts;

    for (int32_t i = 0; i < static_cast<int32_t>(texts.size()) - 1; i++) {
        if (kinds[i] != SegmentBreakKind::Text || kinds[i + 1] != SegmentBreakKind::Text) continue;
        if (!isCJK(texts[i]) || !isCJK(texts[i + 1])) continue;

        HeadTail split;
        if (splitTrailingForwardStickyCluster(texts[i], split) == nullptr) continue;

        texts[i] = split.head;
        texts[i + 1] = split.tail + texts[i + 1];
        starts[i + 1] = starts[i] + static_cast<int32_t>(split.head.size());
    }

    return { static_cast<int32_t>(texts.size()), texts, isWordLike, kinds, starts };
}

// ============================================================
// Build merged segmentation (core analysis)
// ============================================================

static MergedSegmentation buildMergedSegmentation(
    const std::u32string& normalized,
    const AnalysisProfile& profile,
    const WhiteSpaceProfile& wsp)
{
    WordSegmenter wordSegmenter;
    std::vector<WordSegmenter::Segment> wordSegments = wordSegmenter.segment(normalized);

    int32_t mergedLen = 0;
    std::vector<std::u32string> mergedTexts;
    std::vector<bool> mergedWordLike;
    std::vector<SegmentBreakKind> mergedKinds;
    std::vector<int32_t> mergedStarts;

    for (const auto& s : wordSegments) {
        std::vector<SegmentationPiece> pieces = splitSegmentByBreakKind(
            s.text, s.isWordLike, s.start, wsp);

        for (const auto& piece : pieces) {
            bool isText = (piece.kind == SegmentBreakKind::Text);

            if (
                profile.carryCJKAfterClosingQuote &&
                isText &&
                mergedLen > 0 &&
                mergedKinds[mergedLen - 1] == SegmentBreakKind::Text &&
                isCJK(piece.text) &&
                isCJK(mergedTexts[mergedLen - 1]) &&
                TextAnalyzer::endsWithClosingQuote(mergedTexts[mergedLen - 1])
            ) {
                mergedTexts[mergedLen - 1] += piece.text;
                mergedWordLike[mergedLen - 1] = mergedWordLike[mergedLen - 1] || piece.isWordLike;
            } else if (
                isText &&
                mergedLen > 0 &&
                mergedKinds[mergedLen - 1] == SegmentBreakKind::Text &&
                isCJKLineStartProhibitedSegment(piece.text) &&
                isCJK(mergedTexts[mergedLen - 1])
            ) {
                mergedTexts[mergedLen - 1] += piece.text;
                mergedWordLike[mergedLen - 1] = mergedWordLike[mergedLen - 1] || piece.isWordLike;
            } else if (
                isText &&
                mergedLen > 0 &&
                mergedKinds[mergedLen - 1] == SegmentBreakKind::Text &&
                endsWithMyanmarMedialGlue(mergedTexts[mergedLen - 1])
            ) {
                mergedTexts[mergedLen - 1] += piece.text;
                mergedWordLike[mergedLen - 1] = mergedWordLike[mergedLen - 1] || piece.isWordLike;
            } else if (
                isText &&
                mergedLen > 0 &&
                mergedKinds[mergedLen - 1] == SegmentBreakKind::Text &&
                piece.isWordLike &&
                containsArabicScript(piece.text) &&
                endsWithArabicNoSpacePunctuation(mergedTexts[mergedLen - 1])
            ) {
                mergedTexts[mergedLen - 1] += piece.text;
                mergedWordLike[mergedLen - 1] = true;
            } else if (
                isText &&
                !piece.isWordLike &&
                mergedLen > 0 &&
                mergedKinds[mergedLen - 1] == SegmentBreakKind::Text &&
                piece.text.size() == 1 &&
                piece.text[0] != '-' &&
                piece.text[0] != 0x2014 &&
                isRepeatedSingleCharRun(mergedTexts[mergedLen - 1], piece.text[0])
            ) {
                mergedTexts[mergedLen - 1] += piece.text;
            } else if (
                isText &&
                !piece.isWordLike &&
                mergedLen > 0 &&
                mergedKinds[mergedLen - 1] == SegmentBreakKind::Text &&
                (
                    isLeftStickyPunctuationSegment(piece.text) ||
                    (piece.text == U"-" && mergedWordLike[mergedLen - 1])
                )
            ) {
                mergedTexts[mergedLen - 1] += piece.text;
            } else {
                if (static_cast<int32_t>(mergedTexts.size()) <= mergedLen) {
                    mergedTexts.push_back(piece.text);
                    mergedWordLike.push_back(piece.isWordLike);
                    mergedKinds.push_back(piece.kind);
                    mergedStarts.push_back(piece.start);
                } else {
                    mergedTexts[mergedLen] = piece.text;
                    mergedWordLike[mergedLen] = piece.isWordLike;
                    mergedKinds[mergedLen] = piece.kind;
                    mergedStarts[mergedLen] = piece.start;
                }
                mergedLen++;
            }
        }
    }

    // Ensure vectors are exactly mergedLen size
    mergedTexts.resize(mergedLen);
    mergedWordLike.resize(mergedLen);
    mergedKinds.resize(mergedLen);
    mergedStarts.resize(mergedLen);

    // Forward merge: escaped quote clusters attach to preceding text
    for (int32_t i = 1; i < mergedLen; i++) {
        if (
            mergedKinds[i] == SegmentBreakKind::Text &&
            !mergedWordLike[i] &&
            isEscapedQuoteClusterSegment(mergedTexts[i]) &&
            mergedKinds[i - 1] == SegmentBreakKind::Text
        ) {
            mergedTexts[i - 1] += mergedTexts[i];
            mergedWordLike[i - 1] = mergedWordLike[i - 1] || mergedWordLike[i];
            mergedTexts[i].clear();
        }
    }

    // Backward merge: forward-sticky clusters carry to the next text segment
    for (int32_t i = mergedLen - 2; i >= 0; i--) {
        if (mergedKinds[i] == SegmentBreakKind::Text && !mergedWordLike[i] && isForwardStickyClusterSegment(mergedTexts[i])) {
            int32_t j = i + 1;
            while (j < mergedLen && mergedTexts[j].empty()) j++;
            if (j < mergedLen && mergedKinds[j] == SegmentBreakKind::Text) {
                mergedTexts[j] = mergedTexts[i] + mergedTexts[j];
                mergedStarts[j] = mergedStarts[i];
                mergedTexts[i].clear();
            }
        }
    }

    // Compact: remove empty entries
    int32_t compactLen = 0;
    for (int32_t read = 0; read < mergedLen; read++) {
        const std::u32string& text = mergedTexts[read];
        if (text.empty()) continue;
        if (compactLen != read) {
            mergedTexts[compactLen] = text;
            mergedWordLike[compactLen] = mergedWordLike[read];
            mergedKinds[compactLen] = mergedKinds[read];
            mergedStarts[compactLen] = mergedStarts[read];
        }
        compactLen++;
    }

    mergedTexts.resize(compactLen);
    mergedWordLike.resize(compactLen);
    mergedKinds.resize(compactLen);
    mergedStarts.resize(compactLen);

    MergedSegmentation compacted = {
        compactLen, mergedTexts, mergedWordLike, mergedKinds, mergedStarts
    };

    // Pipeline: glue → compact already done → glue → mergeAsciiPunctuation →
    // splitHyphen → mergeNumeric → mergeUrlQuery → mergeUrlLike → carryCJK
    MergedSegmentation withGlue = mergeGlueConnectedTextRuns(compacted);
    MergedSegmentation result = carryTrailingForwardStickyAcrossCJKBoundary(
        mergeAsciiPunctuationChains(
            splitHyphenatedNumericRuns(
                mergeNumericRuns(
                    mergeUrlQueryRuns(
                        mergeUrlLikeRuns(withGlue)
                    )
                )
            )
        )
    );

    // Split leading space+combining marks for Arabic script attachment
    for (int32_t i = 0; i < result.len - 1; i++) {
        SpaceMarks sm;
        if (splitLeadingSpaceAndMarks(result.texts[i], sm) == nullptr) continue;
        if (
            (result.kinds[i] != SegmentBreakKind::Space && result.kinds[i] != SegmentBreakKind::PreservedSpace) ||
            result.kinds[i + 1] != SegmentBreakKind::Text ||
            !containsArabicScript(result.texts[i + 1])
        ) {
            continue;
        }

        result.texts[i] = sm.space;
        result.isWordLike[i] = false;
        result.kinds[i] = (result.kinds[i] == SegmentBreakKind::PreservedSpace)
            ? SegmentBreakKind::PreservedSpace
            : SegmentBreakKind::Space;
        result.texts[i + 1] = sm.marks + result.texts[i + 1];
        result.starts[i + 1] = result.starts[i] + static_cast<int32_t>(sm.space.size());
    }

    return result;
}

// ============================================================
// Compile analysis chunks
// ============================================================

static std::vector<AnalysisChunk> compileAnalysisChunks(
    const MergedSegmentation& segmentation,
    const WhiteSpaceProfile& wsp)
{
    if (segmentation.len == 0) return {};
    if (!wsp.preserveHardBreaks) {
        return {{
            0,
            segmentation.len,
            segmentation.len,
        }};
    }

    std::vector<AnalysisChunk> chunks;
    int32_t startSegmentIndex = 0;

    for (int32_t i = 0; i < segmentation.len; i++) {
        if (segmentation.kinds[i] != SegmentBreakKind::HardBreak) continue;

        chunks.push_back({
            startSegmentIndex,
            i,
            i + 1,
        });
        startSegmentIndex = i + 1;
    }

    if (startSegmentIndex < segmentation.len) {
        chunks.push_back({
            startSegmentIndex,
            segmentation.len,
            segmentation.len,
        });
    }

    return chunks;
}

// ============================================================
// TextAnalyzer public API
// ============================================================

TextAnalysis TextAnalyzer::analyzeText(
    const std::u32string& text,
    const AnalysisProfile& profile,
    WhiteSpaceMode whiteSpace)
{
    WhiteSpaceProfile wsp = getWhiteSpaceProfile(whiteSpace);

    std::u32string normalized = (wsp.mode == WhiteSpaceMode::PreWrap)
        ? normalizeWhitespacePreWrap(text)
        : normalizeWhitespaceNormal(text);

    if (normalized.empty()) {
        return {
            normalized,
            {},
            0,
            {},
            {},
            {},
            {},
        };
    }

    MergedSegmentation segmentation = buildMergedSegmentation(normalized, profile, wsp);

    return {
        normalized,
        compileAnalysisChunks(segmentation, wsp),
        segmentation.len,
        segmentation.texts,
        segmentation.isWordLike,
        segmentation.kinds,
        segmentation.starts,
    };
}

bool TextAnalyzer::isCJK(char32_t c) {
    return AgenUIEngine::isCJK(c);
}

bool TextAnalyzer::isCJK(const std::u32string& s) {
    return AgenUIEngine::isCJK(s);
}

bool TextAnalyzer::endsWithClosingQuote(const std::u32string& text) {
    for (int32_t i = static_cast<int32_t>(text.size()) - 1; i >= 0; i--) {
        char32_t ch = text[i];
        if (closingQuoteCharsSet.count(ch)) return true;
        if (!leftStickyPunctuationSet.count(ch)) return false;
    }
    return false;
}

const std::vector<char32_t>& TextAnalyzer::getKinsokuStart() {
    static const std::vector<char32_t> chars = {
        0xFF0C, 0xFF0E, 0xFF01, 0xFF1A, 0xFF1B, 0xFF1F,
        0x3001, 0x3002, 0x30FB,
        0xFF09,
        0x3015, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011,
        0x3017, 0x3019, 0x301B,
        0x30FC,
        0x3005, 0x303B,
        0x309D, 0x309E,
        0x30FD, 0x30FE,
    };
    return chars;
}

const std::vector<char32_t>& TextAnalyzer::getKinsokuEnd() {
    static const std::vector<char32_t> chars = {
        0x0022, 0x0028, 0x005B, 0x007B,
        0x201C, 0x2018, 0x00AB, 0x2039,
        0xFF08,
        0x3014, 0x3008, 0x300A, 0x300C, 0x300E, 0x3010,
        0x3016, 0x3018, 0x301A,
    };
    return chars;
}

const std::vector<char32_t>& TextAnalyzer::getLeftStickyPunctuation() {
    static const std::vector<char32_t> chars = {
        0x002E, 0x002C, 0x0021, 0x003F, 0x003A, 0x003B,
        0x060C, 0x061B, 0x061F,
        0x0964, 0x0965,
        0x104A, 0x104B, 0x104C, 0x104D, 0x104F,
        0x0029, 0x005D, 0x007D,
        0x0025,
        0x0022,
        0x201D, 0x2019, 0x00BB, 0x203A,
        0x2026,
    };
    return chars;
}

} // namespace AgenUIEngine
