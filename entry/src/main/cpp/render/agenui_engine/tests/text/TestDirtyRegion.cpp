/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for DirtyRegion — merge/reset logic.
 * No GPU required — pure data structure validation.
 */

#include <gtest/gtest.h>
#include "modules/text/TextAtlas.h"

using namespace AgenUIEngine;

// ---------------------------------------------------------------------------
// DirtyRegion defaults
// ---------------------------------------------------------------------------

TEST(DirtyRegion, DefaultNotDirty) {
    DirtyRegion r;
    EXPECT_FALSE(r.isDirty);
    EXPECT_EQ(r.width(), 0u);
    EXPECT_EQ(r.height(), 0u);
}

// ---------------------------------------------------------------------------
// DirtyRegion merge
// ---------------------------------------------------------------------------

TEST(DirtyRegion, MergeSingle) {
    DirtyRegion r;
    r.merge(10, 20, 30, 40);
    EXPECT_TRUE(r.isDirty);
    EXPECT_EQ(r.minX, 10u);
    EXPECT_EQ(r.minY, 20u);
    EXPECT_EQ(r.maxX, 40u);  // 10 + 30
    EXPECT_EQ(r.maxY, 60u);  // 20 + 40
    EXPECT_EQ(r.width(), 30u);
    EXPECT_EQ(r.height(), 40u);
}

TEST(DirtyRegion, MergeExpands) {
    DirtyRegion r;
    r.merge(10, 10, 20, 20);
    r.merge(5, 5, 10, 10);  // extends to upper-left
    EXPECT_EQ(r.minX, 5u);
    EXPECT_EQ(r.minY, 5u);
    EXPECT_EQ(r.maxX, 30u);  // max(30, 15) = 30
    EXPECT_EQ(r.maxY, 30u);  // max(30, 15) = 30
}

TEST(DirtyRegion, MergeContained) {
    DirtyRegion r;
    r.merge(0, 0, 100, 100);
    r.merge(10, 10, 10, 10);  // fully contained
    EXPECT_EQ(r.minX, 0u);
    EXPECT_EQ(r.maxX, 100u);
    EXPECT_EQ(r.width(), 100u);
    EXPECT_EQ(r.height(), 100u);
}

TEST(DirtyRegion, MergeMultipleExpandsBothDirections) {
    DirtyRegion r;
    r.merge(50, 50, 10, 10);
    r.merge(0, 0, 10, 10);      // top-left
    r.merge(90, 90, 10, 10);    // bottom-right
    EXPECT_EQ(r.minX, 0u);
    EXPECT_EQ(r.minY, 0u);
    EXPECT_EQ(r.maxX, 100u);
    EXPECT_EQ(r.maxY, 100u);
}

// ---------------------------------------------------------------------------
// DirtyRegion reset
// ---------------------------------------------------------------------------

TEST(DirtyRegion, ResetClearsDirty) {
    DirtyRegion r;
    r.merge(0, 0, 10, 10);
    EXPECT_TRUE(r.isDirty);
    r.reset();
    EXPECT_FALSE(r.isDirty);
    EXPECT_EQ(r.minX, UINT32_MAX);
    EXPECT_EQ(r.minY, UINT32_MAX);
    EXPECT_EQ(r.maxX, 0u);
    EXPECT_EQ(r.maxY, 0u);
    EXPECT_EQ(r.width(), 0u);
    EXPECT_EQ(r.height(), 0u);
}

TEST(DirtyRegion, ResetThenMergeAgain) {
    DirtyRegion r;
    r.merge(100, 100, 50, 50);
    r.reset();
    r.merge(0, 0, 10, 10);
    EXPECT_TRUE(r.isDirty);
    EXPECT_EQ(r.minX, 0u);
    EXPECT_EQ(r.maxX, 10u);
}

// ---------------------------------------------------------------------------
// DirtyRegion dimensions
// ---------------------------------------------------------------------------

TEST(DirtyRegion, WidthHeightCorrect) {
    DirtyRegion r;
    r.merge(0, 0, 200, 100);
    EXPECT_EQ(r.width(), 200u);
    EXPECT_EQ(r.height(), 100u);
}

TEST(DirtyRegion, ZeroSizeWhenNotDirty) {
    DirtyRegion r;
    EXPECT_EQ(r.width(), 0u);
    EXPECT_EQ(r.height(), 0u);
}
