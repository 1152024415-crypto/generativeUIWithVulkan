#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform CircleData {
    vec2 pos;
    vec2 size;
    vec4 color;
} circle;

void main() {
    vec2 pixel_pos = circle.pos + uv * circle.size;
    vec2 center = circle.pos + circle.size * 0.5;
    vec2 half_size = circle.size * 0.5;
    vec2 p = (pixel_pos - center) / half_size;

    float dist = length(p);

    // Hard edge: inside circle alpha=1, outside discard
    if (dist > 1.0) discard;

    outColor = vec4(circle.color.z, circle.color.y, circle.color.x, circle.color.w);
}
