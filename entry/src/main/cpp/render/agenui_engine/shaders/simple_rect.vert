#version 450

// Vertex inputs - position (vec3) + color (vec4, with alpha)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

// Uniform buffer (binding = 0) - 64 bytes total
// Struct: mat4 mvp only (no corner radius needed for simple rectangle)
layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

// Outputs to fragment shader
layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
}
