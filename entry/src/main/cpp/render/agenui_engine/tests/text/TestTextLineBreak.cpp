/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for TextLineBreaker — line breaking algorithm.
 * Constructs PreparedLineBreakData directly; no GPU required.
 */

#include <gtest/gtest.h>
#include "modules/text/text_layout/TextLineBreak.h"

using namespace AgenUIEngine;

namespace {

// Helper to build a simple prepared data with uniform segment widths
PreparedLineBreakData makeSimplePrepared(
    const std::vector<float>& widths,
    const std::vector<SegmentBreakKind>& kinds,
    bool simpleFastPath = true)
{
    PreparedLineBreakData data;
    data.widths = widths;
    data.kinds = kinds;
    data.simpleLineWalkFastPath = simpleFastPath;

    // Build trivial chunks: one chunk per segment.
    // consumedEndSegmentIndex is exclusive (one past the last segment).
    data.chunks.resize(widths.size());
    for (size_t i = 0; i < widths.size(); ++i) {
        data.chunks[i].startSegmentIndex = static_cast<int32_t>(i);
        data.chunks[i].endSegmentIndex = static_cast<int32_t>(i + 1);
        data.chunks[i].consumedEndSegmentIndex = static_cast<int32_t>(i + 1);
    }

    // Empty breakableWidths (no breakable segments)
    data.breakableWidths.resize(widths.size());
    data.breakablePrefixWidths.resize(widths.size());

    return data;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// countPreparedLines
// ---------------------------------------------------------------------------

TEST(TextLineBreak, CountLines_SingleLine) {
    // 3 segments, each 50px wide, maxWidth = 200 → fits on one line
    auto data = makeSimplePrepared(
        {50.0f, 50.0f, 50.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text, SegmentBreakKind::Text}
    );

    int32_t lines = TextLineBreaker::countPreparedLines(data, 200.0f);
    EXPECT_EQ(lines, 1);
}

TEST(TextLineBreak, CountLines_TwoLines) {
    // 4 segments, each 100px wide, maxWidth = 200 → 2 lines
    auto data = makeSimplePrepared(
        {100.0f, 100.0f, 100.0f, 100.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text,
         SegmentBreakKind::Text, SegmentBreakKind::Text}
    );

    int32_t lines = TextLineBreaker::countPreparedLines(data, 200.0f);
    EXPECT_EQ(lines, 2);
}

TEST(TextLineBreak, CountLines_EmptyText) {
    auto data = makeSimplePrepared({}, {});
    int32_t lines = TextLineBreaker::countPreparedLines(data, 200.0f);
    // Empty text → 0 lines
    EXPECT_EQ(lines, 0);
}

// ---------------------------------------------------------------------------
// walkPreparedLines
// ---------------------------------------------------------------------------

TEST(TextLineBreak, WalkPreparedLines_CallbackCount) {
    auto data = makeSimplePrepared(
        {100.0f, 100.0f, 100.0f, 100.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text,
         SegmentBreakKind::Text, SegmentBreakKind::Text}
    );

    int callbackCount = 0;
    int32_t lines = TextLineBreaker::walkPreparedLines(data, 200.0f,
        [&](const InternalLayoutLine& line) {
            callbackCount++;
        });

    EXPECT_EQ(callbackCount, 2);
    EXPECT_EQ(lines, 2);
}

TEST(TextLineBreak, WalkPreparedLines_LineWidths) {
    auto data = makeSimplePrepared(
        {100.0f, 100.0f, 100.0f, 100.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text,
         SegmentBreakKind::Text, SegmentBreakKind::Text}
    );

    TextLineBreaker::walkPreparedLines(data, 200.0f,
        [&](const InternalLayoutLine& line) {
            EXPECT_LE(line.width, 200.0f);
        });
}

// ---------------------------------------------------------------------------
// layoutNextLineRange
// ---------------------------------------------------------------------------

TEST(TextLineBreak, LayoutNextLineRange_SingleLine) {
    auto data = makeSimplePrepared(
        {50.0f, 50.0f, 50.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text, SegmentBreakKind::Text}
    );

    LineBreakCursor start;
    InternalLayoutLine line;
    bool hasMore = TextLineBreaker::layoutNextLineRange(data, start, 200.0f, line);

    EXPECT_TRUE(hasMore);
    // Line should cover all 3 segments; endSegmentIndex is exclusive (past last)
    EXPECT_EQ(line.endSegmentIndex, 3);
}

TEST(TextLineBreak, LayoutNextLineRange_MultiLine) {
    auto data = makeSimplePrepared(
        {100.0f, 100.0f, 100.0f, 100.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text,
         SegmentBreakKind::Text, SegmentBreakKind::Text}
    );

    LineBreakCursor cursor;
    InternalLayoutLine line1;
    bool hasMore = TextLineBreaker::layoutNextLineRange(data, cursor, 200.0f, line1);
    EXPECT_TRUE(hasMore);

    // First line should cover first 2 segments
    EXPECT_GE(line1.endSegmentIndex, 1);

    // Advance cursor for next line
    cursor.segmentIndex = line1.endSegmentIndex + 1;
    cursor.graphemeIndex = 0;

    // Normalize the cursor
    TextLineBreaker::normalizeLineStart(data, cursor);

    InternalLayoutLine line2;
    hasMore = TextLineBreaker::layoutNextLineRange(data, cursor, 200.0f, line2);
    EXPECT_TRUE(hasMore);
}

// ---------------------------------------------------------------------------
// normalizeLineStart
// ---------------------------------------------------------------------------

TEST(TextLineBreak, NormalizeLineStart_AtBeginning) {
    auto data = makeSimplePrepared(
        {50.0f, 50.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text}
    );

    LineBreakCursor cursor;
    bool hasMore = TextLineBreaker::normalizeLineStart(data, cursor);

    // At beginning of data, should return true
    EXPECT_TRUE(hasMore);
    EXPECT_EQ(cursor.segmentIndex, 0);
}

// ---------------------------------------------------------------------------
// SimpleFastPath
// ---------------------------------------------------------------------------

TEST(TextLineBreak, SimpleFastPath) {
    auto data = makeSimplePrepared(
        {100.0f, 100.0f},
        {SegmentBreakKind::Text, SegmentBreakKind::Text},
        true  // simpleLineWalkFastPath = true
    );

    int32_t lines = TextLineBreaker::countPreparedLines(data, 200.0f);
    EXPECT_EQ(lines, 1);
}

// ---------------------------------------------------------------------------
// Breakable widths
// ---------------------------------------------------------------------------

TEST(TextLineBreak, BreakableWidths) {
    // Create data with a space (breakable) in the middle
    PreparedLineBreakData data;
    data.widths = {100.0f, 10.0f, 100.0f};
    data.kinds = {SegmentBreakKind::Text, SegmentBreakKind::Space, SegmentBreakKind::Text};
    data.simpleLineWalkFastPath = false;

    // Mark the space as breakable
    data.breakableWidths.resize(3);
    data.breakablePrefixWidths.resize(3);
    // Segment 1 (space) has a breakable width
    data.breakableWidths[1] = {10.0f};
    data.breakablePrefixWidths[1] = {100.0f};

    // Single chunk covering all 3 segments
    data.chunks.resize(1);
    data.chunks[0] = {0, 3, 3};

    data.lineEndFitAdvances = {100.0f, 10.0f, 100.0f};
    data.lineEndPaintAdvances = {100.0f, 10.0f, 100.0f};

    // With maxWidth=150, should break at the space → 2 lines
    int32_t lines = TextLineBreaker::countPreparedLines(data, 150.0f);
    EXPECT_EQ(lines, 2);
}

// ---------------------------------------------------------------------------
// Pending break (break after space)
// ---------------------------------------------------------------------------

TEST(TextLineBreak, PendingBreakAtSpace) {
    // "word space word" — when line overflows, break at space
    PreparedLineBreakData data;
    data.widths = {80.0f, 10.0f, 80.0f};
    data.kinds = {SegmentBreakKind::Text, SegmentBreakKind::Space, SegmentBreakKind::Text};
    data.simpleLineWalkFastPath = true;

    data.breakableWidths.resize(3);
    data.breakablePrefixWidths.resize(3);

    data.chunks.resize(3);
    for (int i = 0; i < 3; ++i) {
        data.chunks[i].startSegmentIndex = i;
        data.chunks[i].endSegmentIndex = i + 1;
        data.chunks[i].consumedEndSegmentIndex = i + 1;
    }

    // maxWidth=100: first word fits (80), +space(10)=90 fits, +word(80)=170 > 100
    // Should break at space boundary → 2 lines
    int32_t lines = TextLineBreaker::countPreparedLines(data, 100.0f);
    EXPECT_EQ(lines, 2);
}

// ---------------------------------------------------------------------------
// Breakable segments (word wider than maxWidth)
// ---------------------------------------------------------------------------

TEST(TextLineBreak, BreakableSegmentWiderThanMaxWidth) {
    // Single segment wider than maxWidth, with breakable graphemes
    PreparedLineBreakData data;
    data.widths = {300.0f};
    data.kinds = {SegmentBreakKind::Text};
    data.simpleLineWalkFastPath = false;

    // Breakable grapheme widths: 3 graphemes of 100px each
    data.breakableWidths.resize(1);
    data.breakablePrefixWidths.resize(1);
    data.breakableWidths[0] = {100.0f, 100.0f, 100.0f};
    data.breakablePrefixWidths[0] = {100.0f, 200.0f, 300.0f};

    data.chunks.resize(1);
    data.chunks[0] = {0, 1, 1};

    data.lineEndFitAdvances = {300.0f};
    data.lineEndPaintAdvances = {300.0f};

    // maxWidth=200: should break into 2 lines (100+100=200, then 100)
    int32_t lines = TextLineBreaker::countPreparedLines(data, 200.0f);
    EXPECT_EQ(lines, 2);
}

// ---------------------------------------------------------------------------
// Many lines
// ---------------------------------------------------------------------------

TEST(TextLineBreak, CountLines_ManyLines) {
    // 10 segments, each 50px, maxWidth=100 → 5 lines
    std::vector<float> widths(10, 50.0f);
    std::vector<SegmentBreakKind> kinds(10, SegmentBreakKind::Text);

    auto data = makeSimplePrepared(widths, kinds);
    int32_t lines = TextLineBreaker::countPreparedLines(data, 100.0f);
    EXPECT_EQ(lines, 5);
}

TEST(TextLineBreak, CountLines_SingleWideSegment) {
    // Single segment wider than maxWidth → 1 line (can't break)
    auto data = makeSimplePrepared(
        {500.0f},
        {SegmentBreakKind::Text}
    );
    int32_t lines = TextLineBreaker::countPreparedLines(data, 100.0f);
    EXPECT_EQ(lines, 1);
}

// ---------------------------------------------------------------------------
// walkPreparedLines line widths
// ---------------------------------------------------------------------------

TEST(TextLineBreak, WalkPreparedLines_AllLinesWithinMaxWidth) {
    // 6 segments of 60px, maxWidth=150 → each line ≤ 150
    std::vector<float> widths(6, 60.0f);
    std::vector<SegmentBreakKind> kinds(6, SegmentBreakKind::Text);
    auto data = makeSimplePrepared(widths, kinds);

    TextLineBreaker::walkPreparedLines(data, 150.0f,
        [&](const InternalLayoutLine& line) {
            EXPECT_LE(line.width, 150.0f);
        });
}

// ---------------------------------------------------------------------------
// walkPreparedLines line ranges
// ---------------------------------------------------------------------------

TEST(TextLineBreak, WalkPreparedLines_LineRangesContiguous) {
    std::vector<float> widths(4, 100.0f);
    std::vector<SegmentBreakKind> kinds(4, SegmentBreakKind::Text);
    auto data = makeSimplePrepared(widths, kinds);

    int32_t prevEndSegIdx = 0;
    int32_t prevEndGraphIdx = 0;
    bool first = true;

    TextLineBreaker::walkPreparedLines(data, 200.0f,
        [&](const InternalLayoutLine& line) {
            if (!first) {
                // Lines should be contiguous
                EXPECT_EQ(line.startSegmentIndex, prevEndSegIdx);
            }
            first = false;
            prevEndSegIdx = line.endSegmentIndex;
            prevEndGraphIdx = line.endGraphemeIndex;
        });
}

// ---------------------------------------------------------------------------
// Empty prepared data edge cases
// ---------------------------------------------------------------------------

TEST(TextLineBreak, CountPreparedLinesSimple_Empty) {
    PreparedLineBreakData data;
    data.simpleLineWalkFastPath = true;
    // Use public interface; internally it will delegate to the simple path
    int32_t lines = TextLineBreaker::countPreparedLines(data, 100.0f);
    EXPECT_EQ(lines, 0);
}

// ---------------------------------------------------------------------------
// Chunks covering full data
// ---------------------------------------------------------------------------

TEST(TextLineBreak, SingleChunkMultiSegment) {
    PreparedLineBreakData data;
    data.widths = {50.0f, 50.0f, 50.0f, 50.0f};
    data.kinds = {SegmentBreakKind::Text, SegmentBreakKind::Text,
                  SegmentBreakKind::Text, SegmentBreakKind::Text};
    data.simpleLineWalkFastPath = false;

    data.breakableWidths.resize(4);
    data.breakablePrefixWidths.resize(4);
    data.lineEndFitAdvances = {50.0f, 50.0f, 50.0f, 50.0f};
    data.lineEndPaintAdvances = {50.0f, 50.0f, 50.0f, 50.0f};

    // One chunk covering all segments
    data.chunks.resize(1);
    data.chunks[0] = {0, 4, 4};

    // maxWidth=100 → 2 lines
    int32_t lines = TextLineBreaker::countPreparedLines(data, 100.0f);
    EXPECT_EQ(lines, 2);
}

// ---------------------------------------------------------------------------
// Empty chunk handling
// ---------------------------------------------------------------------------

TEST(TextLineBreak, EmptyChunkEmitsOneLine) {
    PreparedLineBreakData data;
    data.widths = {0.0f};
    data.kinds = {SegmentBreakKind::Text};
    data.simpleLineWalkFastPath = false;

    data.breakableWidths.resize(1);
    data.breakablePrefixWidths.resize(1);
    data.lineEndFitAdvances = {0.0f};
    data.lineEndPaintAdvances = {0.0f};

    // startSegmentIndex == endSegmentIndex → empty chunk
    data.chunks.resize(1);
    data.chunks[0] = {0, 0, 1};

    int32_t lines = TextLineBreaker::countPreparedLines(data, 100.0f);
    EXPECT_EQ(lines, 1);  // empty chunk emits 1 line
}

// ---------------------------------------------------------------------------
// Soft hyphen handling
// ---------------------------------------------------------------------------

TEST(TextLineBreak, SoftHyphenTriggersBreak) {
    PreparedLineBreakData data;
    data.widths = {80.0f, 0.0f, 80.0f};
    data.kinds = {SegmentBreakKind::Text, SegmentBreakKind::SoftHyphen, SegmentBreakKind::Text};
    data.simpleLineWalkFastPath = false;
    data.discretionaryHyphenWidth = 10.0f;

    data.breakableWidths.resize(3);
    data.breakablePrefixWidths.resize(3);
    data.lineEndFitAdvances = {80.0f, 0.0f, 80.0f};
    data.lineEndPaintAdvances = {80.0f, 0.0f, 80.0f};

    // Single chunk
    data.chunks.resize(1);
    data.chunks[0] = {0, 3, 3};

    // maxWidth=90: first word(80) + soft hyphen fits, second word doesn't
    // Should break at soft hyphen → 2 lines
    int32_t lines = TextLineBreaker::countPreparedLines(data, 90.0f);
    EXPECT_EQ(lines, 2);
}
