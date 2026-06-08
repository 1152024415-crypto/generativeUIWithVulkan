#version 450

// Vertex inputs - position (vec3) + UV (vec2)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

// Push constants for rectangle data (96 bytes = 24 floats)
layout(push_constant) uniform RectData {
    vec2 pos;
    vec2 size;
    vec4 color;
    vec4 radii;          // Must match fragment shader: four corner radii
    float is_text;
    float glassFlags;    // Glass effect flags (0 = no glass)
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

// Uniform buffer - MVP matrix only
layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

// Output to fragment shader
layout(location = 0) out vec2 uv;

void main() {
    vec4 pos = ubo.mvp * vec4(inPosition, 1.0);
    gl_Position = pos;

    // Pass UV coordinates [0,1] for SDF calculation
    uv = inUV;
}
