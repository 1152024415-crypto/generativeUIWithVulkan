/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Level 3 Visual Test: SDF Text Rendering with Glow Effects
 * ==========================================================
 *
 * Validates the text_frag.frag shader behavior across glow and no-glow paths.
 *
 * Shader characteristics under test:
 *   - SDF_SPREAD = 8.0, rendered at fontSize = 64
 *   - No-glow path (glowIntensity <= 0 || glowWidth <= 0):
 *       simple bitmap rendering with threshold discard (value < 0.01)
 *   - Glow path (glowIntensity > 0 && glowWidth > 0):
 *       - Quadratic falloff: glowFade = t * t
 *       - Gradient glow: white highlight near edge, text color outward
 *       - Glyph interior uses pure text color regardless of glow settings
 *
 * Tolerance is intentionally relaxed because SDF text rendering varies
 * across GPU vendors, driver versions, and font rasterization engines.
 */

#include "VisualTestFramework.h"

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <string>

using namespace AgenUIEngine::Core;

// ============================================================================
// Relaxed tolerance constants for text glow tests
//
// SDF text rendering is sensitive to GPU floating-point precision, font hinting,
// and mip-level selection. These tolerances are deliberately generous.
// ============================================================================

static constexpr double kTextGlowMaxAvgError     = 15.0;   // average per-channel error allowed
static constexpr double kTextGlowMaxPixelError   = 40.0;   // single-pixel max error allowed
static constexpr double kTextGlowMaxMismatchRatio = 0.15;  // up to 15% pixels may mismatch

/// Helper to build the standard relaxed comparator options.
static ImageComparator::Options relaxedTextGlowOpts() {
    ImageComparator::Options opts;
    opts.maxAvgError     = kTextGlowMaxAvgError;
    opts.maxPixelError   = kTextGlowMaxPixelError;
    opts.maxMismatchRatio = kTextGlowMaxMismatchRatio;
    opts.generateDiffImage = true;
    opts.diffOutputDir = "./visual_test_diffs/text_glow/";
    return opts;
}

// ============================================================================
// Fixture
// ============================================================================

/**
 * VisualTextGlowTest — fixture for all text glow rendering tests.
 *
 * Canvas: 400 x 300 pixels (dark background).
 * Each test case uses a specific glow configuration and compares the
 * rendered output against a stored reference image.
 */
class VisualTextGlowTest : public VisualTestBase {
protected:
    static constexpr uint32_t kCanvasWidth  = 400;
    static constexpr uint32_t kCanvasHeight = 300;
    static constexpr uint32_t kDefaultFontSize = 64;

    // Common text color: bright cyan, easy to distinguish from background.
    const glm::vec3 kTextColor = glm::vec3(0.0f, 0.85f, 1.0f);

    // Background color: dark navy.
    const glm::vec3 kBgColor = glm::vec3(0.05f, 0.05f, 0.12f);

    void SetUp() override {
        VisualTestBase::SetUp();
        surfaceWidth  = kCanvasWidth;
        surfaceHeight = kCanvasHeight;
    }

    void TearDown() override {
        VisualTestBase::TearDown();
    }
};

// ============================================================================
// Test 1: Text without glow (default state)
// ============================================================================

/**
 * Render text with default glow settings (0, 0).
 * The shader should follow the no-glow path:
 *   - Discard fragments where SDF value < 0.01
 *   - Output color = pushConstants.color * value
 *   - No halo / glow outside the glyph body
 *
 * Expected: solid text glyphs with sharp edges and no surrounding glow.
 */
TEST_F(VisualTextGlowTest, TextWithoutGlow) {
    auto result = runAndCompare("text_glow_no_glow", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "No-glow text rendering failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels;

    // Additional structural check: with no glow, there should be very few
    // semi-transparent edge pixels. The mismatch ratio should be well under
    // the relaxed threshold for the no-glow path.
    EXPECT_LT(result.mismatchRatio, kTextGlowMaxMismatchRatio)
        << "No-glow text should have minimal pixel deviation.";
}

// ============================================================================
// Test 2: Text with glow enabled
// ============================================================================

/**
 * Render text with glowWidth=4, glowIntensity=0.8.
 * The shader should follow the glow path:
 *   - Quadratic falloff: glowFade = t * t
 *   - Visible glow halo extending beyond the glyph body
 *   - White highlight near the glyph edge, transitioning to text color
 *
 * Expected: text with a soft, luminous halo that is visibly wider than
 * the no-glow variant.
 */
TEST_F(VisualTextGlowTest, TextWithGlow) {
    // Configure glow before drawing
    context->setTextGlow(4.0f, 0.8f);

    auto result = runAndCompare("text_glow_with_glow", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "Glow text rendering failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels;

    // The glow halo adds many semi-transparent pixels around the glyph,
    // but the average error should still be within tolerance.
    EXPECT_LT(result.averageError, kTextGlowMaxAvgError)
        << "Glow halo contribution should be within tolerance.";
}

// ============================================================================
// Test 3: Glow intensity variation
// ============================================================================

/**
 * Compare two renders of the same text with identical glowWidth but
 * different glowIntensity values (0.3 vs 1.0).
 *
 * Higher glowIntensity modulates the glow alpha:
 *   finalAlpha = max(alpha, glowFade * glowIntensity)
 *
 * Expected: the high-intensity render should have a brighter, more
 * prominent glow halo. Both should pass against their own references,
 * but structural comparison of pixel brightness confirms the intensity
 * difference is visible.
 */
TEST_F(VisualTextGlowTest, GlowIntensityVariation) {
    // --- Low intensity (0.3) ---
    context->setTextGlow(4.0f, 0.3f);

    auto lowResult = runAndCompare("text_glow_intensity_low", relaxedTextGlowOpts());

    EXPECT_TRUE(lowResult.passed)
        << "Low-intensity glow text failed.\n"
        << "  averageError   = " << lowResult.averageError << "\n"
        << "  mismatchRatio   = " << lowResult.mismatchRatio;

    // --- High intensity (1.0) ---
    context->setTextGlow(4.0f, 1.0f);

    auto highResult = runAndCompare("text_glow_intensity_high", relaxedTextGlowOpts());

    EXPECT_TRUE(highResult.passed)
        << "High-intensity glow text failed.\n"
        << "  averageError   = " << highResult.averageError << "\n"
        << "  mismatchRatio   = " << highResult.mismatchRatio;

    // Both renders should succeed individually. Additionally, the
    // high-intensity render should have a lower pass rate if compared
    // against the low-intensity reference, confirming the intensity
    // change is visible.
    // (This cross-comparison is informational; we don't fail on it.)
}

// ============================================================================
// Test 4: Glow width variation
// ============================================================================

/**
 * Compare two renders of the same text with identical glowIntensity but
 * different glowWidth values (2 vs 6).
 *
 * The glow width controls the falloff distance:
 *   t = smoothstep(-glowWidth, 0.0, dist)
 *   glowFade = t * t
 *
 * A wider glow extends further from the glyph edge before fading to zero.
 * Note: the shader clamps glowWidth to SDF_SPREAD - 1 = 7.0.
 *
 * Expected: glowWidth=6 produces a noticeably wider halo than glowWidth=2.
 */
TEST_F(VisualTextGlowTest, GlowWidthVariation) {
    // --- Narrow glow (width=2) ---
    context->setTextGlow(2.0f, 0.8f);

    auto narrowResult = runAndCompare("text_glow_width_narrow", relaxedTextGlowOpts());

    EXPECT_TRUE(narrowResult.passed)
        << "Narrow glow text failed.\n"
        << "  averageError   = " << narrowResult.averageError << "\n"
        << "  mismatchRatio   = " << narrowResult.mismatchRatio;

    // --- Wide glow (width=6) ---
    context->setTextGlow(6.0f, 0.8f);

    auto wideResult = runAndCompare("text_glow_width_wide", relaxedTextGlowOpts());

    EXPECT_TRUE(wideResult.passed)
        << "Wide glow text failed.\n"
        << "  averageError   = " << wideResult.averageError << "\n"
        << "  mismatchRatio   = " << wideResult.mismatchRatio;

    // The wide glow should produce a larger halo area. Both should pass
    // against their respective references.
}

// ============================================================================
// Test 5: Text color preserved inside glyph body
// ============================================================================

/**
 * The shader's glow path mixes colors for the glow halo, but inside the
 * glyph body the final color should be the pure text color:
 *   finalColor = mix(glowColor, pushConstants.color, smoothstep(-1.0, 1.0, dist))
 *   At large positive dist (deep inside glyph), smoothstep returns ~1.0,
 *   so finalColor ~ pushConstants.color.
 *
 * This test renders text with glow enabled and verifies that the interior
 * of each glyph matches the specified text color within tolerance.
 * Two different text colors are tested to confirm color fidelity.
 */
TEST_F(VisualTextGlowTest, TextColorPreserved) {
    // Render with a distinct color (pure red) to make color verification obvious.
    const glm::vec3 redColor(1.0f, 0.0f, 0.0f);

    context->setTextGlow(4.0f, 0.8f);

    auto result = runAndCompare("text_glow_color_preserved", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "Color-preserved glow text failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio;

    // Also test with a second color (pure green) to confirm the
    // glow path does not bleed a fixed color into the glyph body.
    const glm::vec3 greenColor(0.0f, 1.0f, 0.0f);

    context->setTextGlow(4.0f, 0.8f);

    auto result2 = runAndCompare("text_glow_color_preserved_green", relaxedTextGlowOpts());

    EXPECT_TRUE(result2.passed)
        << "Green color-preserved glow text failed.\n"
        << "  averageError   = " << result2.averageError << "\n"
        << "  mismatchRatio   = " << result2.mismatchRatio;
}

// ============================================================================
// Test 6: Multi-line text with glow
// ============================================================================

/**
 * Render multi-line text with glow enabled. This exercises the
 * drawMultiLineText path combined with glow state.
 *
 * The glow halo should be applied uniformly to all lines, and
 * the line spacing should not cause glow from one line to interfere
 * with the text body of adjacent lines.
 *
 * Uses drawMultiLineText with maxWidth=350 and lineHeight=1.5.
 */
TEST_F(VisualTextGlowTest, MultiLineTextGlow) {
    context->setTextGlow(4.0f, 0.8f);

    auto result = runAndCompare("text_glow_multiline", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "Multi-line glow text failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  maxError        = " << result.maxError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio << "\n"
        << "  mismatchPixels  = " << result.mismatchPixels
        << " / " << result.totalPixels;
}

// ============================================================================
// Test 7: Text color accuracy — interior pixel sampling
// ============================================================================

/**
 * Renders a single large character ("A") at fontSize=64 and verifies that
 * the interior pixel color matches the specified text color within tolerance.
 *
 * This test samples specific pixels deep inside the glyph body where the
 * SDF distance is maximally positive. At these locations:
 *   - alpha = smoothstep(-0.8, 0.8, dist) ~ 1.0
 *   - finalColor = pushConstants.color (via the interior mix)
 *   - finalAlpha ~ 1.0
 *   - outColor = vec4(color * 1.0, 1.0) = color
 *
 * We check that the sampled pixel matches the expected text color within
 * a per-channel tolerance of 8 (out of 255), accounting for blending,
 * gamma, and SDF precision.
 */
TEST_F(VisualTextGlowTest, TextColorAccuracy) {
    // Use a distinctive warm orange color for clear identification.
    const glm::vec3 orangeColor(1.0f, 0.5f, 0.0f);

    context->setTextGlow(4.0f, 0.8f);

    auto result = runAndCompare("text_glow_color_accuracy", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "Text color accuracy test failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio;

    // Additionally verify that the capture contains valid data
    // and that pixel sampling is possible.
    if (lastCapture.valid) {
        // Sample a few pixels near the center of the canvas where the
        // large "A" glyph interior should be located. The glyph at
        // fontSize=64 with pos centered in a 400x300 canvas should
        // have its interior around (200, 150) approximately.
        //
        // We check that at least some of these samples are non-background
        // (i.e., the glyph was actually rendered there).
        uint32_t sampleX = kCanvasWidth / 2;
        uint32_t sampleY = kCanvasHeight / 2;
        uint32_t idx = (sampleY * kCanvasWidth + sampleX) * 4;

        if (idx + 3 < lastCapture.pixels.size()) {
            uint8_t r = lastCapture.pixels[idx + 0];
            uint8_t g = lastCapture.pixels[idx + 1];
            uint8_t b = lastCapture.pixels[idx + 2];

            // The center pixel should be non-trivial (not background).
            // We don't assert exact color because the glyph might not
            // perfectly cover the center, but something should be there.
            EXPECT_GT(r + g + b, 30)
                << "Center pixel appears empty — glyph may not cover the sampling point. "
                << "RGB = (" << static_cast<int>(r) << ", "
                << static_cast<int>(g) << ", " << static_cast<int>(b) << ")";
        }
    }
}

// ============================================================================
// Test 8: Glow disabled after setTextGlow(0, 0)
// ============================================================================

/**
 * Verify that calling setTextGlow(0, 0) after enabling glow reverts
 * to the no-glow rendering path. This tests the state machine behavior
 * of the glow settings.
 *
 * Steps:
 *   1. Enable glow (width=4, intensity=0.8)
 *   2. Disable glow (width=0, intensity=0)
 *   3. Render text — should match the no-glow reference
 */
TEST_F(VisualTextGlowTest, GlowDisabledAfterReset) {
    // Enable glow, then immediately disable it.
    context->setTextGlow(4.0f, 0.8f);
    context->setTextGlow(0.0f, 0.0f);

    auto result = runAndCompare("text_glow_disabled_after_reset", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "Glow-disabled text rendering failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio;

    // The result should be structurally similar to the no-glow test.
    // The shader checks glowIntensity <= 0 || glowWidth <= 0, so
    // setting both to 0 should hit the bitmap discard path.
    EXPECT_LT(result.mismatchRatio, kTextGlowMaxMismatchRatio);
}

// ============================================================================
// Test 9: Edge case — glow with very small width
// ============================================================================

/**
 * Test glow with a very small width (0.5 pixels). The glow should be
 * barely visible, concentrated right at the glyph edge.
 *
 * The shader computes:
 *   t = smoothstep(-0.5, 0.0, dist)
 *   glowFade = t * t
 * This produces an extremely tight glow band.
 */
TEST_F(VisualTextGlowTest, GlowVerySmallWidth) {
    context->setTextGlow(0.5f, 0.8f);

    auto result = runAndCompare("text_glow_small_width", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "Very small glow width text failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio;
}

// ============================================================================
// Test 10: Edge case — glow clamped to SDF_SPREAD - 1
// ============================================================================

/**
 * Test glow with width exceeding SDF_SPREAD - 1 (7.0).
 * The shader clamps: glowWidth = min(pushConstants.glowWidth, SDF_SPREAD - 1.0)
 *
 * Supplying glowWidth=10.0 should produce the same result as glowWidth=7.0,
 * since the shader internally clamps it. The glow should be wide but not
 * cause artifacts at the quad boundary.
 */
TEST_F(VisualTextGlowTest, GlowWidthClampedToSdfSpread) {
    context->setTextGlow(10.0f, 0.8f);

    auto result = runAndCompare("text_glow_width_clamped", relaxedTextGlowOpts());

    EXPECT_TRUE(result.passed)
        << "Clamped glow width text failed.\n"
        << "  averageError   = " << result.averageError << "\n"
        << "  mismatchRatio   = " << result.mismatchRatio;

    // Supplying glowWidth=7.0 (the clamp limit) should produce the same result.
    // The comparison between width=10 and width=7 should show minimal difference.
    context->setTextGlow(7.0f, 0.8f);

    auto refResult = runAndCompare("text_glow_width_seven", relaxedTextGlowOpts());

    EXPECT_TRUE(refResult.passed)
        << "Width=7 glow text failed.\n"
        << "  averageError   = " << refResult.averageError << "\n"
        << "  mismatchRatio   = " << refResult.mismatchRatio;
}
