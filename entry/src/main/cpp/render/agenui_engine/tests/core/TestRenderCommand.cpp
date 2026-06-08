/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for RenderCommand and FrameStats data structures.
 * No GPU required — pure data structure validation.
 */

#include <gtest/gtest.h>
#include "core/RenderCommand.h"

using namespace AgenUIEngine::Core;

// ---------------------------------------------------------------------------
// RenderCommand
// ---------------------------------------------------------------------------

TEST(RenderCommand, DefaultValues) {
    RenderCommand cmd;
    EXPECT_EQ(cmd.type, DrawType::Rectangle);
    EXPECT_FLOAT_EQ(cmd.depth, 0.0f);
    EXPECT_FALSE(cmd.transparent);
    EXPECT_FLOAT_EQ(cmd.cornerRadius, 0.0f);
    EXPECT_EQ(cmd.fontSize, 0u);
    EXPECT_FLOAT_EQ(cmd.maxWidth, 0.0f);
    EXPECT_FLOAT_EQ(cmd.lineHeight, 0.0f);
    EXPECT_FLOAT_EQ(cmd.glowWidth, 0.0f);
    EXPECT_FLOAT_EQ(cmd.glowIntensity, 0.0f);
}

TEST(RenderCommand, SortByKey) {
    // With SortKey, sorting uses the encoded key value, not raw depth
    RenderCommand cmd1, cmd2;
    cmd1.sortKey = SortKey::makeOpaque(DrawType::Rectangle, 1.0f, 0);
    cmd2.sortKey = SortKey::makeOpaque(DrawType::Rectangle, 2.0f, 1);
    EXPECT_TRUE(cmd1 < cmd2);
    EXPECT_FALSE(cmd2 < cmd1);
}

TEST(RenderCommand, SortStability) {
    // Same sortKey should return false for both directions
    RenderCommand cmd1, cmd2;
    cmd1.sortKey = SortKey::makeOpaque(DrawType::Rectangle, 5.0f, 0);
    cmd2.sortKey = SortKey::makeOpaque(DrawType::Rectangle, 5.0f, 0);
    EXPECT_FALSE(cmd1 < cmd2);
    EXPECT_FALSE(cmd2 < cmd1);
}

TEST(RenderCommand, RenderCommandFields) {
    RenderCommand cmd;
    cmd.position = glm::vec2(10.0f, 20.0f);
    cmd.size = glm::vec2(100.0f, 200.0f);
    cmd.color = glm::vec3(0.5f, 0.6f, 0.7f);
    cmd.text = "Hello";
    cmd.fontSize = 24;
    cmd.imagePath = "/path/to/image.png";
    cmd.cornerRadius = 15.0f;
    cmd.maxWidth = 500.0f;
    cmd.lineHeight = 1.5f;
    cmd.glowWidth = 3.0f;
    cmd.glowIntensity = 0.8f;

    EXPECT_EQ(cmd.position, glm::vec2(10.0f, 20.0f));
    EXPECT_EQ(cmd.size, glm::vec2(100.0f, 200.0f));
    EXPECT_EQ(cmd.color, glm::vec3(0.5f, 0.6f, 0.7f));
    EXPECT_EQ(cmd.text, "Hello");
    EXPECT_EQ(cmd.fontSize, 24u);
    EXPECT_EQ(cmd.imagePath, "/path/to/image.png");
    EXPECT_FLOAT_EQ(cmd.cornerRadius, 15.0f);
    EXPECT_FLOAT_EQ(cmd.maxWidth, 500.0f);
    EXPECT_FLOAT_EQ(cmd.lineHeight, 1.5f);
    EXPECT_FLOAT_EQ(cmd.glowWidth, 3.0f);
    EXPECT_FLOAT_EQ(cmd.glowIntensity, 0.8f);
}

// ---------------------------------------------------------------------------
// DrawType enum
// ---------------------------------------------------------------------------

TEST(DrawType, EnumValuesDistinct) {
    // Verify all enum values are distinct
    DrawType types[] = {
        DrawType::Rectangle,
        DrawType::RoundedRectangle,
        DrawType::GlassRoundedRectangle,
        DrawType::Text,
        DrawType::MultiLineText,
        DrawType::Image,
        DrawType::Custom
    };
    int count = sizeof(types) / sizeof(types[0]);
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            EXPECT_NE(static_cast<int>(types[i]), static_cast<int>(types[j]))
                << "DrawType values at index " << i << " and " << j << " are equal";
        }
    }
}

// ---------------------------------------------------------------------------
// FrameStats
// ---------------------------------------------------------------------------

TEST(FrameStats, InitZero) {
    FrameStats stats;
    EXPECT_EQ(stats.drawCalls, 0u);
    EXPECT_EQ(stats.batchedDrawCalls, 0u);
    EXPECT_EQ(stats.triangleCount, 0u);
    EXPECT_EQ(stats.commandCount, 0u);
}

TEST(FrameStats, Clear) {
    FrameStats stats;
    stats.drawCalls = 10;
    stats.batchedDrawCalls = 5;
    stats.triangleCount = 100;
    stats.commandCount = 20;

    stats.clear();

    EXPECT_EQ(stats.drawCalls, 0u);
    EXPECT_EQ(stats.batchedDrawCalls, 0u);
    EXPECT_EQ(stats.triangleCount, 0u);
    EXPECT_EQ(stats.commandCount, 0u);
}

// ---------------------------------------------------------------------------
// Additional RenderCommand edge cases
// ---------------------------------------------------------------------------

TEST(RenderCommand, SortByKey_NegativeDepth) {
    // Negative depth gets quantized into the SortKey; the key ordering is still well-defined
    RenderCommand cmd1, cmd2;
    cmd1.sortKey = SortKey::makeOpaque(DrawType::Rectangle, -5.0f, 0);
    cmd2.sortKey = SortKey::makeOpaque(DrawType::Rectangle, 3.0f, 1);
    EXPECT_TRUE(cmd1 < cmd2);
    EXPECT_FALSE(cmd2 < cmd1);
}

TEST(RenderCommand, SortByKey_ZeroVsPositive) {
    RenderCommand cmd1, cmd2;
    cmd1.sortKey = SortKey::makeOpaque(DrawType::Rectangle, 0.0f, 0);
    cmd2.sortKey = SortKey::makeOpaque(DrawType::Rectangle, 0.001f, 1);
    EXPECT_TRUE(cmd1 < cmd2);
}

TEST(RenderCommand, TransparentField) {
    RenderCommand cmd;
    EXPECT_FALSE(cmd.transparent);
    cmd.transparent = true;
    EXPECT_TRUE(cmd.transparent);
}

TEST(RenderCommand, PositionSizeColorSetGet) {
    RenderCommand cmd;
    cmd.position = glm::vec2(0.0f);
    cmd.size = glm::vec2(0.0f);
    cmd.color = glm::vec3(0.0f);
    EXPECT_EQ(cmd.position, glm::vec2(0.0f));
    EXPECT_EQ(cmd.size, glm::vec2(0.0f));
    EXPECT_EQ(cmd.color, glm::vec3(0.0f));
}

TEST(RenderCommand, EmptyTextAndPaths) {
    RenderCommand cmd;
    EXPECT_TRUE(cmd.text.empty());
    EXPECT_TRUE(cmd.imagePath.empty());
}

// ---------------------------------------------------------------------------
// FrameStats edge cases
// ---------------------------------------------------------------------------

TEST(FrameStats, ClearIdempotent) {
    FrameStats stats;
    stats.clear();
    stats.clear();  // double clear should be safe
    EXPECT_EQ(stats.drawCalls, 0u);
}

TEST(FrameStats, Accumulation) {
    FrameStats stats;
    stats.drawCalls += 5;
    stats.drawCalls += 3;
    EXPECT_EQ(stats.drawCalls, 8u);
}

TEST(FrameStats, LargeValues) {
    FrameStats stats;
    stats.drawCalls = UINT32_MAX;
    stats.triangleCount = UINT32_MAX;
    EXPECT_EQ(stats.drawCalls, UINT32_MAX);
    EXPECT_EQ(stats.triangleCount, UINT32_MAX);
    stats.clear();
    EXPECT_EQ(stats.drawCalls, 0u);
}
