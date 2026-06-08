#version 450

// Push constants for projection matrix
layout(push_constant) uniform PushConstants {
    mat4 projection;
} push;

// Vertex input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;

void main() {
    fragTexCoord = inTexCoord;
    gl_Position = push.projection * vec4(inPosition, 1.0);
}
