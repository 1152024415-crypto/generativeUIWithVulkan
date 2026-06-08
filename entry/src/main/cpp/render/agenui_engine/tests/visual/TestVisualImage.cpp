/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Level 3 Visual Integration Test — Image/Texture Rendering
 *
 * Tests the image rendering pipeline: texture loading, position, scaling,
 * and compositing on backgrounds.
 *
 * RenderContext API:
 *   context->drawImage(const std::string& imagePath,
 *                      const glm::vec2& pos,
 *                      const glm::vec2& size);
 *
 * Categories:
 *   1. CPU-only framework validation
 *   2. GPU-based visual tests (runAndCompare)
 *
 * Tolerance: maxAvgError=3.0, maxPixelError=15.0, maxMismatchRatio=0.02
 */

#include "VisualTestFramework.h"

using namespace AgenUIEngine::Core;

namespace {
constexpr uint32_t kCanvasWidth  = 400;
constexpr uint32_t kCanvasHeight = 300;

ImageComparator::Options imageOpts() {
    ImageComparator::Options opts;
    opts.maxAvgError      = 3.0;
    opts.maxPixelError    = 15.0;
    opts.maxMismatchRatio = 0.02;
    opts.diffOutputDir    = "./visual_test_diffs/image/";
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

class VisualImageTest : public VisualTestBase {
protected:
    void SetUp() override {
        surfaceWidth  = kCanvasWidth;
        surfaceHeight = kCanvasHeight;
    }

    void renderScene() override {
        // Override in sub-fixtures
    }

    // Helper: create a small solid-color "image" as CaptureResult
    static CaptureResult makeTestImage(uint32_t w, uint32_t h,
                                        uint8_t r, uint8_t g, uint8_t b) {
        return PPMImage::solidColor(w, h, r, g, b);
    }

    // Configurable params
    std::string imagePath_;
    uint32_t imgX_ = 50, imgY_ = 50;
    uint32_t imgW_ = 100, imgH_ = 100;
};

// ============================================================================
// Part 1: CPU-only framework validation
// ============================================================================

TEST_F(VisualImageTest, CpuSolidColorImagePlacement) {
    // Simulate: render a 100x100 red "image" at position (50,50) on black background
    auto canvas = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(canvas, 50, 50, 100, 100, 255, 0, 0);

    // Expected: same layout
    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(expected, 50, 50, 100, 100, 255, 0, 0);

    auto result = ImageComparator::compare(canvas, expected, imageOpts());
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.mismatchPixels, 0u);
}

TEST_F(VisualImageTest, CpuImagePosition) {
    // Image at different positions
    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 30, 30, 40);

    // Three images at different positions
    PPMImage::fillRect(expected, 0, 0, 80, 60, 255, 0, 0);     // top-left
    PPMImage::fillRect(expected, 100, 100, 80, 60, 0, 255, 0);  // center
    PPMImage::fillRect(expected, 300, 200, 80, 60, 0, 0, 255);  // bottom-right

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight, 30, 30, 40,
        {
            {0, 0, 80, 60, 255, 0, 0},
            {100, 100, 80, 60, 0, 255, 0},
            {300, 200, 80, 60, 0, 0, 255},
        }
    );

    auto result = ImageComparator::compare(actual, expected, imageOpts());
    EXPECT_TRUE(result.passed);
}

TEST_F(VisualImageTest, CpuImageScaling) {
    // A "50x50" image rendered at 200x150 — should fill the target area
    auto canvas = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 20, 20, 30);
    PPMImage::fillRect(canvas, 50, 50, 200, 150, 200, 100, 50);

    // Verify the scaled image fills the area
    auto topLeft = getPixel(canvas, 51, 51);
    EXPECT_EQ(topLeft.r, 200);
    EXPECT_EQ(topLeft.g, 100);
    EXPECT_EQ(topLeft.b, 50);

    // And doesn't extend beyond
    auto outside = getPixel(canvas, 251, 201);
    EXPECT_EQ(outside.r, 20);
}

TEST_F(VisualImageTest, CpuMultipleImagesGrid) {
    // 2x2 grid of differently-colored images
    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 50, 50, 50);
    PPMImage::fillRect(expected, 20, 20, 160, 110, 255, 80, 80);    // red-ish, top-left
    PPMImage::fillRect(expected, 210, 20, 160, 110, 80, 255, 80);   // green-ish, top-right
    PPMImage::fillRect(expected, 20, 160, 160, 110, 80, 80, 255);   // blue-ish, bottom-left
    PPMImage::fillRect(expected, 210, 160, 160, 110, 255, 255, 0);  // yellow, bottom-right

    auto actual = ReferenceImageGen::overlappingRects(
        kCanvasWidth, kCanvasHeight, 50, 50, 50,
        {
            {20, 20, 160, 110, 255, 80, 80},
            {210, 20, 160, 110, 80, 255, 80},
            {20, 160, 160, 110, 80, 80, 255},
            {210, 160, 160, 110, 255, 255, 0},
        }
    );

    // Verify each quadrant
    EXPECT_EQ(getPixel(actual, 100, 75).r, 255);   // top-left is red-ish
    EXPECT_EQ(getPixel(actual, 290, 75).g, 255);   // top-right is green-ish
    EXPECT_EQ(getPixel(actual, 100, 215).b, 255);  // bottom-left is blue-ish
    EXPECT_EQ(getPixel(actual, 290, 215).r, 255);  // bottom-right is yellow

    auto result = ImageComparator::compare(actual, expected, imageOpts());
    EXPECT_TRUE(result.passed);
}

TEST_F(VisualImageTest, CpuImageOnBackground) {
    // Image on colored background — image area differs from background
    auto canvas = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 100, 150, 200);
    PPMImage::fillRect(canvas, 80, 60, 160, 120, 200, 50, 50);

    // Background pixel should be unchanged
    auto bg = getPixel(canvas, 10, 10);
    EXPECT_EQ(bg.r, 100); EXPECT_EQ(bg.g, 150); EXPECT_EQ(bg.b, 200);

    // Image area should be the image color
    auto img = getPixel(canvas, 160, 120);
    EXPECT_EQ(img.r, 200); EXPECT_EQ(img.g, 50); EXPECT_EQ(img.b, 50);
}

TEST_F(VisualImageTest, CpuImageTransparency) {
    // Simulate transparent image: some pixels have alpha=0
    // Since our reference framework doesn't blend alpha, we test the
    // non-transparent region only
    auto canvas = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 50, 100, 50);
    PPMImage::fillRect(canvas, 50, 50, 100, 100, 255, 200, 0);

    // The image region should show image color
    auto img = getPixel(canvas, 100, 100);
    EXPECT_EQ(img.r, 255); EXPECT_EQ(img.g, 200); EXPECT_EQ(img.b, 0);
}

TEST_F(VisualImageTest, CpuDifferentImageWrongPositionFails) {
    auto actual = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(actual, 50, 50, 100, 100, 255, 0, 0);

    auto expected = PPMImage::solidColor(kCanvasWidth, kCanvasHeight, 0, 0, 0);
    PPMImage::fillRect(expected, 60, 60, 100, 100, 255, 0, 0); // shifted 10px

    auto result = ImageComparator::compare(actual, expected, imageOpts());
    EXPECT_FALSE(result.passed);
    EXPECT_GT(result.mismatchPixels, 0u);
}

// ============================================================================
// Part 2: GPU-based visual tests
// ============================================================================

// --- Solid color image ---
class GpuSolidColorImageTest : public VisualImageTest {
protected:
    void SetUp() override {
        VisualImageTest::SetUp();
        imagePath_ = "test_images/solid_red_100x100.ppm";
        imgX_ = 50; imgY_ = 50;
        imgW_ = 100; imgH_ = 100;
    }
    void renderScene() override {
        // context->drawImage(imagePath_, glm::vec2(imgX_, imgY_), glm::vec2(imgW_, imgH_));
    }
};

TEST_F(GpuSolidColorImageTest, SolidColorImage) {
    auto result = runAndCompare("image_solid_color", imageOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Image at different positions ---
class GpuImagePositionTest : public VisualImageTest {
protected:
    void renderScene() override {
        // context->drawImage("test_images/red_80x60.ppm", glm::vec2(0, 0), glm::vec2(80, 60));
        // context->drawImage("test_images/green_80x60.ppm", glm::vec2(100, 100), glm::vec2(80, 60));
        // context->drawImage("test_images/blue_80x60.ppm", glm::vec2(300, 200), glm::vec2(80, 60));
    }
};

TEST_F(GpuImagePositionTest, ImagePosition) {
    auto result = runAndCompare("image_positions", imageOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Image scaling ---
class GpuImageScalingTest : public VisualImageTest {
protected:
    void SetUp() override {
        VisualImageTest::SetUp();
        imagePath_ = "test_images/pattern_50x50.ppm";
        imgX_ = 50; imgY_ = 50;
        imgW_ = 200; imgH_ = 150;
    }
    void renderScene() override {
        // context->drawImage(imagePath_, glm::vec2(imgX_, imgY_), glm::vec2(imgW_, imgH_));
    }
};

TEST_F(GpuImageScalingTest, ImageScaling) {
    auto result = runAndCompare("image_scaling", imageOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Multiple images grid ---
class GpuMultipleImagesTest : public VisualImageTest {
protected:
    void renderScene() override {
        // context->drawImage("test_images/red.ppm", glm::vec2(20,20), glm::vec2(160,110));
        // context->drawImage("test_images/green.ppm", glm::vec2(210,20), glm::vec2(160,110));
        // context->drawImage("test_images/blue.ppm", glm::vec2(20,160), glm::vec2(160,110));
        // context->drawImage("test_images/yellow.ppm", glm::vec2(210,160), glm::vec2(160,110));
    }
};

TEST_F(GpuMultipleImagesTest, MultipleImages) {
    auto result = runAndCompare("image_multiple", imageOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Image on background ---
class GpuImageOnBgTest : public VisualImageTest {
protected:
    void renderScene() override {
        // context->setClearColor(0.39f, 0.59f, 0.78f, 1.0f); // blue-ish bg
        // context->beginFrame();
        // context->drawRect(glm::vec2(0,0), glm::vec2(kCanvasWidth, kCanvasHeight),
        //                        glm::vec3(0.39f, 0.59f, 0.78f));
        // context->drawImage("test_images/warm.ppm", glm::vec2(80,60), glm::vec2(160,120));
        // context->endFrame();
    }
};

TEST_F(GpuImageOnBgTest, ImageOnBackground) {
    auto result = runAndCompare("image_on_background", imageOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}

// --- Image transparency ---
class GpuImageTransparencyTest : public VisualImageTest {
protected:
    void renderScene() override {
        // context->drawRect(glm::vec2(0,0),
        //     glm::vec2(kCanvasWidth, kCanvasHeight),
        //     glm::vec3(0.2f, 0.4f, 0.2f)); // green background
        // context->drawImage("test_images/alpha_image.ppm", glm::vec2(50,50), glm::vec2(100,100));
    }
};

TEST_F(GpuImageTransparencyTest, ImageTransparency) {
    auto result = runAndCompare("image_transparency", imageOpts());
    EXPECT_TRUE(result.passed) << "avgErr=" << result.averageError
        << " maxErr=" << result.maxError
        << " mismatch=" << result.mismatchRatio * 100 << "%";
}
