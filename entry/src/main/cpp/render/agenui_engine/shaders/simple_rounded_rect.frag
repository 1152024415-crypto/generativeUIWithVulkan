#version 450

/* Input from vertex shader */
layout(location = 0) in vec2 uv;

/* Push constants for rectangle data (96 bytes = 24 floats) */
layout(push_constant) uniform RectData {
    vec2 pos;
    vec2 size;
    vec4 color;
    vec4 radii;
    float is_text;
    float glassFlags;
    float edgeSharpness;
    float glowWidth;
    float glowIntensity;
    float specBandDist;
    float specIntensity;
    float specWidth;
    float chromaStr;
    float glassAlpha;
    float lightAngle;
    float pad;
} rect;

/* Background texture for glass refraction (descriptor set 1, binding 1) */
layout(set = 1, binding = 1) uniform sampler2D bgTexture;

/* Output color */
layout(location = 0) out vec4 outColor;

void main() {
    vec2 pixel_pos = rect.pos + uv * rect.size;
    vec2 center = rect.pos + rect.size * 0.5;
    vec2 p = pixel_pos - center;
    vec2 half_size = rect.size * 0.5;

    bool isGlass = rect.glassFlags > 0.5;
    float cornerR = min(min(rect.radii.x, rect.radii.y), min(rect.radii.z, rect.radii.w));
    cornerR = min(cornerR, min(half_size.x, half_size.y));

    // NDC-space SDF with aspect ratio correction
    float cornerRy = max(rect.pad, 0.0001);
    float yScale = cornerR / cornerRy;
    vec2 pc = vec2(p.x, p.y * yScale);
    vec2 hc = vec2(half_size.x, half_size.y * yScale);
    vec2 q = abs(pc) - hc + cornerR;
    float dist = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - cornerR;

    // Keep anti-aliasing inside the rounded rectangle only. A symmetric AA band
    // fades pixels outside the shape as well; on bright backgrounds that reads as
    // a white rim around every rounded corner.
    float aa = max(fwidth(dist), 0.0001);
    if (dist > 0.0) discard;
    float shapeAlpha = 1.0 - smoothstep(-aa, 0.0, dist);

    // --- Non-glass path ---
    if (!isGlass) {
        if (shapeAlpha < 0.01) discard;
        outColor = vec4(rect.color.b, rect.color.g, rect.color.r, rect.color.a * shapeAlpha);
        return;
    }

    /* ================================================================
     * LIQUID GLASS v2 — 响应鸿蒙 6.1 "透明玻璃" 描述
     *   · 折射 + box blur（保留）
     *   · 分通道色散 (chromaStr，边缘强中心弱) — "光影折射"
     *   · 镜面高光带 (specIntensity / specWidth / specBandDist) — "镜面高光响应"
     * ================================================================ */

    // Reuse tight AA shape (fwidth*2) instead of wide glass AA (fwidth*aaScale)
    // Wide AA caused visible white fringe at rounded corners
    float shape = shapeAlpha;

    // Screen UV: NDC [-1,1] → texture [0,1]
    vec2 screenUV = (pixel_pos + vec2(1.0)) * 0.5;
    vec2 centerUV = (center + vec2(1.0)) * 0.5;

    // Normalized local coords [-1,1]
    vec2 localPos    = p / max(half_size, vec2(0.001));
    float centerDist = length(localPos);

    // rb1 = glass body mask
    float rb1 = shape;

    // rb3 = shadow gradient 从边缘向内衰减
    float shadowW = fwidth(dist) * 14.0;
    float rb3 = 1.0 - smoothstep(0.0, shadowW, -dist);

    float transition = smoothstep(0.0, 1.0, rb1);
    if (transition < 0.01) discard;

    // ── 1. 折射（轻微）：原值 0.4 会让靠近边缘的背景严重变形，改为 0.12 仅保留一丝通透感 ──
    float lensScale = 1.0 - centerDist * centerDist * 0.12;
    vec2  lensDir   = screenUV - centerUV;
    vec2  lensUV    = clamp(centerUV + lensDir * lensScale, vec2(0.0), vec2(1.0));

    // ── 2. 色散：靠近边缘分通道 UV 偏移，营造玻璃边缘彩虹晕 ──
    vec2 texelSize   = 1.0 / vec2(textureSize(bgTexture, 0));
    float chromaMix  = rect.chromaStr * (1.0 - rb3);
    vec2  chromaDir  = normalize(lensDir + vec2(1e-5));
    vec2  chromaOff  = chromaDir * texelSize * chromaMix * 8.0;

    // ── 3. 9×9 box blur（加强）— R/B 通道 UV 偏移产生色散 ──
    vec3  blurred = vec3(0.0);
    float total   = 0.0;
    for (int x = -4; x <= 4; x++) {
        for (int y = -4; y <= 4; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize * 1.5;
            blurred.r += texture(bgTexture, lensUV + offset + chromaOff).r;
            blurred.g += texture(bgTexture, lensUV + offset              ).g;
            blurred.b += texture(bgTexture, lensUV + offset - chromaOff).b;
            total     += 1.0;
        }
    }
    blurred /= total;

    // ── 4. 顶亮-底柔垂直渐变 ──
    float gradient = clamp((clamp(-localPos.y, 0.0, 0.5) + 0.1) / 2.0, 0.0, 1.0)
                   + clamp((clamp( localPos.y, -10.0, 0.5) * rb3 + 0.1) / 2.0, 0.0, 1.0);

    // ── 5. 镜面高光带 (specular band) ──
    //   bandY ∈ [-1,1]，由 specBandDist 决定（0=顶, 1=底），高斯柔化，宽度由 specWidth 决定
    float bandY        = -1.0 + rect.specBandDist * 2.0;
    float bandD        = abs(localPos.y - bandY);
    float bandW        = max(rect.specWidth, 0.01);
    float bandFalloff  = exp(-bandD * bandD / (2.0 * bandW * bandW));
    // 水平方向稍收拢以免两端过亮
    float bandHoriz    = 1.0 - smoothstep(0.7, 1.0, abs(localPos.x));
    float specBand     = bandFalloff * bandHoriz * rect.specIntensity * rb1;

    // ── 6. 合成 — tint 加至 18% 让玻璃不至透明过度，gradient 系数保留 0.5 ──
    vec3 tint = rect.color.rgb;
    // Fade tint and gradient at the SDF AA boundary to prevent:
    //  1) clamp-to-white at rounded corners (wider AA band → more bright pixels)
    //  2) faint bright fringe just outside the glass shape
    float interiorMask = smoothstep(0.0, 0.6, rb1);
    vec3 base = mix(blurred, tint, 0.18 * interiorMask);
    vec3 lighting = clamp(
        base
        + vec3(rb1) * gradient * 0.5 * interiorMask
        + vec3(specBand),
        0.0, 1.0);

    vec3  rawBg      = texture(bgTexture, screenUV).rgb;
    vec3  finalColor = mix(rawBg, lighting, transition);
    float alpha      = shape * rect.glassAlpha;

    if (alpha < 0.04) discard;
    outColor = vec4(finalColor.b, finalColor.g, finalColor.r, alpha);
}
