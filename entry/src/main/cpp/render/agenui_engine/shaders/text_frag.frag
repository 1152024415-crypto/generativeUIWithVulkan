#version 450

// Inputs from vertex shader
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 fragLocalCoord;
layout(location = 2) in vec2 fragUvOrigin;

// Output
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

layout(binding = 0) uniform sampler2D textTexture;

// SDF was rendered at fontSize=64, spread=8.
// The quad maps exactly to the SDF bitmap, so at the quad edge dist = -SDF_SPREAD.

const float SDF_SPREAD = 8.0;

void main() {
    float value = texture(textTexture, fragTexCoord).r;

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

    // --- No glow, no stroke: original bitmap rendering path ---
    if (!hasGlow && !hasStroke) {
        if (value < 0.01) discard;
        outColor = vec4(fillColor * value, value);
        return;
    }

    // --- SDF distance (used by glow and stroke paths) ---
    // R8_UNORM: sampler returns 0.0-1.0, edge at ~0.5
    // dist in SDF pixels: value=0.5 -> 0 (edge), 1.0 -> +8 (inside), 0.0 -> -8 (outside)
    float dist = (value - 0.5) * 2.0 * SDF_SPREAD;

    // --- Stroke only (no glow) ---
    if (!hasGlow && hasStroke) {
        float bodyAlpha = smoothstep(-0.8, 0.8, dist);
        float strokeAlpha = smoothstep(-pushConstants.strokeWidth - 0.8, -pushConstants.strokeWidth + 0.8, dist);
        vec3 col = mix(strokeCol, fillColor, bodyAlpha);
        float alpha = max(bodyAlpha, strokeAlpha);
        if (alpha < 0.01) discard;
        outColor = vec4(col * alpha, alpha);
        return;
    }

    // --- Glow path (with optional stroke) ---
    // Clamp effective glow width to the SDF coverage range.
    float glowWidth = min(pushConstants.glowWidth, SDF_SPREAD - 1.0);

    // Inside the glyph: solid fill with edge smoothing
    float alpha = smoothstep(-0.8, 0.8, dist);

    // Glow falloff: quadratic for soft appearance
    float t = smoothstep(-glowWidth, 0.0, dist);
    float glowFade = t * t;

    // Gradient glow color: bright white highlight at edge -> text color outward
    float colorT = smoothstep(-glowWidth * 0.5, 0.0, dist);
    vec3 innerColor = mix(fillColor, vec3(1.0), 0.85);
    vec3 outerColor = fillColor;
    vec3 glowColor = mix(outerColor, innerColor, colorT);

    // Inside the glyph: use pure fill color
    vec3 finalColor = mix(glowColor, fillColor, smoothstep(-1.0, 1.0, dist));

    // Apply stroke in the band between body edge and glow only.
    // strokeBand = 1 only in the stroke ring (bodyOpacity→0, strokeOpacity→1),
    // so the outer glow keeps its gradient color instead of being overwritten by strokeColor.
    if (hasStroke) {
        float strokeAlpha = smoothstep(-pushConstants.strokeWidth - 0.8, -pushConstants.strokeWidth + 0.8, dist);
        float strokeBand = strokeAlpha * (1.0 - alpha);
        finalColor = mix(finalColor, strokeCol, strokeBand);
        alpha = max(alpha, strokeAlpha);
    }

    float finalAlpha = max(alpha, glowFade * pushConstants.glowIntensity);

    if (finalAlpha < 0.01) discard;

    outColor = vec4(finalColor * finalAlpha, finalAlpha);
}
