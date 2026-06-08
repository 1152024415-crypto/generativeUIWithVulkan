/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for Glyph data structure defaults.
 * No GPU required — pure data structure validation.
 */

#include <gtest/gtest.h>
#include "modules/text/FontManager.h"

using namespace AgenUIEngine;

TEST(Glyph, DefaultValues) {
    Glyph g;
    EXPECT_EQ(g.codepoint, 0u);
    EXPECT_EQ(g.width, 0u);
    EXPECT_EQ(g.height, 0u);
    EXPECT_EQ(g.bearingX, 0);
    EXPECT_EQ(g.bearingY, 0);
    EXPECT_EQ(g.bitmapLeft, 0);
    EXPECT_EQ(g.bitmapTop, 0);
    EXPECT_EQ(g.advance, 0u);
    EXPECT_EQ(g.bitmapData, nullptr);
    EXPECT_EQ(g.bitmapSize, 0u);
    EXPECT_EQ(g.atlasX, 0u);
    EXPECT_EQ(g.atlasY, 0u);
}

TEST(Glyph, FieldAssignment) {
    Glyph g;
    g.codepoint = 'A';
    g.width = 12;
    g.height = 16;
    g.bearingX = 1;
    g.bearingY = 14;
    g.bitmapLeft = 0;
    g.bitmapTop = 14;
    g.advance = 14 << 6;  // 26.6 fixed-point
    g.atlasX = 100;
    g.atlasY = 200;

    EXPECT_EQ(g.codepoint, 'A');
    EXPECT_EQ(g.width, 12u);
    EXPECT_EQ(g.height, 16u);
    EXPECT_EQ(g.advance, 14u << 6);
    EXPECT_EQ(g.atlasX, 100u);
    EXPECT_EQ(g.atlasY, 200u);
}

TEST(Glyph, MultipleInstances) {
    Glyph g1, g2;
    g1.codepoint = 'A';
    g2.codepoint = 'B';
    EXPECT_NE(g1.codepoint, g2.codepoint);
    EXPECT_EQ(g1.width, g2.width);  // both default to 0
}
