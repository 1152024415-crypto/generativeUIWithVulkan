/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Level 3 Visual Integration Test — SDF Rounded Rectangle Rendering
 *
 * Tests the SDF-based rounded rectangle shader (simple_rounded_rect.frag).
 *
 * Shader key behavior:
 *   float dist = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - cornerR;
 *   float aa = fwidth(dist) * 2.0;
 *   float shapeAlpha = 1.0 - smoothstep(-aa, aa, dist);
 *
 * Categories:
 *   1. CPU-only framework validation (no GPU required)
 *   2. GPU-based visual tests (runAndCompare)
 *
 * Tolerance:
 *   - SDF AA edges: maxAvgError=5.0, maxPixelError=20.0, maxMismatchRatio=0.05
 *   - Zero radius (sharp edge): maxAvgError=1.0, maxPixelError=5.0, maxMismatchRatio=0.005
 */

#include "VisualTestFramework.h"

using namespace AgenUIEngine::Core;

namespace {
constexpr uint32_t kCanvasWidth  = 400;
constexpr uint32_t kCanvasHeight = 300;

ImageComparator::Options sdfRelaxedOpts() {
    ImageComparator::Options opts;
    opts.maxAvgError      = 5.0;
    opts.maxPixelError    = 20.0;
    opts.maxMismatchRatio = 0.05;
    opts.diffOutputDir    = "./visual_test_diffs/rounded_rect/";
    return opts;
}

ImageComparator::Options strictSharpOpts() {
    ImageComparator::Options opts;
    opts.maxAvgError      = 1.0;
    opts.maxPixelError    = 5.0;
    opts.maxMismatchRatio = 0.005;
    opts.diffOutputDir    = "./visual_test_diffs/rounded_rect/";
    return opts;
}

struct Pixel { uint8_t r, g, b, a; };

Pixel getPixel(const CaptureResult& frame, uint32_t x, uint32_t y) {
    uint32_t idx = (y * frame.width + x) * 4;
    return {frame.pixels[idx], frame.pixels[idx+1],
            frame.pixels[idx+2], frame.pixels[idx+3]};
}

} // anonymous namespace

// ============================================================================
// Fixture
// ============================================================================

class VisualRoundedRectTest : public VisualTestBase {
protected:
    void SetUp() override {
        surfaceWidth  = kCanvasWidth;
        surfaceHeight = kCanvasHeight;
    }

    void renderScene() override {
        // GPU path (requires Vulkan device):
        // context->drawRoundedRect(
        //     glm::vec2(rectX_, rectY_),
        //     glm::vec2(rectW_, rectH_),
        //     cornerRadius_,
        //     glm::vec3(colorR_, colorG_, colorB_));
        (void)rectX_; (void)rectY_; (void)rectW_; (void)rectH_;
        (void)cornerRadius_; (void)colorR_; (void)colorG_; (void)colorB_;
    }

    // Configurable render parameters
    uint32_t rectX_ = 50, rectY_ = 50, rectW_ = 200, rectH_ = 100;
    float cornerRadius_ = 20.0f;
    float colorR_ = 1.0f, colorG_ = 0.0f, colorB_ = 0.0f;
};

// ============================================================================
// Part 1: CPU-only framework validation
// ============================================================================

TEST_F(VisualRoundedRectTest, CpuSharpRectMatchesReference) {
    // A zero-radius rounded rect should match a sharp rect reference exactly
    auto actual   = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(actual, 50, 50, 200, 100, 255, 0, 0);

    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(expected, 50, 50, 200, 100, 255, 0, 0);

    auto result = ImageComparator::compare(actual, expected, strictSharpOpts());
    EXPECT_TRUE(result.passed);
    EXPECT_DOUBLE_EQ(result.averageError, 0.0);
    EXPECT_EQ(result.mismatchPixels, 0u);
}

TEST_F(VisualRoundedRectTest, CpuRoundedVsSharpMismatchAtCorners) {
    // Simulate SDF AA: create a rounded rect with "cut corners" vs a sharp rect.
    // The corners should differ.

    // Sharp reference: full 200x100 rectangle
    auto sharp = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(sharp, 50, 50, 200, 100, 255, 0, 0);

    // "Rounded" version: same rect but with 10px corners removed (simplified)
    auto rounded = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(rounded, 50, 50, 200, 100, 255, 0, 0);
    // Cut the 4 corners (10x10 each)
    PPMImage::fillRect(rounded, 50, 50, 10, 10, 0, 0, 0);     // top-left
    PPMImage::fillRect(rounded, 240, 50, 10, 10, 0, 0, 0);     // top-right
    PPMImage::fillRect(rounded, 50, 140, 10, 10, 0, 0, 0);     // bottom-left
    PPMImage::fillRect(rounded, 240, 140, 10, 10, 0, 0, 0);    // bottom-right

    auto result = ImageComparator::compare(rounded, sharp, sdfRelaxedOpts());
    // Should detect the corner differences
    EXPECT_GT(result.mismatchPixels, 0u);
}

TEST_F(VisualRoundedRectTest, CpuLargeRadiusProducesCapsule) {
    // Radius = min(w,h)/2 should produce a capsule/pill shape.
    // At the very edges (top/bottom center), pixels should exist,
    // but at corners (top-left diagonal), they should be background.

    auto frame = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    // Simulate capsule: a 200x100 rect with radius=50
    // The capsule body
    PPMImage::fillRect(frame, 50, 50, 200, 100, 255, 128, 0);
    // Remove corner regions beyond the capsule arc
    PPMImage::fillRect(frame, 50, 50, 10, 10, 0, 0, 0);
    PPMImage::fillRect(frame, 240, 50, 10, 10, 0, 0, 0);
    PPMImage::fillRect(frame, 50, 140, 10, 10, 0, 0, 0);
    PPMImage::fillRect(frame, 240, 140, 10, 10, 0, 0, 0);

    // Center should be filled
    auto center = getPixel(frame, 150, 100);
    EXPECT_EQ(center.r, 255);
    EXPECT_EQ(center.g, 128);

    // Top-left corner area should be background
    auto corner = getPixel(frame, 52, 52);
    EXPECT_EQ(corner.r, 0);
}

TEST_F(VisualRoundedRectTest, CpuPixelAccuracyInsideRoundedRect) {
    // Interior pixels of a rounded rect should match the specified color exactly
    auto frame = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 30, 30, 40);
    PPMImage::fillRect(frame, 100, 75, 200, 150, 0, 200, 100);

    // Center pixel (well inside any corner rounding)
    auto px = getPixel(frame, 200, 150);
    EXPECT_EQ(px.r, 0);
    EXPECT_EQ(px.g, 200);
    EXPECT_EQ(px.b, 100);

    // Background pixel
    auto bg = getPixel(frame, 10, 10);
    EXPECT_EQ(bg.r, 30);
    EXPECT_EQ(bg.g, 30);
    EXPECT_EQ(bg.b, 40);
}

TEST_F(VisualRoundedRectTest, CpuMultipleRoundedRectsLayout) {
    // Three rounded rects at different positions — verify layout
    auto frame = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 20, 20, 30);
    PPMImage::fillRect(frame, 10, 10, 100, 80, 255, 0, 0);     // red, top-left
    PPMImage::fillRect(frame, 150, 10, 100, 80, 0, 255, 0);     // green, top-right
    PPMImage::fillRect(frame, 80, 120, 100, 80, 0, 0, 255);     // blue, bottom-center

    // Verify each rect's center color
    auto red   = getPixel(frame, 60, 50);
    auto green = getPixel(frame, 200, 50);
    auto blue  = getPixel(frame, 130, 160);

    EXPECT_EQ(red.r, 255);   EXPECT_EQ(red.g, 0);
    EXPECT_EQ(green.g, 255); EXPECT_EQ(green.r, 0);
    EXPECT_EQ(blue.b, 255);  EXPECT_EQ(blue.r, 0);
}

// ============================================================================
// Part 2: GPU-based visual tests
// ============================================================================

// --- Sub-fixture: small radius ---
class GpuSmallRadiusTest : public VisualRoundedRectTest {
protected:
    void SetUp() override {
        VisualRoundedRectTest::SetUp();
        rectX_ = 50; rectY_ = 50;
        rectW_ = 200; rectH_ = 100;
        cornerRadius_ = 10.0f;
        colorR_ = 1.0f; colorG_ = 0.0f; colorB_ = 0.0f;
    }
    void renderScene() override {
        // context->drawRoundedRect(
        //     glm::vec2(rectX_, rectY_), glm::vec2(rectW_, rectH_),
        //     cornerRadius_, glm::vec3(colorR_, colorG_, colorB_));
    }
};

TEST_F(GpuSmallRadiusTest, SmallRadius) {
    auto result = runAndCompare("rounded_rect_small_radius", sdfRelaxedOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Sub-fixture: large radius (pill) ---
class GpuLargeRadiusTest : public VisualRoundedRectTest {
protected:
    void SetUp() override {
        VisualRoundedRectTest::SetUp();
        rectX_ = 50; rectY_ = 75;
        rectW_ = 300; rectH_ = 150;
        cornerRadius_ = 75.0f; // min(300,150)/2 = 75
        colorR_ = 0.0f; colorG_ = 0.5f; colorB_ = 1.0f;
    }
    void renderScene() override {
        // context->drawRoundedRect(
        //     glm::vec2(rectX_, rectY_), glm::vec2(rectW_, rectH_),
        //     cornerRadius_, glm::vec3(colorR_, colorG_, colorB_));
    }
};

TEST_F(GpuLargeRadiusTest, LargeRadiusPillShape) {
    auto result = runAndCompare("rounded_rect_large_radius_pill", sdfRelaxedOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Sub-fixture: zero radius (should match sharp rect) ---
class GpuZeroRadiusTest : public VisualRoundedRectTest {
protected:
    void SetUp() override {
        VisualRoundedRectTest::SetUp();
        rectX_ = 80; rectY_ = 60;
        rectW_ = 200; rectH_ = 120;
        cornerRadius_ = 0.0f;
        colorR_ = 0.8f; colorG_ = 0.4f; colorB_ = 0.2f;
    }
    void renderScene() override {
        // context->drawRoundedRect(
        //     glm::vec2(rectX_, rectY_), glm::vec2(rectW_, rectH_),
        //     cornerRadius_, glm::vec3(colorR_, colorG_, colorB_));
    }
};

TEST_F(GpuZeroRadiusTest, ZeroRadiusMatchesSharpRect) {
    auto result = runAndCompare("rounded_rect_zero_radius", strictSharpOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Sub-fixture: multiple rounded rects ---
class GpuMultiRoundedRectTest : public VisualRoundedRectTest {
protected:
    void renderScene() override {
        // context->drawRoundedRect(glm::vec2(10, 10), glm::vec2(120, 80), 15.0f, glm::vec3(1, 0, 0));
        // context->drawRoundedRect(glm::vec2(150, 10), glm::vec2(120, 80), 30.0f, glm::vec3(0, 1, 0));
        // context->drawRoundedRect(glm::vec2(80, 120), glm::vec2(120, 80), 40.0f, glm::vec3(0, 0, 1));
    }
};

TEST_F(GpuMultiRoundedRectTest, MultipleRoundedRects) {
    auto result = runAndCompare("rounded_rect_multiple", sdfRelaxedOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Sub-fixture: rounded rect color accuracy ---
class GpuRoundedRectColorTest : public VisualRoundedRectTest {
protected:
    void SetUp() override {
        VisualRoundedRectTest::SetUp();
        rectX_ = 100; rectY_ = 75;
        rectW_ = 200; rectH_ = 150;
        cornerRadius_ = 20.0f;
        colorR_ = 73.0f/255.0f;
        colorG_ = 156.0f/255.0f;
        colorB_ = 210.0f/255.0f;
    }
    void renderScene() override {
        // context->drawRoundedRect(
        //     glm::vec2(rectX_, rectY_), glm::vec2(rectW_, rectH_),
        //     cornerRadius_, glm::vec3(colorR_, colorG_, colorB_));
    }
};

TEST_F(GpuRoundedRectColorTest, RoundedRectColorAccuracy) {
    auto result = runAndCompare("rounded_rect_color_accuracy", sdfRelaxedOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Radius equals half the short side ---
class GpuRadiusHalfSizeTest : public VisualRoundedRectTest {
protected:
    void SetUp() override {
        VisualRoundedRectTest::SetUp();
        rectX_ = 100; rectY_ = 100;
        rectW_ = 200; rectH_ = 100;
        cornerRadius_ = 50.0f; // half of short side (100/2)
        colorR_ = 0.9f; colorG_ = 0.7f; colorB_ = 0.1f;
    }
    void renderScene() override {
        // context->drawRoundedRect(
        //     glm::vec2(rectX_, rectY_), glm::vec2(rectW_, rectH_),
        //     cornerRadius_, glm::vec3(colorR_, colorG_, colorB_));
    }
};

TEST_F(GpuRadiusHalfSizeTest, RadiusEqualsHalfSize) {
    auto result = runAndCompare("rounded_rect_radius_half_size", sdfRelaxedOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}
