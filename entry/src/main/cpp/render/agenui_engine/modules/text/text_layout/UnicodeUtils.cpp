#include "UnicodeUtils.h"

#if __has_include(<unicode/ubrk.h>)
#define AGENUI_HAS_ICU 1
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>
#else
#define AGENUI_HAS_ICU 0
#endif

#include <cstring>
#include <mutex>

namespace AgenUIEngine {

// ============================================================
// Locale management
// ============================================================

static const char* s_locale = "";
static std::mutex s_localeMutex;

void setUnicodeLocale(const char* locale) {
    std::lock_guard<std::mutex> lock(s_localeMutex);
    s_locale = locale ? locale : "";
}

static const char* getLocale() {
    std::lock_guard<std::mutex> lock(s_localeMutex);
    return s_locale;
}

// ============================================================
// UTF conversion
// ============================================================

std::u16string utf32ToUtf16(const std::u32string& utf32) {
#if AGENUI_HAS_ICU
    if (utf32.empty()) return {};

    const int32_t srcLen = static_cast<int32_t>(utf32.size());
    const int32_t destCapacity = srcLen * 2;

    UChar* destBuf = static_cast<UChar*>(malloc(destCapacity * sizeof(UChar)));
    if (!destBuf) return {};

    int32_t* srcBuf = static_cast<int32_t*>(malloc(srcLen * sizeof(int32_t)));
    if (!srcBuf) { free(destBuf); return {}; }
    if (srcLen > 0) {
        std::memcpy(srcBuf, utf32.data(), srcLen * sizeof(char32_t));
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t destLen = 0;

    u_strFromUTF32(destBuf, destCapacity, &destLen, srcBuf, srcLen, &status);

    std::u16string result;
    if (!U_FAILURE(status) || status == U_BUFFER_OVERFLOW_ERROR) {
        result.resize(static_cast<size_t>(destLen));
        if (destLen > 0) {
            std::memcpy(&result[0], destBuf, static_cast<size_t>(destLen) * sizeof(UChar));
        }
    }

    free(destBuf);
    free(srcBuf);
    return result;
#else
    // Fallback: manual UTF-32 to UTF-16 conversion (BMP only)
    std::u16string result;
    result.reserve(utf32.size());
    for (char32_t c : utf32) {
        if (c <= 0xFFFF) {
            result.push_back(static_cast<char16_t>(c));
        } else {
            // Surrogate pair
            c -= 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 + (c >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 + (c & 0x3FF)));
        }
    }
    return result;
#endif
}

std::u32string utf16ToUtf32(const std::u16string& utf16) {
#if AGENUI_HAS_ICU
    if (utf16.empty()) return {};

    const int32_t srcLen = static_cast<int32_t>(utf16.size());
    const int32_t destCapacity = srcLen;

    int32_t* destBuf = static_cast<int32_t*>(malloc(destCapacity * sizeof(int32_t)));
    if (!destBuf) return {};

    UChar* srcBuf = static_cast<UChar*>(malloc(srcLen * sizeof(UChar)));
    if (!srcBuf) { free(destBuf); return {}; }
    if (srcLen > 0) {
        std::memcpy(srcBuf, utf16.data(), srcLen * sizeof(char16_t));
    }

    UErrorCode status = U_ZERO_ERROR;
    int32_t destLen = 0;

    u_strToUTF32(destBuf, destCapacity, &destLen, srcBuf, srcLen, &status);

    std::u32string result;
    if (!U_FAILURE(status) || status == U_BUFFER_OVERFLOW_ERROR) {
        result.resize(static_cast<size_t>(destLen));
        if (destLen > 0) {
            std::memcpy(&result[0], destBuf, static_cast<size_t>(destLen) * sizeof(int32_t));
        }
    }

    free(destBuf);
    free(srcBuf);
    return result;
#else
    // Fallback: manual UTF-16 to UTF-32 conversion
    std::u32string result;
    result.reserve(utf16.size());
    for (size_t i = 0; i < utf16.size(); ++i) {
        char16_t c = utf16[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < utf16.size()) {
            char16_t lo = utf16[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                char32_t cp = ((static_cast<char32_t>(c) - 0xD800) << 10)
                            + (static_cast<char32_t>(lo) - 0xDC00) + 0x10000;
                result.push_back(cp);
                ++i;
                continue;
            }
        }
        result.push_back(static_cast<char32_t>(c));
    }
    return result;
#endif
}

// ============================================================
// Unicode property queries
// ============================================================

bool isCJK(char32_t c) {
    return (
        (c >= 0x4E00 && c <= 0x9FFF) ||   // CJK Unified Ideographs
        (c >= 0x3400 && c <= 0x4DBF) ||   // CJK Extension A
        (c >= 0x20000 && c <= 0x2A6DF) || // CJK Extension B
        (c >= 0x2A700 && c <= 0x2B73F) || // CJK Extension C
        (c >= 0x2B740 && c <= 0x2B81F) || // CJK Extension D
        (c >= 0x2B820 && c <= 0x2CEAF) || // CJK Extension E
        (c >= 0x2CEB0 && c <= 0x2EBEF) || // CJK Extension F
        (c >= 0x30000 && c <= 0x3134F) || // CJK Extension G
        (c >= 0xF900 && c <= 0xFAFF) ||   // CJK Compatibility Ideographs
        (c >= 0x2F800 && c <= 0x2FA1F) || // CJK Compatibility Ideographs Supplement
        (c >= 0x3000 && c <= 0x303F) ||   // CJK Symbols and Punctuation
        (c >= 0x3040 && c <= 0x309F) ||   // Hiragana
        (c >= 0x30A0 && c <= 0x30FF) ||   // Katakana
        (c >= 0xAC00 && c <= 0xD7AF) ||   // Hangul Syllables
        (c >= 0xFF00 && c <= 0xFFEF)      // Halfwidth and Fullwidth Forms
    );
}

bool isCJK(const std::u32string& s) {
    for (char32_t c : s) {
        if (isCJK(c)) return true;
    }
    return false;
}

bool isDecimalDigit(char32_t c) {
#if AGENUI_HAS_ICU
    return u_charType(c) == U_DECIMAL_DIGIT_NUMBER;
#else
    return c >= '0' && c <= '9';
#endif
}

bool isCombiningMark(char32_t c) {
#if AGENUI_HAS_ICU
    int8_t gc = u_charType(c);
    return gc == U_NON_SPACING_MARK || gc == U_COMBINING_SPACING_MARK || gc == U_ENCLOSING_MARK;
#else
    // Basic combining mark ranges
    return (c >= 0x0300 && c <= 0x036F) ||   // Combining Diacritical Marks
           (c >= 0x1AB0 && c <= 0x1AFF) ||   // Combining Diacritical Marks Extended
           (c >= 0x1DC0 && c <= 0x1DFF) ||   // Combining Diacritical Marks Supplement
           (c >= 0x20D0 && c <= 0x20FF) ||   // Combining Diacritical Marks for Symbols
           (c >= 0xFE20 && c <= 0xFE2F);     // Combining Half Marks
#endif
}

bool isEmojiPresentation(char32_t c) {
#if AGENUI_HAS_ICU
    return u_hasBinaryProperty(c, UCHAR_EMOJI_PRESENTATION);
#else
    // Basic emoji ranges (incomplete but covers common ones)
    return (c >= 0x1F600 && c <= 0x1F64F) ||  // Emoticons
           (c >= 0x1F300 && c <= 0x1F5FF) ||  // Misc Symbols and Pictographs
           (c >= 0x1F680 && c <= 0x1F6FF) ||  // Transport and Map
           (c >= 0x1F900 && c <= 0x1F9FF) ||  // Supplemental Symbols and Pictographs
           (c >= 0x2600 && c <= 0x26FF) ||    // Misc Symbols
           (c >= 0x2700 && c <= 0x27BF);      // Dingbats
#endif
}

bool isArabicScript(char32_t c) {
#if AGENUI_HAS_ICU
    return u_getIntPropertyValue(c, UCHAR_SCRIPT) == 1;  // 1 = USCRIPT_ARABIC
#else
    return (c >= 0x0600 && c <= 0x06FF) ||   // Arabic
           (c >= 0x0750 && c <= 0x077F) ||   // Arabic Supplement
           (c >= 0x08A0 && c <= 0x08FF) ||   // Arabic Extended-A
           (c >= 0xFB50 && c <= 0xFDFF) ||   // Arabic Presentation Forms-A
           (c >= 0xFE70 && c <= 0xFEFF);     // Arabic Presentation Forms-B
#endif
}

// ============================================================
// WordSegmenter
// ============================================================

WordSegmenter::WordSegmenter() = default;
WordSegmenter::~WordSegmenter() = default;

std::vector<WordSegmenter::Segment> WordSegmenter::segment(const std::u32string& text) const {
    std::vector<Segment> result;
    if (text.empty()) return result;

#if AGENUI_HAS_ICU
    std::u16string utf16 = utf32ToUtf16(text);
    if (utf16.empty()) return result;

    const int32_t textLen = static_cast<int32_t>(utf16.size());
    UErrorCode status = U_ZERO_ERROR;

    UChar* ucharBuf = static_cast<UChar*>(malloc(textLen * sizeof(UChar)));
    if (!ucharBuf) return result;
    if (textLen > 0) {
        std::memcpy(ucharBuf, utf16.data(), textLen * sizeof(char16_t));
    }

    UBreakIterator* bi = ubrk_open(UBRK_WORD, getLocale(), ucharBuf, textLen, &status);

    if (U_FAILURE(status) || !bi) {
        free(ucharBuf);
        return result;
    }

    // Build UTF-16 to UTF-32 index map
    std::vector<int32_t> utf16ToUtf32Map(textLen + 1);
    int32_t utf32Pos = 0;
    for (int32_t i = 0; i < textLen; ) {
        utf16ToUtf32Map[i] = utf32Pos;
        char16_t ch = utf16[i];
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < textLen) {
            i += 2;
        } else {
            i += 1;
        }
        utf32Pos++;
    }
    utf16ToUtf32Map[textLen] = utf32Pos;

    int32_t prev = 0;
    int32_t curr = ubrk_first(bi);

    while (curr != UBRK_DONE) {
        if (curr > prev) {
            std::u16string segUtf16(utf16.substr(prev, curr - prev));
            std::u32string segUtf32 = utf16ToUtf32(segUtf16);

            int32_t rule = ubrk_getRuleStatus(bi);
            bool wordLike = (rule != UBRK_WORD_NONE);

            result.push_back({
                std::move(segUtf32),
                wordLike,
                utf16ToUtf32Map[prev]
            });
        }
        prev = curr;
        curr = ubrk_next(bi);
    }

    ubrk_close(bi);
    free(ucharBuf);
#else
    // Fallback: split on whitespace and CJK boundaries
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t c = text[i];
        bool isBoundary = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (isBoundary && i > start) {
            result.push_back({text.substr(start, i - start), true, static_cast<int32_t>(start)});
            start = i + 1;
        } else if (isCJK(c)) {
            if (i > start) {
                result.push_back({text.substr(start, i - start), true, static_cast<int32_t>(start)});
            }
            result.push_back({text.substr(i, 1), true, static_cast<int32_t>(i)});
            start = i + 1;
        }
    }
    if (start < text.size()) {
        result.push_back({text.substr(start), true, static_cast<int32_t>(start)});
    }
#endif
    return result;
}

// ============================================================
// GraphemeSegmenter
// ============================================================

GraphemeSegmenter::GraphemeSegmenter() = default;
GraphemeSegmenter::~GraphemeSegmenter() = default;

std::vector<GraphemeSegmenter::Segment> GraphemeSegmenter::segment(const std::u32string& text) const {
    std::vector<Segment> result;
    if (text.empty()) return result;

#if AGENUI_HAS_ICU
    std::u16string utf16 = utf32ToUtf16(text);
    if (utf16.empty()) return result;

    const int32_t textLen = static_cast<int32_t>(utf16.size());
    UErrorCode status = U_ZERO_ERROR;

    UChar* ucharBuf = static_cast<UChar*>(malloc(textLen * sizeof(UChar)));
    if (!ucharBuf) {
        for (size_t i = 0; i < text.size(); ++i) {
            result.push_back({ text.substr(i, 1), static_cast<int32_t>(i) });
        }
        return result;
    }
    if (textLen > 0) {
        std::memcpy(ucharBuf, utf16.data(), textLen * sizeof(char16_t));
    }

    UBreakIterator* bi = ubrk_open(UBRK_CHARACTER, getLocale(), ucharBuf, textLen, &status);

    if (U_FAILURE(status) || !bi) {
        for (size_t i = 0; i < text.size(); ++i) {
            result.push_back({ text.substr(i, 1), static_cast<int32_t>(i) });
        }
        free(ucharBuf);
        return result;
    }

    // Build UTF-16 to UTF-32 index map
    std::vector<int32_t> idxMap(textLen + 1);
    int32_t utf32Pos = 0;
    for (int32_t i = 0; i < textLen; ) {
        idxMap[i] = utf32Pos;
        char16_t ch = utf16[i];
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < textLen) {
            i += 2;
        } else {
            i += 1;
        }
        utf32Pos++;
    }
    idxMap[textLen] = utf32Pos;

    int32_t prev = 0;
    int32_t curr = ubrk_first(bi);

    while (curr != UBRK_DONE) {
        if (curr > prev) {
            std::u16string segUtf16(utf16.substr(prev, curr - prev));
            std::u32string segUtf32 = utf16ToUtf32(segUtf16);

            result.push_back({
                std::move(segUtf32),
                idxMap[prev]
            });
        }
        prev = curr;
        curr = ubrk_next(bi);
    }

    ubrk_close(bi);
    free(ucharBuf);
#else
    // Fallback: each code point is a grapheme cluster
    for (size_t i = 0; i < text.size(); ++i) {
        result.push_back({ text.substr(i, 1), static_cast<int32_t>(i) });
    }
#endif
    return result;
}

} // namespace AgenUIEngine
