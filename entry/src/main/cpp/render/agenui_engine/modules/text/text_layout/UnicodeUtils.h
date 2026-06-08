#ifndef AGENUI_UNICODE_UTILS_H
#define AGENUI_UNICODE_UTILS_H

#include <string>
#include <vector>
#include <cstdint>

namespace AgenUIEngine {

// ============================================================
// ICU C API wrappers for Unicode segmentation and property queries
// ============================================================

/**
 * Word segmenter: splits text into word-boundary segments.
 * Wraps ICU ubrk_open(UBRK_WORD, ...).
 */
class WordSegmenter {
public:
    WordSegmenter();
    ~WordSegmenter();

    // Non-copyable
    WordSegmenter(const WordSegmenter&) = delete;
    WordSegmenter& operator=(const WordSegmenter&) = delete;

    struct Segment {
        std::u32string text;
        bool isWordLike;
        int32_t start;  // byte offset in original string
    };

    /** Segment UTF-32 text into word-like and non-word-like pieces. */
    std::vector<Segment> segment(const std::u32string& text) const;

private:
    // The locale string passed to ICU; empty = default
    static const char* s_locale;
};

/**
 * Grapheme segmenter: splits text into user-perceived character clusters.
 * Wraps ICU ubrk_open(UBRK_CHARACTER, ...).
 */
class GraphemeSegmenter {
public:
    GraphemeSegmenter();
    ~GraphemeSegmenter();

    GraphemeSegmenter(const GraphemeSegmenter&) = delete;
    GraphemeSegmenter& operator=(const GraphemeSegmenter&) = delete;

    struct Segment {
        std::u32string text;
        int32_t start;
    };

    /** Segment UTF-32 text into grapheme clusters. */
    std::vector<Segment> segment(const std::u32string& text) const;
};

// ============================================================
// Unicode property queries
// ============================================================

/** Test if codepoint is in a CJK ideograph or related range. */
bool isCJK(char32_t c);

/** Test if a u32string contains any CJK characters. */
bool isCJK(const std::u32string& s);

/** Test if codepoint is a Unicode decimal digit (General_Category = Nd). */
bool isDecimalDigit(char32_t c);

/** Test if codepoint is a combining mark (General_Category = M). */
bool isCombiningMark(char32_t c);

/** Test if codepoint has Emoji_Presentation property. */
bool isEmojiPresentation(char32_t c);

/** Test if codepoint is an Arabic script character. */
bool isArabicScript(char32_t c);

// ============================================================
// UTF conversion helpers
// ============================================================

/** Convert UTF-32 string to UTF-16 (for ICU input). */
std::u16string utf32ToUtf16(const std::u32string& utf32);

/** Convert UTF-16 string to UTF-32. */
std::u32string utf16ToUtf32(const std::u16string& utf16);

/** Set the locale for segmenters. Call before first use. */
void setUnicodeLocale(const char* locale);

} // namespace AgenUIEngine

#endif // AGENUI_UNICODE_UTILS_H
