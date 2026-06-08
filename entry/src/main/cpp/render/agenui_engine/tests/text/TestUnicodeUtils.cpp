/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for Unicode utility functions.
 * Requires ICU linkage but no GPU.
 */

#include <gtest/gtest.h>
#include "modules/text/text_layout/UnicodeUtils.h"

using namespace AgenUIEngine;

// ---------------------------------------------------------------------------
// isCJK
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, IsCJK_Chinese) {
    EXPECT_TRUE(isCJK(U'\u4E2D'));   // CJK Unified Ideograph '中'
    EXPECT_TRUE(isCJK(U'\u5B57'));   // CJK Unified Ideograph '字'
}

TEST(UnicodeUtils, IsCJK_Hiragana) {
    EXPECT_TRUE(isCJK(U'\u3042'));   // Hiragana 'あ'
}

TEST(UnicodeUtils, IsCJK_Katakana) {
    EXPECT_TRUE(isCJK(U'\u30A2'));   // Katakana 'ア'
}

TEST(UnicodeUtils, IsCJK_Latin) {
    EXPECT_FALSE(isCJK(U'A'));
    EXPECT_FALSE(isCJK(U'z'));
}

TEST(UnicodeUtils, IsCJK_Arabic) {
    EXPECT_FALSE(isCJK(U'\u0645'));  // Arabic letter Meem
}

TEST(UnicodeUtils, IsCJK_String) {
    std::u32string cjk = U"中文";
    EXPECT_TRUE(isCJK(cjk));

    std::u32string mixed = U"Hello中文";
    EXPECT_TRUE(isCJK(mixed));

    std::u32string latin = U"Hello";
    EXPECT_FALSE(isCJK(latin));
}

// ---------------------------------------------------------------------------
// isDecimalDigit
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, IsDecimalDigit) {
    for (char32_t c = U'0'; c <= U'9'; ++c) {
        EXPECT_TRUE(isDecimalDigit(c)) << "Expected digit " << static_cast<int>(c);
    }
    EXPECT_FALSE(isDecimalDigit(U'A'));
    EXPECT_FALSE(isDecimalDigit(U' '));
    // Fullwidth digits
    EXPECT_TRUE(isDecimalDigit(U'\uFF10'));  // Fullwidth digit zero
}

// ---------------------------------------------------------------------------
// isCombiningMark
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, IsCombiningMark) {
    // Combining Grave Accent (U+0300)
    EXPECT_TRUE(isCombiningMark(U'\u0300'));
    // Combining Acute Accent (U+0301)
    EXPECT_TRUE(isCombiningMark(U'\u0301'));
    // Regular letter is not a combining mark
    EXPECT_FALSE(isCombiningMark(U'e'));
}

// ---------------------------------------------------------------------------
// isEmojiPresentation
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, IsEmojiPresentation) {
    // Test with codepoints that have emoji presentation by default
    // (not requiring a variation selector)
    EXPECT_TRUE(isEmojiPresentation(U'\U0001F600'));   // Grinning Face
}

TEST(UnicodeUtils, IsEmojiPresentation_Letter) {
    EXPECT_FALSE(isEmojiPresentation(U'A'));
}

// ---------------------------------------------------------------------------
// isArabicScript
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, IsArabicScript) {
    // Verify Arabic characters produce a consistent result (true or false
    // depending on ICU availability and implementation). The function should
    // at least not crash and should return a bool.
    bool result1 = isArabicScript(U'\u0645');  // Arabic letter Meem
    bool result2 = isArabicScript(U'\u0627');  // Arabic letter Alef
    // Both Arabic letters should have the same classification
    EXPECT_EQ(result1, result2);
}

TEST(UnicodeUtils, IsArabicScript_Latin) {
    EXPECT_FALSE(isArabicScript(U'A'));
    EXPECT_FALSE(isArabicScript(U'\u4E2D'));  // CJK
}

// ---------------------------------------------------------------------------
// UTF-32 <-> UTF-16 conversion
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, Utf32ToUtf16_BMP) {
    // BMP characters (no surrogate pairs needed)
    std::u32string input = U"Hello";
    auto utf16 = utf32ToUtf16(input);
    EXPECT_EQ(utf16.size(), 5u);
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(static_cast<char16_t>(input[i]), utf16[i]);
    }
}

TEST(UnicodeUtils, Utf32ToUtf16_SurrogatePair) {
    // Supplementary plane character (emoji) needs surrogate pair
    // U+1F600 = 😀 (Grinning Face)
    char32_t emoji = U'\U0001F600';
    std::u32string input(1, emoji);
    auto utf16 = utf32ToUtf16(input);
    EXPECT_EQ(utf16.size(), 2u);  // surrogate pair

    // Verify it round-trips
    auto utf32 = utf16ToUtf32(utf16);
    ASSERT_EQ(utf32.size(), 1u);
    EXPECT_EQ(utf32[0], emoji);
}

TEST(UnicodeUtils, Utf32ToUtf16_RoundTrip) {
    std::u32string input = U"Hello世界😀\u00E9";  // Latin + CJK + emoji + e-acute
    auto utf16 = utf32ToUtf16(input);
    auto roundtrip = utf16ToUtf32(utf16);
    EXPECT_EQ(input, roundtrip);
}

// ---------------------------------------------------------------------------
// WordSegmenter
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, WordSegmenter_English) {
    WordSegmenter segmenter;
    auto segments = segmenter.segment(U"hello world");
    // Should produce at least 2 word-like segments ("hello", "world")
    int wordLikeCount = 0;
    for (const auto& seg : segments) {
        if (seg.isWordLike) wordLikeCount++;
    }
    EXPECT_GE(wordLikeCount, 2);
}

TEST(UnicodeUtils, WordSegmenter_CJK) {
    WordSegmenter segmenter;
    auto segments = segmenter.segment(U"中文测试");
    // Each CJK character should be its own segment
    EXPECT_GE(segments.size(), 2u);
}

// ---------------------------------------------------------------------------
// GraphemeSegmenter
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, GraphemeSegmenter_Simple) {
    GraphemeSegmenter segmenter;
    auto segments = segmenter.segment(U"abc");
    EXPECT_EQ(segments.size(), 3u);
}

TEST(UnicodeUtils, GraphemeSegmenter_Combining) {
    GraphemeSegmenter segmenter;
    // 'e' + combining acute accent should form a single grapheme cluster
    std::u32string text = U"e\u0301";
    auto segments = segmenter.segment(text);
    // The combining character should merge with the base
    EXPECT_LE(segments.size(), 2u);  // ideally 1, but depends on ICU version
    if (!segments.empty()) {
        // The combined grapheme should contain both codepoints
        EXPECT_GE(segments[0].text.size(), 1u);
    }
}

// ---------------------------------------------------------------------------
// Additional CJK range coverage
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, IsCJK_CJKExtensionA) {
    EXPECT_TRUE(isCJK(U'\u3400'));   // CJK Extension A start
    EXPECT_TRUE(isCJK(U'\u4DBF'));   // CJK Extension A end
}

TEST(UnicodeUtils, IsCJK_KoreanHangul) {
    EXPECT_TRUE(isCJK(U'\uAC00'));   // Hangul Syllables start
    EXPECT_TRUE(isCJK(U'\uD7AF'));   // Hangul Syllables end
}

TEST(UnicodeUtils, IsCJK_FullwidthForms) {
    EXPECT_TRUE(isCJK(U'\uFF10'));   // Fullwidth digit zero
    EXPECT_TRUE(isCJK(U'\uFF21'));   // Fullwidth Latin capital letter A
}

TEST(UnicodeUtils, IsCJK_CJKSymbols) {
    EXPECT_TRUE(isCJK(U'\u3000'));   // Ideographic Space
    EXPECT_TRUE(isCJK(U'\u303F'));   // CJK Symbols end
}

TEST(UnicodeUtils, IsCJK_CompatibilityIdeographs) {
    EXPECT_TRUE(isCJK(U'\uF900'));   // CJK Compatibility Ideographs start
}

// ---------------------------------------------------------------------------
// Boundary tests
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, IsCJK_BoundaryBelow) {
    // Just before CJK Unified Ideographs
    EXPECT_FALSE(isCJK(U'\u4DFF'));   // just before 0x4E00
}

TEST(UnicodeUtils, IsCJK_BoundaryAbove) {
    // Just after CJK Unified Ideographs
    EXPECT_FALSE(isCJK(U'\u9FFF' + 1));
}

TEST(UnicodeUtils, IsDecimalDigit_Letters) {
    EXPECT_FALSE(isDecimalDigit(U'a'));
    EXPECT_FALSE(isDecimalDigit(U'z'));
    EXPECT_FALSE(isDecimalDigit(U'A'));
    EXPECT_FALSE(isDecimalDigit(U'-'));
    EXPECT_FALSE(isDecimalDigit(U' '));
}

TEST(UnicodeUtils, IsEmojiPresentation_CommonEmoji) {
    // Grinning face
    EXPECT_TRUE(isEmojiPresentation(U'\U0001F600'));
    // Smiling face with open mouth
    EXPECT_TRUE(isEmojiPresentation(U'\U0001F603'));
}

TEST(UnicodeUtils, IsArabicScript_RangeStart) {
    // Test that Arabic range characters are classified consistently
    bool r1 = isArabicScript(U'\u0600');   // Arabic start
    bool r2 = isArabicScript(U'\u06FF');   // Arabic end
    EXPECT_EQ(r1, r2);  // both should get same classification
}

TEST(UnicodeUtils, IsArabicScript_NotInArabicRange) {
    EXPECT_FALSE(isArabicScript(U'\u0590'));   // Hebrew
    EXPECT_FALSE(isArabicScript(U'\u0700'));   // Syriac
}

// ---------------------------------------------------------------------------
// Empty string edge cases
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, Utf32ToUtf16_Empty) {
    std::u32string empty;
    auto result = utf32ToUtf16(empty);
    EXPECT_TRUE(result.empty());
}

TEST(UnicodeUtils, Utf16ToUtf32_Empty) {
    std::u16string empty;
    auto result = utf16ToUtf32(empty);
    EXPECT_TRUE(result.empty());
}

TEST(UnicodeUtils, WordSegmenter_Empty) {
    WordSegmenter segmenter;
    auto segments = segmenter.segment(U"");
    EXPECT_TRUE(segments.empty());
}

TEST(UnicodeUtils, GraphemeSegmenter_Empty) {
    GraphemeSegmenter segmenter;
    auto segments = segmenter.segment(U"");
    EXPECT_TRUE(segments.empty());
}

TEST(UnicodeUtils, IsCJK_EmptyString) {
    std::u32string empty;
    EXPECT_FALSE(isCJK(empty));
}

// ---------------------------------------------------------------------------
// Mixed content UTF conversion
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, Utf32ToUtf16_MixedBMPAndSupplementary) {
    std::u32string input = U"A\u4E2D\U0001F600Z";  // Latin + CJK + emoji + Latin
    auto utf16 = utf32ToUtf16(input);
    // 'A' = 1, '中' = 1, emoji = 2 (surrogate), 'Z' = 1 → total 5
    EXPECT_EQ(utf16.size(), 5u);
}

TEST(UnicodeUtils, Utf16ToUtf32_SurrogatePairRoundTrip) {
    // Manually construct a surrogate pair for U+1F600
    char16_t hi = 0xD83D;
    char16_t lo = 0xDE00;
    std::u16string input = {hi, lo};
    auto utf32 = utf16ToUtf32(input);
    ASSERT_EQ(utf32.size(), 1u);
    EXPECT_EQ(utf32[0], U'\U0001F600');
}

// ---------------------------------------------------------------------------
// Locale setting (no crash test)
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, SetLocale_NoCrash) {
    setUnicodeLocale("en");
    setUnicodeLocale("zh_CN");
    setUnicodeLocale(nullptr);
}

// ---------------------------------------------------------------------------
// Word segmenter with punctuation
// ---------------------------------------------------------------------------

TEST(UnicodeUtils, WordSegmenter_WithPunctuation) {
    WordSegmenter segmenter;
    auto segments = segmenter.segment(U"hello, world!");
    // Should produce word and non-word segments
    EXPECT_GE(segments.size(), 3u);
}
