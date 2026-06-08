#version 450

// MTSDF text fragment shader.
//
// - Coverage: 4-sample rotated-grid SS inside a wide 2.5-px AA band. Wide AA is
//   the only way to make diagonal / curved strokes stop looking like stairs -
//   single-pixel AA leaves a ~1/AA_WIDTH coverage step between adjacent rows,
//   which reads as staircase on non-axis-aligned edges.
// - Shape: median(RGB) for crisp corners; alpha (true SDF) for smooth gradients.
// - Lighting: light-from-above simulated both at the outer rim (via SDF
//   gradient) and inside the body (via local quad y-coord) so the top glow
//   reads strongly on the glyph interior rather than only on the thin rim.

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 fragLocalCoord;
layout(location = 2) in vec2 fragUvOrigin;

layout(location = 0) out vec4 outColor;

// Push constants layout (128 bytes total):
// Offset  0-63:  mat4 projection (vertex shader)
// Offset 64-75:  vec3 color1 (BGR) — gradient start / single color
// Offset 76-79:  float hasGradient (1.0 = gradient, 0.0 = single color)
// Offset 80-83:  float glowIntensity
// Offset 84-87:  float glowWidth
// Offset 88-91:  float color2_b (BGR) — gradient end color
// Offset 92-95:  float color2_g
// Offset 96-99:  float color2_r
// Offset 100-103: float strokeWidth (0 = no stroke)
// Offset 104-107: float strokeColor_b (BGR)
// Offset 108-111: float strokeColor_g
// Offset 112-115: float strokeColor_r
// Offset 116-119: float gradientDir (0=vertical, 1=horizontal)
// Offset 120-127: padding (2 floats)
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec3 color1;
    layout(offset = 76) float hasGradient;
    layout(offset = 80) float glowIntensity;
    layout(offset = 84) float glowWidth;
    layout(offset = 88) float color2_b;     // BGR
    layout(offset = 92) float color2_g;
    layout(offset = 96) float color2_r;
    layout(offset = 100) float strokeWidth;
    layout(offset = 104) float strokeColor_b; // BGR
    layout(offset = 108) float strokeColor_g;
    layout(offset = 112) float strokeColor_r;
    layout(offset = 116) float gradientDir;
} pushConstants;

layout(binding = 0) uniform sampler2D msdfAtlas;

const float ATLAS_PX_RANGE = 8.0;    // match msdf-atlas-gen -pxrange
const float AA_WIDTH_PX    = 2.5;    // coverage transition width in screen pixels

float median3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

// Canonical Chlumsky formula - stable across zoom levels.
float screenPxRange() {
    vec2 unitRange = vec2(ATLAS_PX_RANGE) / vec2(textureSize(msdfAtlas, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(fragTexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    float spr = screenPxRange();

    // Clamp glow/stroke widths to the SDF coverage range.
    // The MSDF distance range covers ±(spr * 0.5) screen pixels from the edge.
    // smoothstep needs its lower edge to be <= -spr*0.5 to return 0 at the
    // farthest point.  For stroke: lower edge = -sw - AA_WIDTH_PX/2.
    // So sw <= spr*0.5 - AA_WIDTH_PX/2.  Use the same bound for glow.
    float maxEffectRange = max(spr * 0.5 - AA_WIDTH_PX * 0.5, 0.5);
    float ow  = min(max(pushConstants.glowWidth, 1.0), maxEffectRange);

    // Construct vec3 from individual floats
    vec3 color2 = vec3(pushConstants.color2_b, pushConstants.color2_g, pushConstants.color2_r);
    vec3 strokeCol = vec3(pushConstants.strokeColor_b, pushConstants.strokeColor_g, pushConstants.strokeColor_r);

    // Determine fill color: gradient or single color
    float gradientT = (pushConstants.gradientDir > 0.5) ? fragLocalCoord.x : fragLocalCoord.y;
    vec3 fillColor = (pushConstants.hasGradient > 0.5)
        ? mix(pushConstants.color1, color2, gradientT)
        : pushConstants.color1;

    bool hasGlow = (pushConstants.glowIntensity > 0.0 && pushConstants.glowWidth > 0.0);
    bool hasStroke = (pushConstants.strokeWidth > 0.0);

    // Rotated-grid 4-sample SS: points form a 22.5-degree-rotated square spanning
    // most of the pixel footprint. Better angular coverage than axis-aligned 2x2.
    vec2 duv_dx = dFdx(fragTexCoord);
    vec2 duv_dy = dFdy(fragTexCoord);
    const vec2 RG[4] = vec2[4](
        vec2( 0.375,  0.125),
        vec2(-0.125,  0.375),
        vec2(-0.375, -0.125),
        vec2( 0.125, -0.375)
    );

    float bodyAcc = 0.0;
    float covAcc  = 0.0;
    float strokeAcc = 0.0;
    for (int i = 0; i < 4; ++i) {
        vec2 uv = fragTexCoord + RG[i].x * duv_dx + RG[i].y * duv_dy;
        vec4 s = texture(msdfAtlas, uv);
        float sds = median3(s.r, s.g, s.b);
        float spd = spr * (sds - 0.5);
        bodyAcc += clamp(spd / AA_WIDTH_PX + 0.5, 0.0, 1.0);
        covAcc  += clamp((spd + ow) / AA_WIDTH_PX + 0.5, 0.0, 1.0);
        if (hasStroke) {
            strokeAcc += clamp((spd + pushConstants.strokeWidth) / AA_WIDTH_PX + 0.5, 0.0, 1.0);
        }
    }
    float bodyOpacity     = bodyAcc * 0.25;
    float outlineCoverage = covAcc  * 0.25;
    float strokeOpacity   = strokeAcc * 0.25;

    // --- No glow, no stroke: plain text (still supersampled) ---
    if (!hasGlow && !hasStroke) {
        if (bodyOpacity < 0.01) discard;
        outColor = vec4(fillColor * bodyOpacity, bodyOpacity);
        return;
    }

    // --- Stroke only (no glow) ---
    // Keep supersampled bodyOpacity for high-quality body edges.
    // Clamp stroke width to SDF coverage range.
    if (!hasGlow && hasStroke) {
        float sw = min(pushConstants.strokeWidth, maxEffectRange);
        vec4 sC = texture(msdfAtlas, fragTexCoord);
        float sdsC = median3(sC.r, sC.g, sC.b);
        float distC = spr * (sdsC - 0.5);
        float strokeEdge = smoothstep(-sw - AA_WIDTH_PX * 0.5, -sw + AA_WIDTH_PX * 0.5, distC);

        vec3 col = mix(strokeCol, fillColor, bodyOpacity);
        float alpha = max(bodyOpacity, strokeEdge);
        if (alpha < 0.01) discard;
        outColor = vec4(col * alpha, alpha);
        return;
    }

    // --- Glow path (with optional stroke) ---
    if (outlineCoverage < 0.01) discard;

    // Use the MSDF median (same data as bodyOpacity) for distance-based glow/stroke.
    // The alpha channel may be binary (1/0) in some atlas configurations, which makes
    // distance-based calculations unreliable.  The RGB median gives a proper gradient.
    vec4 sCenter = texture(msdfAtlas, fragTexCoord);
    float sdsCenter = median3(sCenter.r, sCenter.g, sCenter.b);
    float distCenter = spr * (sdsCenter - 0.5);  // signed distance in screen pixels

    // Glow fade: quadratic, 1 at body edge (dist=0), 0 at glow outer edge (dist=-ow)
    float glowT    = smoothstep(-ow, 0.0, distCenter);
    float glowFade = glowT * glowT;
    float glowAlpha = glowFade * pushConstants.glowIntensity;

    float finalAlpha = max(bodyOpacity, glowAlpha);
    float distStrokeAlpha = 0.0;
    if (hasStroke) {
        float sw = min(pushConstants.strokeWidth, maxEffectRange);
        distStrokeAlpha = smoothstep(-sw - AA_WIDTH_PX * 0.5, -sw + AA_WIDTH_PX * 0.5, distCenter);
        finalAlpha = max(finalAlpha, distStrokeAlpha);
    }
    if (finalAlpha < 0.01) discard;

    // --- Inner top-lit body: upper half of the glyph blends toward white ---
    float innerTopLit = smoothstep(0.65, 0.0, fragLocalCoord.y);
    vec3  bodyLit = mix(fillColor, vec3(1.0), innerTopLit * 0.30);

    // --- Directional rim: top -> white highlight, bottom -> slightly darker,
    // sides -> mid. Uses the true-SDF (alpha) gradient so it's smooth on diagonals.
    float sdTrue   = sCenter.a;
    vec2  gradTrue = vec2(dFdx(sdTrue), dFdy(sdTrue));
    float gradLen  = length(gradTrue);
    float lightY   = (gradLen > 1e-4) ? gradTrue.y / gradLen : 0.0;
    float litness  = lightY * 0.5 + 0.5;

    vec3 rimTarget = mix(fillColor * 0.8, vec3(1.0), litness);
    vec3 rimColor  = mix(fillColor, rimTarget, pushConstants.glowIntensity);
    vec3 col       = mix(rimColor, bodyLit, bodyOpacity);

    // Apply stroke in the band between body edge and glow only.
    if (hasStroke) {
        float strokeBand = distStrokeAlpha * (1.0 - bodyOpacity);
        col = mix(col, strokeCol, strokeBand);
    }

    outColor = vec4(col * finalAlpha, finalAlpha);
}
