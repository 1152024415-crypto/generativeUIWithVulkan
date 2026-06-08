/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for RenderQueue — command collection and sorting.
 * No GPU required.
 */

#include <gtest/gtest.h>
#include "core/RenderQueue.h"

using namespace AgenUIEngine::Core;

TEST(RenderQueue, EmptyQueue) {
    RenderQueue queue;
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_EQ(queue.getCommandCount(), 0u);
}

TEST(RenderQueue, BeginEndQueue) {
    RenderQueue queue;
    queue.beginQueue();
    // After begin, commands are cleared and collecting is true
    EXPECT_TRUE(queue.isEmpty());

    queue.drawRect({0, 0}, {100, 100}, {1, 1, 1});
    EXPECT_FALSE(queue.isEmpty());
    EXPECT_EQ(queue.getCommandCount(), 1u);

    queue.endQueue();
    // After end, commands are sorted but still present
    EXPECT_EQ(queue.getCommandCount(), 1u);
}

TEST(RenderQueue, DrawRectangle) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRect({10, 20}, {100, 200}, {0.5f, 0.6f, 0.7f});
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].type, DrawType::Rectangle);
    EXPECT_EQ(cmds[0].position, glm::vec2(10, 20));
    EXPECT_EQ(cmds[0].size, glm::vec2(100, 200));
    EXPECT_EQ(cmds[0].color, glm::vec3(0.5f, 0.6f, 0.7f));
}

TEST(RenderQueue, DrawRoundedRectangle) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRoundedRect({5, 10}, {50, 60}, 12.0f, {1, 0, 0});
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].type, DrawType::RoundedRectangle);
    EXPECT_FLOAT_EQ(cmds[0].cornerRadius, 12.0f);
    EXPECT_EQ(cmds[0].position, glm::vec2(5, 10));
}

TEST(RenderQueue, DrawGlassRoundedRectangle) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawGlassRoundedRect({0, 0}, {200, 100}, 20.0f, {0.1f, 0.2f, 0.3f});
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].type, DrawType::GlassRoundedRectangle);
    EXPECT_FLOAT_EQ(cmds[0].cornerRadius, 20.0f);
    EXPECT_TRUE(cmds[0].transparent);
}

TEST(RenderQueue, DrawText) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawText("Hello", {30, 40}, 16, {1, 1, 1}, 2.0f, 0.5f);
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].type, DrawType::Text);
    EXPECT_EQ(cmds[0].text, "Hello");
    EXPECT_EQ(cmds[0].fontSize, 16u);
    EXPECT_FLOAT_EQ(cmds[0].glowWidth, 2.0f);
    EXPECT_FLOAT_EQ(cmds[0].glowIntensity, 0.5f);
    EXPECT_TRUE(cmds[0].transparent);
}

TEST(RenderQueue, DrawMultiLineText) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawMultiLineText("Line1\nLine2", {0, 0}, 14, {0, 0, 0}, 300.0f, 1.2f);
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].type, DrawType::MultiLineText);
    EXPECT_EQ(cmds[0].text, "Line1\nLine2");
    EXPECT_FLOAT_EQ(cmds[0].maxWidth, 300.0f);
    EXPECT_FLOAT_EQ(cmds[0].lineHeight, 1.2f);
    EXPECT_TRUE(cmds[0].transparent);
}

TEST(RenderQueue, DrawImage) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawImage("/images/test.png", {10, 20}, {640, 480});
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].type, DrawType::Image);
    EXPECT_EQ(cmds[0].imagePath, "/images/test.png");
    EXPECT_EQ(cmds[0].position, glm::vec2(10, 20));
    EXPECT_EQ(cmds[0].size, glm::vec2(640, 480));
}

TEST(RenderQueue, MultipleCommands) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    EXPECT_EQ(queue.getCommandCount(), 1u);
    queue.drawText("abc", {0, 0}, 12, {0, 0, 0});
    EXPECT_EQ(queue.getCommandCount(), 2u);
    queue.drawImage("img.png", {0, 0}, {50, 50});
    EXPECT_EQ(queue.getCommandCount(), 3u);
    queue.endQueue();

    EXPECT_EQ(queue.getCommandCount(), 3u);
}

TEST(RenderQueue, SortByDepth) {
    RenderQueue queue;
    queue.beginQueue();

    // Add commands with varying depths (via modifying after adding)
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});

    // Manually set depths
    auto& cmds = const_cast<std::vector<RenderCommand>&>(queue.getCommands());
    cmds[0].depth = 3.0f;
    cmds[1].depth = 1.0f;
    cmds[2].depth = 2.0f;

    queue.endQueue();

    const auto& sorted = queue.getCommands();
    ASSERT_EQ(sorted.size(), 3u);
    EXPECT_FLOAT_EQ(sorted[0].depth, 1.0f);
    EXPECT_FLOAT_EQ(sorted[1].depth, 2.0f);
    EXPECT_FLOAT_EQ(sorted[2].depth, 3.0f);
}

TEST(RenderQueue, ClearResetsQueue) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.endQueue();
    EXPECT_FALSE(queue.isEmpty());

    queue.clear();
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_EQ(queue.getCommandCount(), 0u);
}

TEST(RenderQueue, GetCommandsReturnsAll) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.drawRoundedRect({0, 0}, {10, 10}, 5.0f, {0, 0, 0});
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    EXPECT_EQ(cmds.size(), 2u);
}

TEST(RenderQueue, CommandDataIntegrity) {
    RenderQueue queue;
    queue.beginQueue();

    glm::vec2 pos(15.5f, 25.5f);
    glm::vec2 size(200.0f, 300.0f);
    glm::vec3 color(0.8f, 0.2f, 0.4f);
    queue.drawRect(pos, size, color);

    glm::vec3 textColor(0.1f, 0.9f, 0.3f);
    queue.drawText("TestText", {50, 60}, 32, textColor, 4.0f, 0.7f);

    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 2u);

    // Verify rectangle command
    EXPECT_EQ(cmds[0].type, DrawType::Rectangle);
    EXPECT_EQ(cmds[0].position, pos);
    EXPECT_EQ(cmds[0].size, size);
    EXPECT_EQ(cmds[0].color, color);

    // Verify text command
    EXPECT_EQ(cmds[1].type, DrawType::Text);
    EXPECT_EQ(cmds[1].text, "TestText");
    EXPECT_EQ(cmds[1].fontSize, 32u);
    EXPECT_FLOAT_EQ(cmds[1].glowWidth, 4.0f);
    EXPECT_FLOAT_EQ(cmds[1].glowIntensity, 0.7f);
}

// ---------------------------------------------------------------------------
// Stable sort semantics
// ---------------------------------------------------------------------------

TEST(RenderQueue, SortStabilitySameDepth) {
    RenderQueue queue;
    queue.beginQueue();

    // Add 5 rect commands with the same depth but different positions
    queue.drawRect({10, 0}, {10, 10}, {1, 0, 0});   // pos.x=10
    queue.drawRect({20, 0}, {10, 10}, {0, 1, 0});   // pos.x=20
    queue.drawRect({30, 0}, {10, 10}, {0, 0, 1});   // pos.x=30
    queue.drawRect({40, 0}, {10, 10}, {1, 1, 0});   // pos.x=40
    queue.drawRect({50, 0}, {10, 10}, {0, 1, 1});   // pos.x=50

    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 5u);
    // All depths are 0 (default), so stable_sort should preserve insertion order
    for (size_t i = 0; i < cmds.size(); ++i) {
        float expectedX = static_cast<float>(10 + 10 * i);
        EXPECT_FLOAT_EQ(cmds[i].position.x, expectedX)
            << "Order changed at index " << i;
    }
}

TEST(RenderQueue, SortMixedDepthsStable) {
    RenderQueue queue;
    queue.beginQueue();

    // Add commands with mixed depths, some equal
    queue.drawRect({10, 0}, {10, 10}, {1, 0, 0});
    queue.drawRect({20, 0}, {10, 10}, {0, 1, 0});
    queue.drawRect({30, 0}, {10, 10}, {0, 0, 1});

    auto& cmds = const_cast<std::vector<RenderCommand>&>(queue.getCommands());
    cmds[0].depth = 1.0f;
    cmds[1].depth = 0.0f;  // should come first
    cmds[2].depth = 1.0f;  // same as [0], should keep relative order

    queue.endQueue();

    const auto& sorted = queue.getCommands();
    ASSERT_EQ(sorted.size(), 3u);
    EXPECT_FLOAT_EQ(sorted[0].depth, 0.0f);
    EXPECT_FLOAT_EQ(sorted[0].position.x, 20.0f);
    // cmds[0] and cmds[2] both have depth=1.0, stable_sort preserves order
    EXPECT_FLOAT_EQ(sorted[1].depth, 1.0f);
    EXPECT_FLOAT_EQ(sorted[1].position.x, 10.0f);
    EXPECT_FLOAT_EQ(sorted[2].depth, 1.0f);
    EXPECT_FLOAT_EQ(sorted[2].position.x, 30.0f);
}

// ---------------------------------------------------------------------------
// beginQueue clears previous commands
// ---------------------------------------------------------------------------

TEST(RenderQueue, BeginQueueClearsPreviousCommands) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.endQueue();
    EXPECT_EQ(queue.getCommandCount(), 1u);

    // beginQueue again should clear
    queue.beginQueue();
    EXPECT_TRUE(queue.isEmpty());
    EXPECT_EQ(queue.getCommandCount(), 0u);
}

// ---------------------------------------------------------------------------
// All draw types cover transparent flag
// ---------------------------------------------------------------------------

TEST(RenderQueue, TransparentFlags) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.drawRoundedRect({0, 0}, {10, 10}, 5.0f, {1, 1, 1});
    queue.drawGlassRoundedRect({0, 0}, {10, 10}, 5.0f, {1, 1, 1});
    queue.drawText("t", {0, 0}, 12, {0, 0, 0});
    queue.drawMultiLineText("t", {0, 0}, 12, {0, 0, 0}, 100.0f, 1.0f);
    queue.drawImage("a.png", {0, 0}, {10, 10});
    queue.endQueue();

    const auto& cmds = queue.getCommands();
    ASSERT_EQ(cmds.size(), 6u);
    // After SortKey sorting: opaque commands grouped first (by DrawType),
    // then transparent commands grouped after.
    // Opaque: Rectangle(0), RoundedRectangle(1), Image(5)
    // Transparent: GlassRoundedRectangle(2), Text(3), MultiLineText(4)
    EXPECT_FALSE(cmds[0].transparent);  // Rectangle (opaque)
    EXPECT_FALSE(cmds[1].transparent);  // RoundedRectangle (opaque)
    EXPECT_FALSE(cmds[2].transparent);  // Image (opaque)
    EXPECT_TRUE(cmds[3].transparent);   // GlassRoundedRectangle (transparent)
    EXPECT_TRUE(cmds[4].transparent);   // Text (transparent)
    EXPECT_TRUE(cmds[5].transparent);   // MultiLineText (transparent)
}

// ---------------------------------------------------------------------------
// Negative depth sorting
// ---------------------------------------------------------------------------

TEST(RenderQueue, SortNegativeDepths) {
    RenderQueue queue;
    queue.beginQueue();
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});
    queue.drawRect({0, 0}, {10, 10}, {1, 1, 1});

    auto& cmds = const_cast<std::vector<RenderCommand>&>(queue.getCommands());
    cmds[0].depth = 2.0f;
    cmds[1].depth = -1.0f;
    cmds[2].depth = 0.0f;

    queue.endQueue();

    const auto& sorted = queue.getCommands();
    EXPECT_FLOAT_EQ(sorted[0].depth, -1.0f);
    EXPECT_FLOAT_EQ(sorted[1].depth, 0.0f);
    EXPECT_FLOAT_EQ(sorted[2].depth, 2.0f);
}

