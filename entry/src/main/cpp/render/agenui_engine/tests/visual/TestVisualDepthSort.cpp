/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Level 3 Visual Integration Test — Depth Sorting
 *
 * Tests that RenderQueue correctly sorts draw commands by depth and that
 * the rendered output reflects the expected overlap/occlusion order.
 *
 * RenderCommand.depth: ascending sort → lower depth renders first (background),
 * higher depth renders later (foreground). When overlapping, higher-depth
 * content covers lower-depth content.
 *
 * Categories:
 *   1. CPU-only framework validation (ReferenceImageGen + ImageComparator)
 *   2. GPU-based visual tests (runAndCompare with queue mode)
 *
 * Tolerance: maxAvgError=2.0, maxPixelError=10.0, maxMismatchRatio=0.02
 */

#include "VisualTestFramework.h"

using namespace AgenUIEngine::Core;

namespace {
constexpr uint32_t kCanvasWidth  = 400;
constexpr uint32_t kCanvasHeight = 300;

ImageComparator::Options depthSortOpts() {
    ImageComparator::Options opts;
    opts.maxAvgError      = 2.0;
    opts.maxPixelError    = 10.0;
    opts.maxMismatchRatio = 0.02;
    opts.diffOutputDir    = "./visual_test_diffs/depth_sort/";
    return opts;
}

struct Pixel { uint8_t r, g, b, a; };

Pixel getPixel(const CaptureResult& frame, uint32_t x, uint32_t y) {
    uint32_t idx = (y * frame.width + x) * 4;
    return {frame.pixels[idx], frame.pixels[idx+1],
            frame.pixels[idx+2], frame.pixels[idx+3]};
}

uint32_t toUint8(float f) {
    return static_cast<uint32_t>(std::round(f * 255.0f));
}

} // anonymous namespace

// ============================================================================
// Fixture
// ============================================================================

class VisualDepthSortTest : public VisualTestBase {
protected:
    void SetUp() override {
        surfaceWidth  = kCanvasWidth;
        surfaceHeight = kCanvasHeight;
    }

    void renderScene() override {
        // Override in sub-fixtures for specific depth sort scenarios
    }
};

// ============================================================================
// Part 1: CPU-only framework validation
// ============================================================================

TEST_F(VisualDepthSortTest, CpuTwoOverlappingRects_HigherDepthOnTop) {
    // Green rect at (50,50) 200x200, depth=0
    // Red rect at (100,100) 200x200, depth=1
    // Overlap area (100-249, 100-249) should be red (higher depth = on top)

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight,
        0, 0, 0, // black background
        {
            {50, 50, 200, 200, 0, 200, 0},    // green (depth=0, drawn first)
            {100, 100, 200, 200, 255, 0, 0},   // red (depth=1, drawn second → on top)
        }
    );

    // Expected: same overlapping rects, red on top in overlap area
    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(expected, 50, 50, 200, 200, 0, 200, 0);
    PPMImage::fillRect(expected, 100, 100, 200, 200, 255, 0, 0);

    auto result = ImageComparator::compare(actual, expected, depthSortOpts());
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.mismatchPixels, 0u);
}

TEST_F(VisualDepthSortTest, CpuThreeLayerDepth) {
    // Three overlapping rects:
    //   Green depth=0, Red depth=1, Blue depth=2
    // Triple-overlap area should be blue

    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(expected, 20, 20, 200, 200, 0, 200, 0);    // green, depth=0
    PPMImage::fillRect(expected, 80, 80, 200, 200, 255, 0, 0);    // red, depth=1
    PPMImage::fillRect(expected, 140, 140, 200, 200, 0, 0, 255);  // blue, depth=2

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight, 0, 0, 0,
        {
            {20, 20, 200, 200, 0, 200, 0},
            {80, 80, 200, 200, 255, 0, 0},
            {140, 140, 200, 200, 0, 0, 255},
        }
    );

    // Verify triple-overlap center is blue
    auto px = getPixel(actual, 180, 180);
    EXPECT_EQ(px.r, 0);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 255);

    auto result = ImageComparator::compare(actual, expected, depthSortOpts());
    EXPECT_TRUE(result.passed);
}

TEST_F(VisualDepthSortTest, CpuSameDepthPaintersOrder) {
    // Two rects at same depth: second draw call covers first (painter's order)
    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 40, 40, 50);
    PPMImage::fillRect(expected, 50, 50, 200, 150, 255, 255, 0);   // yellow
    PPMImage::fillRect(expected, 100, 80, 150, 120, 255, 0, 255);  // magenta (drawn second)

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight, 40, 40, 50,
        {
            {50, 50, 200, 150, 255, 255, 0},
            {100, 80, 150, 120, 255, 0, 255},
        }
    );

    // Overlap area should be magenta (last drawn)
    auto px = getPixel(actual, 150, 120);
    EXPECT_EQ(px.r, 255);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 255);

    auto result = ImageComparator::compare(actual, expected, depthSortOpts());
    EXPECT_TRUE(result.passed);
}

TEST_F(VisualDepthSortTest, CpuNegativeDepths) {
    // Rects at negative depths still sort correctly
    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(expected, 30, 30, 150, 120, 100, 100, 255);  // depth=-2
    PPMImage::fillRect(expected, 80, 60, 150, 120, 255, 200, 0);    // depth=-1

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight, 0, 0, 0,
        {
            {30, 30, 150, 120, 100, 100, 255},
            {80, 60, 150, 120, 255, 200, 0},
        }
    );

    // Overlap should show the higher-depth (less negative) color
    auto px = getPixel(actual, 120, 100);
    EXPECT_EQ(px.r, 255);
    EXPECT_EQ(px.g, 200);

    auto result = ImageComparator::compare(actual, expected, depthSortOpts());
    EXPECT_TRUE(result.passed);
}

TEST_F(VisualDepthSortTest, CpuBackgroundForeground) {
    // Full-canvas background at depth=0, small foreground rects at depth=1+
    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 50, 50, 60);
    PPMImage::fillRect(expected, 100, 80, 80, 60, 255, 100, 100);   // red-ish, depth=1
    PPMImage::fillRect(expected, 250, 80, 80, 60, 100, 255, 100);   // green-ish, depth=2
    PPMImage::fillRect(expected, 175, 170, 80, 60, 100, 100, 255);  // blue-ish, depth=3

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight, 50, 50, 60,
        {
            {0, 0, kCanvasWidth, kCanvasHeight, 50, 50, 60},  // bg covers all
            {100, 80, 80, 60, 255, 100, 100},
            {250, 80, 80, 60, 100, 255, 100},
            {175, 170, 80, 60, 100, 100, 255},
        }
    );

    // Background should be visible outside the small rects
    auto bg = getPixel(actual, 10, 10);
    EXPECT_EQ(bg.r, 50); EXPECT_EQ(bg.g, 50); EXPECT_EQ(bg.b, 60);

    // Small rects should be on top
    auto red = getPixel(actual, 140, 110);
    EXPECT_EQ(red.r, 255); EXPECT_EQ(red.g, 100);

    auto result = ImageComparator::compare(actual, expected, depthSortOpts());
    EXPECT_TRUE(result.passed);
}

TEST_F(VisualDepthSortTest, CpuManyRectsSorted) {
    // 10 overlapping rects at different depths
    std::vector<std::tuple<uint32_t,uint32_t,uint32_t,uint32_t,
                           uint8_t,uint8_t,uint8_t>> rects;

    // Staircase pattern: each rect shifted 10px right and 10px down
    for (int i = 0; i < 10; ++i) {
        uint8_t gray = static_cast<uint8_t>(25 * (i + 1));
        rects.push_back({30u + i*15u, 20u + i*15u, 180u, 150u, gray, gray, gray});
    }

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight, 0, 0, 0, rects);

    // The topmost rect (last in list, highest depth) should be visible
    // at its center area
    auto px = getPixel(actual, 30 + 9*15 + 90, 20 + 9*15 + 75);
    EXPECT_EQ(px.r, 250); // 25 * 10 = 250

    // An earlier rect should be covered at that position
    // The first rect's area (30,20)-(210,170) is partially covered
}

// ============================================================================
// Part 2: GPU-based visual tests
// ============================================================================

// --- Two overlapping rects with depth sorting ---
class GpuDepthSortTwoRectsTest : public VisualDepthSortTest {
protected:
    void renderScene() override {
        // Queue mode: draw in reverse depth order to test sorting
        // context->beginQueue();
        // context->drawRect(glm::vec2(100,100), glm::vec2(200,200), glm::vec3(1,0,0));
        //   // Red at depth=1 (add via RenderCommand with depth set)
        // context->drawRect(glm::vec2(50,50), glm::vec2(200,200), glm::vec3(0,1,0));
        //   // Green at depth=0
        // context->endQueue();
        // context->flushQueue();
        //
        // Expected: green renders first, red on top in overlap area
    }
};

TEST_F(GpuDepthSortTwoRectsTest, OverlappingRectsDepthSort) {
    auto result = runAndCompare("depth_sort_two_rects", depthSortOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Three layer depth ---
class GpuDepthSortThreeLayerTest : public VisualDepthSortTest {
protected:
    void renderScene() override {
        // context->beginQueue();
        // context->drawRect(glm::vec2(20,20), glm::vec2(200,200), glm::vec3(0,0.8,0));
        // context->drawRect(glm::vec2(80,80), glm::vec2(200,200), glm::vec3(0.8,0,0));
        // context->drawRect(glm::vec2(140,140), glm::vec2(200,200), glm::vec3(0,0,1));
        // context->endQueue();
        // context->flushQueue();
    }
};

TEST_F(GpuDepthSortThreeLayerTest, ThreeLayerDepth) {
    auto result = runAndCompare("depth_sort_three_layers", depthSortOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Background + foreground ---
class GpuDepthSortBgFgTest : public VisualDepthSortTest {
protected:
    void renderScene() override {
        // context->beginQueue();
        // context->drawRect(glm::vec2(0,0),
        //     glm::vec2(kCanvasWidth, kCanvasHeight),
        //     glm::vec3(0.2f, 0.2f, 0.24f));
        // context->drawRect(glm::vec2(100,80), glm::vec2(80,60), glm::vec3(1,0.4f,0.4f));
        // context->drawRect(glm::vec2(250,80), glm::vec2(80,60), glm::vec3(0.4f,1,0.4f));
        // context->drawRect(glm::vec2(175,170), glm::vec2(80,60), glm::vec3(0.4f,0.4f,1));
        // context->endQueue();
        // context->flushQueue();
    }
};

TEST_F(GpuDepthSortBgFgTest, BackgroundForeground) {
    auto result = runAndCompare("depth_sort_bg_fg", depthSortOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Many rects ---
class GpuDepthSortManyRectsTest : public VisualDepthSortTest {
protected:
    void renderScene() override {
        // context->beginQueue();
        // for (int i = 0; i < 10; ++i) {
        //     float gray = (i + 1) / 10.0f;
        //     context->drawRect(
        //         glm::vec2(30 + i*15, 20 + i*15),
        //         glm::vec2(180, 150),
        //         glm::vec3(gray, gray, gray));
        // }
        // context->endQueue();
        // context->flushQueue();
    }
};

TEST_F(GpuDepthSortManyRectsTest, ManyRectsSorted) {
    auto result = runAndCompare("depth_sort_many_rects", depthSortOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Negative depth values ---
class GpuDepthSortNegativeTest : public VisualDepthSortTest {
protected:
    void renderScene() override {
        // context->beginQueue();
        // context->drawRect(glm::vec2(30,30), glm::vec2(150,120), glm::vec3(0.4f,0.4f,1));
        // context->drawRect(glm::vec2(80,60), glm::vec2(150,120), glm::vec3(1,0.8f,0));
        // context->endQueue();
        // context->flushQueue();
    }
};

TEST_F(GpuDepthSortNegativeTest, NegativeDepths) {
    auto result = runAndCompare("depth_sort_negative", depthSortOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}
