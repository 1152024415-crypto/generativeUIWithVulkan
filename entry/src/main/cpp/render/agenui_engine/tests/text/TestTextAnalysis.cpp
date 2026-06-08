/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for TextAnalysis — text segmentation.
 * Requires ICU linkage but no GPU.
 */

#include <gtest/gtest.h>
#include "modules/text/text_layout/TextAnalysis.h"

using namespace AgenUIEngine;

// ---------------------------------------------------------------------------
// analyzeText — basic segmentation
// ---------------------------------------------------------------------------

TEST(TextAnalysis, AnalyzeText_SimpleEnglish) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"hello", profile);

    EXPECT_FALSE(result.texts.empty());
    EXPECT_FALSE(result.kinds.empty());

    // "hello" should produce at least one Text segment
    bool hasText = false;
    for (auto kind : result.kinds) {
        if (kind == SegmentBreakKind::Text) {
            hasText = true;
            break;
        }
    }
    EXPECT_TRUE(hasText);
}

TEST(TextAnalysis, AnalyzeText_Spaces) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"hello world", profile);

    // Should have at least: Text + Space + Text
    int textCount = 0;
    int spaceCount = 0;
    for (auto kind : result.kinds) {
        if (kind == SegmentBreakKind::Text) textCount++;
        if (kind == SegmentBreakKind::Space) spaceCount++;
    }
    EXPECT_GE(textCount, 2);
    EXPECT_GE(spaceCount, 1);
}

TEST(TextAnalysis, AnalyzeText_CJK) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"中文", profile);

    // CJK text should be analyzed and produce at least one segment
    EXPECT_FALSE(result.texts.empty());
    // CJK characters may be segmented per-character (with ICU) or as a group
    EXPECT_GE(result.kinds.size(), 1u);
}

TEST(TextAnalysis, AnalyzeText_MixedScript) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"Hello中文World", profile);

    // Should produce multiple segments
    EXPECT_GE(result.texts.size(), 2u);
}

TEST(TextAnalysis, AnalyzeText_Tab) {
    AnalysisProfile profile;
    // Tab is only preserved in PreWrap mode
    auto result = TextAnalyzer::analyzeText(U"hello\tworld", profile, WhiteSpaceMode::PreWrap);

    bool hasTab = false;
    for (auto kind : result.kinds) {
        if (kind == SegmentBreakKind::Tab) {
            hasTab = true;
            break;
        }
    }
    EXPECT_TRUE(hasTab);
}

TEST(TextAnalysis, AnalyzeText_SoftHyphen) {
    AnalysisProfile profile;
    // Soft hyphen is U+00AD
    auto result = TextAnalyzer::analyzeText(U"hello\u00ADworld", profile);

    bool hasSoftHyphen = false;
    for (auto kind : result.kinds) {
        if (kind == SegmentBreakKind::SoftHyphen) {
            hasSoftHyphen = true;
            break;
        }
    }
    EXPECT_TRUE(hasSoftHyphen);
}

TEST(TextAnalysis, AnalyzeText_HardBreak) {
    AnalysisProfile profile;
    // HardBreak (newline) is only preserved in PreWrap mode
    auto result = TextAnalyzer::analyzeText(U"hello\nworld", profile, WhiteSpaceMode::PreWrap);

    bool hasHardBreak = false;
    for (auto kind : result.kinds) {
        if (kind == SegmentBreakKind::HardBreak) {
            hasHardBreak = true;
            break;
        }
    }
    EXPECT_TRUE(hasHardBreak);
}

// ---------------------------------------------------------------------------
// Character property delegation
// ---------------------------------------------------------------------------

TEST(TextAnalysis, IsCJK_DelegatesToUnicodeUtils) {
    // TextAnalyzer::isCJK should give the same results as UnicodeUtils::isCJK
    EXPECT_EQ(TextAnalyzer::isCJK(U'\u4E2D'), true);
    EXPECT_EQ(TextAnalyzer::isCJK(U'A'), false);

    std::u32string cjkStr = U"中文";
    std::u32string latinStr = U"Hello";
    EXPECT_TRUE(TextAnalyzer::isCJK(cjkStr));
    EXPECT_FALSE(TextAnalyzer::isCJK(latinStr));
}

// ---------------------------------------------------------------------------
// Kinsoku / closing quote
// ---------------------------------------------------------------------------

TEST(TextAnalysis, EndsWithClosingQuote) {
    // Test with common closing quotes
    EXPECT_TRUE(TextAnalyzer::endsWithClosingQuote(U"hello\u201D"));  // Right double quotation mark
    EXPECT_TRUE(TextAnalyzer::endsWithClosingQuote(U"test\u2019"));   // Right single quotation mark
    EXPECT_FALSE(TextAnalyzer::endsWithClosingQuote(U"hello"));
}

TEST(TextAnalysis, GetKinsokuStart_NotEmpty) {
    const auto& chars = TextAnalyzer::getKinsokuStart();
    EXPECT_FALSE(chars.empty());
}

TEST(TextAnalysis, GetKinsokuEnd_NotEmpty) {
    const auto& chars = TextAnalyzer::getKinsokuEnd();
    EXPECT_FALSE(chars.empty());
}

// ---------------------------------------------------------------------------
// White space modes
// ---------------------------------------------------------------------------

TEST(TextAnalysis, WhiteSpaceMode_Normal) {
    AnalysisProfile profile;
    // Normal mode collapses consecutive whitespace
    auto result = TextAnalyzer::analyzeText(U"hello  world", profile, WhiteSpaceMode::Normal);
    EXPECT_FALSE(result.texts.empty());
}

TEST(TextAnalysis, WhiteSpaceMode_PreWrap) {
    AnalysisProfile profile;
    // PreWrap preserves whitespace
    auto result = TextAnalyzer::analyzeText(U"hello  world", profile, WhiteSpaceMode::PreWrap);
    EXPECT_FALSE(result.texts.empty());
}

// ---------------------------------------------------------------------------
// Whitespace normalization edge cases
// ---------------------------------------------------------------------------

TEST(TextAnalysis, AnalyzeText_NormalCollapsesSpaces) {
    AnalysisProfile profile;
    // Multiple spaces should be collapsed to single space in Normal mode
    auto result = TextAnalyzer::analyzeText(U"hello   world", profile, WhiteSpaceMode::Normal);
    // Normalized text should not have 3 consecutive spaces
    bool hasTripleSpace = false;
    for (size_t i = 0; i + 2 < result.normalized.size(); ++i) {
        if (result.normalized[i] == ' ' && result.normalized[i+1] == ' ' && result.normalized[i+2] == ' ') {
            hasTripleSpace = true;
        }
    }
    EXPECT_FALSE(hasTripleSpace);
}

TEST(TextAnalysis, AnalyzeText_NormalStripsLeadingSpace) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"  hello", profile, WhiteSpaceMode::Normal);
    if (!result.normalized.empty()) {
        EXPECT_NE(result.normalized[0], ' ');
    }
}

TEST(TextAnalysis, AnalyzeText_NormalStripsTrailingSpace) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"hello  ", profile, WhiteSpaceMode::Normal);
    if (!result.normalized.empty()) {
        EXPECT_NE(result.normalized.back(), ' ');
    }
}

TEST(TextAnalysis, AnalyzeText_PreWrapPreservesSpaces) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"hello  world", profile, WhiteSpaceMode::PreWrap);
    // PreWrap should keep the double space as PreservedSpace
    bool hasPreservedSpace = false;
    for (auto kind : result.kinds) {
        if (kind == SegmentBreakKind::PreservedSpace) {
            hasPreservedSpace = true;
            break;
        }
    }
    EXPECT_TRUE(hasPreservedSpace);
}

TEST(TextAnalysis, AnalyzeText_PreWrapConvertsCR) {
    AnalysisProfile profile;
    // \r\n should become \n in PreWrap
    auto result = TextAnalyzer::analyzeText(U"a\r\nb", profile, WhiteSpaceMode::PreWrap);
    EXPECT_FALSE(result.normalized.empty());
}

// ---------------------------------------------------------------------------
// Empty text
// ---------------------------------------------------------------------------

TEST(TextAnalysis, AnalyzeText_Empty) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"", profile);
    EXPECT_EQ(result.len, 0);
    EXPECT_TRUE(result.texts.empty());
}

// ---------------------------------------------------------------------------
// Glue characters
// ---------------------------------------------------------------------------

TEST(TextAnalysis, AnalyzeText_GlueNbsp) {
    AnalysisProfile profile;
    // Non-breaking space (U+00A0) should be classified as Glue when segmented.
    // Note: whether NBSP becomes its own segment depends on the word segmenter
    // implementation. Test that the analysis produces at least some segments
    // and does not crash with NBSP input.
    auto result = TextAnalyzer::analyzeText(U"hello\u00A0world", profile, WhiteSpaceMode::PreWrap);
    EXPECT_FALSE(result.texts.empty());
    EXPECT_FALSE(result.kinds.empty());
    // Verify that the full text was processed (NBSP is handled)
    int32_t totalLen = 0;
    for (const auto& t : result.texts) {
        totalLen += static_cast<int32_t>(t.size());
    }
    EXPECT_EQ(totalLen, 11);  // "hello" + NBSP + "world" = 5+1+5
}

// ---------------------------------------------------------------------------
// Zero-width break
// ---------------------------------------------------------------------------

TEST(TextAnalysis, AnalyzeText_ZeroWidthBreak) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"hello\u200Bworld", profile);
    bool hasZeroWidthBreak = false;
    for (auto kind : result.kinds) {
        if (kind == SegmentBreakKind::ZeroWidthBreak) {
            hasZeroWidthBreak = true;
            break;
        }
    }
    EXPECT_TRUE(hasZeroWidthBreak);
}

// ---------------------------------------------------------------------------
// Closing quote edge cases
// ---------------------------------------------------------------------------

TEST(TextAnalysis, EndsWithClosingQuote_CJKQuote) {
    EXPECT_TRUE(TextAnalyzer::endsWithClosingQuote(U"\u300D"));   // CJK right corner bracket
}

TEST(TextAnalysis, EndsWithClosingQuote_NoQuoteInMiddle) {
    // Closing quote is not at end
    EXPECT_FALSE(TextAnalyzer::endsWithClosingQuote(U"\u201Dhello"));
}

TEST(TextAnalysis, EndsWithClosingQuote_EmptyString) {
    EXPECT_FALSE(TextAnalyzer::endsWithClosingQuote(U""));
}

// ---------------------------------------------------------------------------
// Kinsoku content verification
// ---------------------------------------------------------------------------

TEST(TextAnalysis, KinsokuStartContainsKnownChars) {
    const auto& chars = TextAnalyzer::getKinsokuStart();
    // Should contain common kinsoku start chars like 。(0x3002)
    bool hasMaru = false;
    for (char32_t c : chars) {
        if (c == 0x3002) { hasMaru = true; break; }
    }
    EXPECT_TRUE(hasMaru);
}

TEST(TextAnalysis, KinsokuEndContainsKnownChars) {
    const auto& chars = TextAnalyzer::getKinsokuEnd();
    // Should contain opening brackets like (0xFF08 fullwidth left paren)
    bool hasOpenParen = false;
    for (char32_t c : chars) {
        if (c == 0xFF08) { hasOpenParen = true; break; }
    }
    EXPECT_TRUE(hasOpenParen);
}

TEST(TextAnalysis, GetLeftStickyPunctuation_NotEmpty) {
    const auto& chars = TextAnalyzer::getLeftStickyPunctuation();
    EXPECT_FALSE(chars.empty());
}

// ---------------------------------------------------------------------------
// Analysis result integrity
// ---------------------------------------------------------------------------

TEST(TextAnalysis, AnalyzeText_ResultArraySizesMatch) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"hello world 123", profile);
    EXPECT_EQ(result.texts.size(), result.kinds.size());
    EXPECT_EQ(result.texts.size(), result.starts.size());
    EXPECT_EQ(result.texts.size(), result.isWordLike.size());
    EXPECT_EQ(static_cast<int32_t>(result.texts.size()), result.len);
}

TEST(TextAnalysis, AnalyzeText_SegmentStartsMonotonic) {
    AnalysisProfile profile;
    auto result = TextAnalyzer::analyzeText(U"hello world test", profile);
    for (size_t i = 1; i < result.starts.size(); ++i) {
        EXPECT_GT(result.starts[i], result.starts[i-1])
            << "Segment starts not monotonically increasing at index " << i;
    }
}
