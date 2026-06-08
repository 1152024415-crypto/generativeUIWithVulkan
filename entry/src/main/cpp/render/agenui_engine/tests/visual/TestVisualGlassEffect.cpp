/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Level 3 Visual Test — Liquid Glass v2 Effect
 * ==============================================
 *
 * Validates the "Liquid Glass v2" glass effect rendered through
 * simple_rounded_rect.frag against programmatically generated reference
 * images. The glass pipeline performs:
 *
 *   - Refraction with lens distortion (centerScale = 1 - dist^2 * 0.12)
 *   - 9x9 box blur of the background texture
 *   - Chromatic aberration (chromaStr-driven per-channel UV offsets)
 *   - Specular highlight band (position, intensity, width configurable)
 *   - Edge ring lighting with top-bright / bottom-dark vertical gradient
 *   - Configurable soft anti-aliasing via edgeSharpness
 *
 * Because the glass effect involves GPU-dependent blur, refraction, and
 * compositing calculations, all tests use relaxed comparison tolerances:
 *   maxAvgError     = 10.0
 *   maxPixelError   = 30.0
 *   maxMismatchRatio = 0.10  (10%)
 *
 * Canvas size: 400 x 300
 */

#include <gtest/gtest.h>
#include <glm/glm.hpp>

#include "visual/VisualTestFramework.h"
#include "core/RenderContext.h"

using namespace AgenUIEngine::Core;

// ============================================================================
// Tolerance constants — relaxed for GPU-dependent glass effects
// ============================================================================

static constexpr double kGlassMaxAvgError     = 10.0;
static constexpr double kGlassMaxPixelError   = 30.0;
static constexpr double kGlassMaxMismatchRatio = 0.10;

static ImageComparator::Options glassCompareOpts() {
    ImageComparator::Options opts;
    opts.maxAvgError     = kGlassMaxAvgError;
    opts.maxPixelError   = kGlassMaxPixelError;
    opts.maxMismatchRatio = kGlassMaxMismatchRatio;
    opts.generateDiffImage = true;
    opts.diffOutputDir = "./visual_test_diffs/glass/";
    return opts;
}

// Canvas dimensions used by all tests in this file
static constexpr uint32_t kCanvasW = 400;
static constexpr uint32_t kCanvasH = 300;

// ============================================================================
// Fixture — VisualGlassEffectTest
// ============================================================================

/**
 * Visual test fixture for the Liquid Glass v2 effect.
 *
 * Sets up a 400x300 rendering surface and provides convenience wrappers
 * around the RenderContext API for drawing backgrounds and glass rects.
 *
 * Subclasses or individual tests override renderScene() to compose
 * the test scene, then call runAndCompare() with a reference image name.
 */
class VisualGlassEffectTest : public VisualTestBase {
protected:
    void SetUp() override {
        surfaceWidth  = kCanvasW;
        surfaceHeight = kCanvasH;
        VisualTestBase::SetUp();
    }

    /**
     * Helper: fill the entire canvas with a solid color by drawing a
     * full-size background rectangle through RenderContext.
     */
    void drawSolidBackground(uint8_t r, uint8_t g, uint8_t b) {
        float fr = r / 255.0f;
        float fg = g / 255.0f;
        float fb = b / 255.0f;
        // Draw a rectangle covering the full surface area.
        // Coordinates are in the surface coordinate system.
        context->drawRect(
            glm::vec2(0.0f, 0.0f),
            glm::vec2(static_cast<float>(surfaceWidth),
                      static_cast<float>(surfaceHeight)),
            glm::vec3(fr, fg, fb));
    }

    /**
     * Helper: draw a horizontal half-and-half background.
     * Top half filled with color (r1,g1,b1), bottom half with (r2,g2,b2).
     */
    void drawSplitBackground(uint8_t r1, uint8_t g1, uint8_t b1,
                              uint8_t r2, uint8_t g2, uint8_t b2) {
        float halfH = static_cast<float>(surfaceHeight) * 0.5f;
        // Top half
        context->drawRect(
            glm::vec2(0.0f, 0.0f),
            glm::vec2(static_cast<float>(surfaceWidth), halfH),
            glm::vec3(r1 / 255.0f, g1 / 255.0f, b1 / 255.0f));
        // Bottom half
        context->drawRect(
            glm::vec2(0.0f, halfH),
            glm::vec2(static_cast<float>(surfaceWidth), halfH),
            glm::vec3(r2 / 255.0f, g2 / 255.0f, b2 / 255.0f));
    }

    /**
     * Helper: issue the glass pass preparation (copies current framebuffer
     * to the background texture used by simple_rounded_rect.frag for
     * refraction, blur, and chromatic aberration sampling).
     */
    void prepareGlass() {
        context->prepareGlassPass();
    }

    /**
     * Helper: draw a glass rounded rectangle with the given parameters.
     */
    void drawGlassRect(float x, float y, float w, float h,
                       float radius, uint8_t r, uint8_t g, uint8_t b) {
        context->drawGlassRoundedRect(
            glm::vec2(x, y),
            glm::vec2(w, h),
            radius,
            glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f));
    }

    /**
     * Helper: draw a regular (non-glass) rounded rectangle.
     */
    void drawRegularRect(float x, float y, float w, float h,
                          float radius, uint8_t r, uint8_t g, uint8_t b) {
        context->drawRoundedRect(
            glm::vec2(x, y),
            glm::vec2(w, h),
            radius,
            glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f));
    }
};

// ============================================================================
// Test 1: GlassOnSolidBackground
// ============================================================================

/**
 * GlassOnSolidBackground
 *
 * Draws a solid green background (0, 180, 80), then calls prepareGlassPass()
 * to snapshot the framebuffer as the background texture, then draws a glass
 * rounded rectangle tinted white (255, 255, 255) in the center of the canvas.
 *
 * Validation (programmatic):
 *   The glass area should contain a BLURRED MIX of the background color
 *   and the glass tint — it must NOT be identical to either the pure
 *   background green or the pure white tint alone. The glass shader blends
 *   blurred background with 18% tint plus lighting terms, so the result
 *   is a softened, desaturated green.
 *
 *   We compare the captured frame against a reference image that represents
 *   the expected blurred-mix appearance, using relaxed glass tolerances.
 */
class GlassOnSolidBackgroundScene : public VisualGlassEffectTest {
protected:
    void renderScene() override {
        // Step 1: Solid green background
        drawSolidBackground(0, 180, 80);

        // Step 2: Snapshot framebuffer for glass shader sampling
        prepareGlass();

        // Step 3: Glass rect centered on canvas, white tint
        float glassW = 200.0f;
        float glassH = 150.0f;
        float glassX = (kCanvasW - glassW) * 0.5f;
        float glassY = (kCanvasH - glassH) * 0.5f;
        drawGlassRect(glassX, glassY, glassW, glassH, 16.0f,
                      255, 255, 255);
    }
};

TEST_F(GlassOnSolidBackgroundScene, GlassOnSolidBackground) {
    auto result = runAndCompare("glass_on_solid_background", glassCompareOpts());
    EXPECT_TRUE(result.passed)
        << "Glass on solid background comparison failed.\n"
        << "  averageError   = " << result.averageError << " (max " << kGlassMaxAvgError << ")\n"
        << "  mismatchRatio  = " << result.mismatchRatio << " (max " << kGlassMaxMismatchRatio << ")\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels << "\n"
        << "  diffImage       = " << result.diffImagePath;

    // Additional semantic check: the glass center pixel should NOT be
    // the same as the pure background color (0, 180, 80) nor the pure
    // glass tint (255, 255, 255). The blur + tint blend produces a
    // distinct intermediate color.
    auto actual = renderAndCapture();
    if (actual.valid) {
        // Sample a pixel near the center of the glass rect
        uint32_t cx = kCanvasW / 2;
        uint32_t cy = kCanvasH / 2;
        uint32_t idx = (cy * actual.width + cx) * 4;
        uint8_t pr = actual.pixels[idx + 0];
        uint8_t pg = actual.pixels[idx + 1];
        uint8_t pb = actual.pixels[idx + 2];

        // The glass area should differ from pure background green
        int bgDist = std::abs(static_cast<int>(pr) - 0)
                   + std::abs(static_cast<int>(pg) - 180)
                   + std::abs(static_cast<int>(pb) - 80);
        EXPECT_GT(bgDist, 20)
            << "Glass center pixel (" << (int)pr << "," << (int)pg << "," << (int)pb
            << ") is too close to background (0,180,80); "
            << "glass effect may not be applying.";

        // The glass area should also differ from pure white tint
        int tintDist = std::abs(static_cast<int>(pr) - 255)
                     + std::abs(static_cast<int>(pg) - 255)
                     + std::abs(static_cast<int>(pb) - 255);
        EXPECT_GT(tintDist, 20)
            << "Glass center pixel (" << (int)pr << "," << (int)pg << "," << (int)pb
            << ") is too close to tint white (255,255,255); "
            << "glass should be a blurred mix, not opaque tint.";
    }
}

// ============================================================================
// Test 2: GlassTransparency
// ============================================================================

/**
 * GlassTransparency
 *
 * Draws a bright magenta background (255, 0, 255), then a glass rect
 * tinted a neutral gray (128, 128, 128) on top. The glass shader
 * composites via: alpha = shape * glassAlpha, where glassAlpha < 1.0
 * in typical usage. Therefore the background magenta should bleed
 * through the glass area.
 *
 * Validation:
 *   The glass area is NOT fully opaque — the rendered color should be
 *   a blend of blurred magenta and the gray tint, meaning the magenta
 *   component is clearly visible but modified. A fully opaque glass
 *   would produce a uniform gray, which we check does not happen.
 */
class GlassTransparencyScene : public VisualGlassEffectTest {
protected:
    void renderScene() override {
        // Bright magenta background
        drawSolidBackground(255, 0, 255);

        prepareGlass();

        // Glass rect with neutral gray tint, covering most of the canvas
        drawGlassRect(50.0f, 50.0f, 300.0f, 200.0f, 20.0f,
                      128, 128, 128);
    }
};

TEST_F(GlassTransparencyScene, GlassTransparency) {
    auto result = runAndCompare("glass_transparency", glassCompareOpts());
    EXPECT_TRUE(result.passed)
        << "Glass transparency comparison failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio  = " << result.mismatchRatio << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels << "\n"
        << "  diffImage       = " << result.diffImagePath;

    // Semantic: background color should bleed through — the glass area
    // must contain some red and blue channel energy from the magenta
    // background rather than being purely the gray tint color.
    auto actual = renderAndCapture();
    if (actual.valid) {
        uint32_t cx = kCanvasW / 2;
        uint32_t cy = kCanvasH / 2;
        uint32_t idx = (cy * actual.width + cx) * 4;
        uint8_t pr = actual.pixels[idx + 0];
        uint8_t pg = actual.pixels[idx + 1];
        uint8_t pb = actual.pixels[idx + 2];

        // Magenta has R=255, B=255. If the glass were fully opaque gray,
        // R and B would both be ~128. Transparency means R and B should
        // be noticeably above 128 (background bleeding through).
        // Allow some margin for blur/tint effects.
        bool hasBackgroundBleed =
            (static_cast<int>(pr) > 140) || (static_cast<int>(pb) > 140);
        EXPECT_TRUE(hasBackgroundBleed)
            << "Glass appears opaque at center pixel ("
            << (int)pr << "," << (int)pg << "," << (int)pb
            << "). Expected background magenta to bleed through "
            << "(R and B channels should be elevated above 140).";

        // The pixel should also differ from pure gray (128,128,128)
        int grayDist = std::abs(static_cast<int>(pr) - 128)
                     + std::abs(static_cast<int>(pg) - 128)
                     + std::abs(static_cast<int>(pb) - 128);
        EXPECT_GT(grayDist, 10)
            << "Glass center pixel is too close to opaque gray (128,128,128); "
            << "background color should influence the result.";
    }
}

// ============================================================================
// Test 3: GlassOnGradientBackground
// ============================================================================

/**
 * GlassOnGradientBackground
 *
 * The canvas is split horizontally: top half red (255, 40, 40),
 * bottom half blue (40, 40, 255). A glass rect is placed in the
 * center, straddling the boundary between the two halves.
 *
 * The 9x9 box blur in the glass shader should sample texels from
 * both the red and blue regions, producing a blended purple-ish tone
 * in the glass area rather than a single solid color.
 *
 * Validation:
 *   Sample pixels from the upper and lower portions of the glass area.
 *   The upper portion should show more red influence, the lower portion
 *   more blue influence. Both should contain non-trivial amounts of
 *   the opposite color due to the blur kernel mixing them.
 */
class GlassOnGradientBackgroundScene : public VisualGlassEffectTest {
protected:
    void renderScene() override {
        // Top half red, bottom half blue
        drawSplitBackground(255, 40, 40,   // top
                            40, 40, 255);  // bottom

        prepareGlass();

        // Glass rect straddling the center boundary
        float glassW = 240.0f;
        float glassH = 180.0f;
        float glassX = (kCanvasW - glassW) * 0.5f;
        float glassY = (kCanvasH - glassH) * 0.5f;
        drawGlassRect(glassX, glassY, glassW, glassH, 18.0f,
                      220, 220, 220);
    }
};

TEST_F(GlassOnGradientBackgroundScene, GlassOnGradientBackground) {
    auto result = runAndCompare("glass_on_gradient_background", glassCompareOpts());
    EXPECT_TRUE(result.passed)
        << "Glass on gradient background comparison failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio  = " << result.mismatchRatio << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels << "\n"
        << "  diffImage       = " << result.diffImagePath;

    // Semantic: the glass area should show a mix of both background colors.
    // Sample in the upper third and lower third of the glass rect.
    auto actual = renderAndCapture();
    if (actual.valid) {
        // Glass rect is centered at (200, 150), size (240, 180)
        // Upper sample: inside glass, but in the red background zone
        uint32_t upperX = kCanvasW / 2;
        uint32_t upperY = kCanvasH / 2 - 40; // above center
        uint32_t lowerX = kCanvasW / 2;
        uint32_t lowerY = kCanvasH / 2 + 40; // below center

        uint32_t idxUpper = (upperY * actual.width + upperX) * 4;
        uint32_t idxLower = (lowerY * actual.width + lowerX) * 4;

        uint8_t ur = actual.pixels[idxUpper + 0];
        uint8_t ub = actual.pixels[idxUpper + 2];
        uint8_t lr = actual.pixels[idxLower + 0];
        uint8_t lb = actual.pixels[idxLower + 2];

        // Upper sample should have more red than blue (red zone influence)
        EXPECT_GT(static_cast<int>(ur), static_cast<int>(ub) + 20)
            << "Upper glass area should show red background dominance. "
            << "Got R=" << (int)ur << " B=" << (int)ub;

        // Lower sample should have more blue than red (blue zone influence)
        EXPECT_GT(static_cast<int>(lb), static_cast<int>(lr) + 20)
            << "Lower glass area should show blue background dominance. "
            << "Got R=" << (int)lr << " B=" << (int)lb;

        // Both samples should have some mix — upper should have nonzero blue,
        // lower should have nonzero red, due to the blur mixing both halves.
        EXPECT_GT(static_cast<int>(ub), 15)
            << "Upper glass area should contain some blue from blur mixing. "
            << "Got B=" << (int)ub;
        EXPECT_GT(static_cast<int>(lr), 15)
            << "Lower glass area should contain some red from blur mixing. "
            << "Got R=" << (int)lr;
    }
}

// ============================================================================
// Test 4: GlassVsNonGlass
// ============================================================================

/**
 * GlassVsNonGlass
 *
 * Draws two rounded rectangles side by side on a solid orange background
 * (255, 140, 20):
 *   - Left:  regular (non-glass) rounded rect, tinted light gray (200,200,200)
 *   - Right: glass rounded rect, same tint color (200,200,200)
 *
 * The two rects should look visibly different: the regular rect is a flat
 * opaque fill, while the glass rect shows blur, refraction, edge lighting,
 * and specular highlights over the background.
 *
 * Validation:
 *   After rendering, sample pixels from the center of each rect and
 *   verify that the glass rect pixel differs significantly from the
 *   regular rect pixel. They share the same tint color and background,
 *   so any difference is attributable to the glass effect pipeline.
 */
class GlassVsNonGlassScene : public VisualGlassEffectTest {
protected:
    void renderScene() override {
        // Solid orange background
        drawSolidBackground(255, 140, 20);

        prepareGlass();

        float rectW = 150.0f;
        float rectH = 120.0f;
        float gap = 20.0f;
        float totalW = rectW * 2.0f + gap;
        float startX = (kCanvasW - totalW) * 0.5f;
        float startY = (kCanvasH - rectH) * 0.5f;

        // Left: regular rounded rect (flat fill)
        drawRegularRect(startX, startY, rectW, rectH, 14.0f,
                        200, 200, 200);

        // Right: glass rounded rect (same tint, same shape)
        float rightX = startX + rectW + gap;
        drawGlassRect(rightX, startY, rectW, rectH, 14.0f,
                      200, 200, 200);
    }
};

TEST_F(GlassVsNonGlassScene, GlassVsNonGlass) {
    auto result = runAndCompare("glass_vs_non_glass", glassCompareOpts());
    EXPECT_TRUE(result.passed)
        << "Glass vs non-glass comparison failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio  = " << result.mismatchRatio << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels << "\n"
        << "  diffImage       = " << result.diffImagePath;

    // Semantic: the two rects should not look the same.
    // The regular rect should be flat (200,200,200) tint.
    // The glass rect should be a modified version due to blur/lighting.
    auto actual = renderAndCapture();
    if (actual.valid) {
        // Rect dimensions from renderScene
        float rectW = 150.0f, rectH = 120.0f, gap = 20.0f;
        float totalW = rectW * 2.0f + gap;
        float startX = (kCanvasW - totalW) * 0.5f;
        float startY = (kCanvasH - rectH) * 0.5f;

        // Sample center of left (regular) rect
        uint32_t leftCx = static_cast<uint32_t>(startX + rectW * 0.5f);
        uint32_t leftCy = static_cast<uint32_t>(startY + rectH * 0.5f);
        uint32_t idxLeft = (leftCy * actual.width + leftCx) * 4;

        // Sample center of right (glass) rect
        float rightX = startX + rectW + gap;
        uint32_t rightCx = static_cast<uint32_t>(rightX + rectW * 0.5f);
        uint32_t rightCy = static_cast<uint32_t>(startY + rectH * 0.5f);
        uint32_t idxRight = (rightCy * actual.width + rightCx) * 4;

        int leftR  = actual.pixels[idxLeft + 0];
        int leftG  = actual.pixels[idxLeft + 1];
        int leftB  = actual.pixels[idxLeft + 2];
        int rightR = actual.pixels[idxRight + 0];
        int rightG = actual.pixels[idxRight + 1];
        int rightB = actual.pixels[idxRight + 2];

        int colorDiff = std::abs(leftR - rightR)
                      + std::abs(leftG - rightG)
                      + std::abs(leftB - rightB);

        // The glass effect produces blur + tint blend + specular + edge
        // lighting, so the pixel colors should differ noticeably from
        // the flat opaque fill of the regular rect.
        EXPECT_GT(colorDiff, 15)
            << "Glass and regular rects look too similar. "
            << "Regular center: (" << leftR << "," << leftG << "," << leftB << "), "
            << "Glass center: (" << rightR << "," << rightG << "," << rightB << "), "
            << "total channel diff = " << colorDiff << ". "
            << "The glass effect should produce a visible difference.";
    }
}

// ============================================================================
// Test 5: GlassAlphaBlending
// ============================================================================

/**
 * GlassAlphaBlending
 *
 * Draws the same glass rect twice on a solid teal background (0, 160, 160),
 * but with different glassAlpha values (set via the shader's glassAlpha
 * push constant). The two rects are placed in different horizontal bands
 * so they do not overlap.
 *
 * Validation:
 *   The two glass rects produce different pixel colors because different
 *   alpha values result in different blending ratios between the blurred
 *   background and the glass lighting composite. A higher alpha makes
 *   the glass tint and lighting more prominent; lower alpha lets more
 *   raw background through.
 *
 *   Since the RenderContext API draws glass rects with a single unified
 *   glassAlpha (embedded in the push constant block), we test by drawing
 *   two glass rects with different tint colors that simulate the effect
 *   of different alpha levels. Alternatively, if the engine supports
 *   per-rect glassAlpha configuration, we test that directly.
 *
 *   In this test we use two different tint colors as a proxy for different
 *   visual results — the key assertion is that two glass rects on the same
 *   background with different tint colors produce distinguishable outputs.
 *   The tint is blended at 18% into the blurred background, so changing
 *   the tint must change the final color.
 */
class GlassAlphaBlendingScene : public VisualGlassEffectTest {
protected:
    void renderScene() override {
        // Solid teal background
        drawSolidBackground(0, 160, 160);

        prepareGlass();

        // Upper glass rect: white tint (simulates high alpha / more tint)
        drawGlassRect(80.0f, 20.0f, 240.0f, 110.0f, 12.0f,
                      255, 255, 255);

        // Lower glass rect: dark tint (simulates low alpha / less tint)
        drawGlassRect(80.0f, 160.0f, 240.0f, 110.0f, 12.0f,
                      40, 40, 60);
    }
};

TEST_F(GlassAlphaBlendingScene, GlassAlphaBlending) {
    auto result = runAndCompare("glass_alpha_blending", glassCompareOpts());
    EXPECT_TRUE(result.passed)
        << "Glass alpha blending comparison failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio  = " << result.mismatchRatio << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels << "\n"
        << "  diffImage       = " << result.diffImagePath;

    // Semantic: the two glass rects should produce different colors.
    // Even though both sample the same blurred background, the tint
    // blend (18% mix with the tint color) means a white-tinted glass
    // will be brighter than a dark-tinted glass.
    auto actual = renderAndCapture();
    if (actual.valid) {
        // Sample center of upper glass rect (white tint)
        uint32_t upperCx = 200; // center x of rect at x=80, w=240
        uint32_t upperCy = 75;  // center y of rect at y=20, h=110
        uint32_t idxUpper = (upperCy * actual.width + upperCx) * 4;

        // Sample center of lower glass rect (dark tint)
        uint32_t lowerCx = 200;
        uint32_t lowerCy = 215; // center y of rect at y=160, h=110
        uint32_t idxLower = (lowerCy * actual.width + lowerCx) * 4;

        int upperR = actual.pixels[idxUpper + 0];
        int upperG = actual.pixels[idxUpper + 1];
        int upperB = actual.pixels[idxUpper + 2];
        int lowerR = actual.pixels[idxLower + 0];
        int lowerG = actual.pixels[idxLower + 1];
        int lowerB = actual.pixels[idxLower + 2];

        // The white-tinted glass should be brighter than the dark-tinted one.
        int upperLum = upperR + upperG + upperB;
        int lowerLum = lowerR + lowerG + lowerB;

        EXPECT_GT(upperLum, lowerLum + 10)
            << "White-tinted glass should be brighter than dark-tinted glass. "
            << "Upper (white tint): (" << upperR << "," << upperG << "," << upperB
            << ") lum=" << upperLum << ", "
            << "Lower (dark tint): (" << lowerR << "," << lowerG << "," << lowerB
            << ") lum=" << lowerLum << ". "
            << "Different glass tint colors should produce different results.";
    }
}

// ============================================================================
// Test 6: MultipleGlassRects
// ============================================================================

/**
 * MultipleGlassRects
 *
 * Draws four glass rounded rectangles on the same dark blue background
 * (20, 30, 80), each with a different tint color:
 *   - Top-left:     warm yellow (255, 220, 100)
 *   - Top-right:    cool cyan   (100, 220, 255)
 *   - Bottom-left:  soft pink   (255, 140, 180)
 *   - Bottom-right: fresh green (140, 255, 180)
 *
 * Each rect is drawn after a single prepareGlassPass(), which captures
 * the current framebuffer state. The shader samples from the bgTexture
 * for each rect independently.
 *
 * Validation:
 *   All four glass rects should be distinguishable from one another
 *   (different tint colors produce visibly different composites).
 *   Additionally, each glass rect should differ from the pure background
 *   color, confirming that the glass effect is active for all rects.
 *
 * The overall frame is compared against a reference image with relaxed
 * glass tolerances.
 */
class MultipleGlassRectsScene : public VisualGlassEffectTest {
protected:
    void renderScene() override {
        // Dark blue background
        drawSolidBackground(20, 30, 80);

        prepareGlass();

        float rectW = 160.0f;
        float rectH = 110.0f;
        float margin = 20.0f;

        // Compute layout: 2x2 grid of glass rects
        float totalGridW = rectW * 2.0f + margin;
        float totalGridH = rectH * 2.0f + margin;
        float originX = (kCanvasW - totalGridW) * 0.5f;
        float originY = (kCanvasH - totalGridH) * 0.5f;

        // Top-left: warm yellow
        drawGlassRect(originX, originY, rectW, rectH, 12.0f,
                      255, 220, 100);

        // Top-right: cool cyan
        drawGlassRect(originX + rectW + margin, originY, rectW, rectH, 12.0f,
                      100, 220, 255);

        // Bottom-left: soft pink
        drawGlassRect(originX, originY + rectH + margin, rectW, rectH, 12.0f,
                      255, 140, 180);

        // Bottom-right: fresh green
        drawGlassRect(originX + rectW + margin, originY + rectH + margin,
                      rectW, rectH, 12.0f,
                      140, 255, 180);
    }
};

TEST_F(MultipleGlassRectsScene, MultipleGlassRects) {
    auto result = runAndCompare("multiple_glass_rects", glassCompareOpts());
    EXPECT_TRUE(result.passed)
        << "Multiple glass rects comparison failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio  = " << result.mismatchRatio << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels << "\n"
        << "  diffImage       = " << result.diffImagePath;

    // Semantic: each glass rect should differ from the background and
    // from each other (different tint colors produce different blends).
    auto actual = renderAndCapture();
    if (actual.valid) {
        float rectW = 160.0f, rectH = 110.0f, margin = 20.0f;
        float totalGridW = rectW * 2.0f + margin;
        float totalGridH = rectH * 2.0f + margin;
        float originX = (kCanvasW - totalGridW) * 0.5f;
        float originY = (kCanvasH - totalGridH) * 0.5f;

        // Sample centers of each rect
        struct Sample { uint32_t x, y; const char* name; };
        Sample samples[4] = {
            { static_cast<uint32_t>(originX + rectW * 0.5f),
              static_cast<uint32_t>(originY + rectH * 0.5f),
              "top-left (yellow)" },
            { static_cast<uint32_t>(originX + rectW + margin + rectW * 0.5f),
              static_cast<uint32_t>(originY + rectH * 0.5f),
              "top-right (cyan)" },
            { static_cast<uint32_t>(originX + rectW * 0.5f),
              static_cast<uint32_t>(originY + rectH + margin + rectH * 0.5f),
              "bottom-left (pink)" },
            { static_cast<uint32_t>(originX + rectW + margin + rectW * 0.5f),
              static_cast<uint32_t>(originY + rectH + margin + rectH * 0.5f),
              "bottom-right (green)" },
        };

        int lum[4];
        for (int i = 0; i < 4; ++i) {
            uint32_t idx = (samples[i].y * actual.width + samples[i].x) * 4;
            int r = actual.pixels[idx + 0];
            int g = actual.pixels[idx + 1];
            int b = actual.pixels[idx + 2];
            lum[i] = r + g + b;

            // Each rect should differ from the dark blue background (20,30,80)
            int bgDist = std::abs(r - 20) + std::abs(g - 30) + std::abs(b - 80);
            EXPECT_GT(bgDist, 20)
                << samples[i].name << " rect center (" << r << "," << g << ","
                << b << ") is too close to background (20,30,80); "
                << "glass effect may not be active.";
        }

        // Check that adjacent rects with different tints produce different
        // luminance values (the tint blend at 18% creates a visible difference).
        // We check at least the top-left vs top-right pair.
        int lumDiff_01 = std::abs(lum[0] - lum[1]);
        EXPECT_GT(lumDiff_01, 5)
            << "Top-left (yellow tint, lum=" << lum[0]
            << ") and top-right (cyan tint, lum=" << lum[1]
            << ") glass rects are too similar. "
            << "Different tint colors should produce different results.";

        // And bottom-left vs bottom-right
        int lumDiff_23 = std::abs(lum[2] - lum[3]);
        EXPECT_GT(lumDiff_23, 5)
            << "Bottom-left (pink tint, lum=" << lum[2]
            << ") and bottom-right (green tint, lum=" << lum[3]
            << ") glass rects are too similar. "
            << "Different tint colors should produce different results.";
    }
}

// ============================================================================
// Test 7: GlassEffectDoesNotCorruptBackground
// ============================================================================

/**
 * GlassEffectDoesNotCorruptBackground
 *
 * A supplementary integrity test: draws a solid orange background, then a
 * glass rect in the center. The pixels OUTSIDE the glass rect should remain
 * exactly (or very close to) the original background color, confirming the
 * glass shader does not corrupt pixels outside its shape boundary.
 *
 * The shader uses `transition < 0.001` guard to pass through raw bgTexture
 * for outside pixels, so the background should be preserved.
 */
class GlassBackgroundIntegrityScene : public VisualGlassEffectTest {
protected:
    void renderScene() override {
        drawSolidBackground(255, 160, 40);

        prepareGlass();

        // Small glass rect in the center, leaving large background areas
        drawGlassRect(150.0f, 100.0f, 100.0f, 100.0f, 12.0f,
                      200, 200, 200);
    }
};

TEST_F(GlassBackgroundIntegrityScene, GlassEffectDoesNotCorruptBackground) {
    auto result = runAndCompare("glass_background_integrity", glassCompareOpts());
    EXPECT_TRUE(result.passed)
        << "Glass background integrity comparison failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio  = " << result.mismatchRatio << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels << "\n"
        << "  diffImage       = " << result.diffImagePath;

    // Semantic: pixels well outside the glass rect should match the
    // original background color (255, 160, 40) very closely.
    auto actual = renderAndCapture();
    if (actual.valid) {
        // Sample four corner regions that are clearly outside the glass rect
        // Glass rect: x=[150,250], y=[100,200]
        // Safe sample points: corners of the canvas
        struct CornerSample {
            uint32_t x, y;
            const char* name;
        };
        CornerSample corners[] = {
            { 10, 10, "top-left" },
            { kCanvasW - 11, 10, "top-right" },
            { 10, kCanvasH - 11, "bottom-left" },
            { kCanvasW - 11, kCanvasH - 11, "bottom-right" },
        };

        for (const auto& c : corners) {
            uint32_t idx = (c.y * actual.width + c.x) * 4;
            int pr = actual.pixels[idx + 0];
            int pg = actual.pixels[idx + 1];
            int pb = actual.pixels[idx + 2];

            // Background is (255, 160, 40). Allow small tolerance for
            // potential swapchain format conversion artifacts.
            int bgDist = std::abs(pr - 255) + std::abs(pg - 160) + std::abs(pb - 40);
            EXPECT_LT(bgDist, 15)
                << c.name << " corner pixel (" << pr << "," << pg << ","
                << pb << ") deviates from expected background (255,160,40). "
                << "Distance = " << bgDist << ". "
                << "Glass effect should not corrupt pixels outside its bounds.";
        }
    }
}
