#version 450

// Push constants for projection matrix
layout(push_constant) uniform PushConstants {
    mat4 projection;
} push;

// Vertex inputs
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec2 inLocalCoord;
layout(location = 3) in vec2 inUvOrigin;

// Outputs to fragment shader
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 fragLocalCoord;
layout(location = 2) out vec2 fragUvOrigin;

void main() {
    fragTexCoord = inTexCoord;
    fragLocalCoord = inLocalCoord;
    fragUvOrigin = inUvOrigin;
    gl_Position = push.projection * vec4(inPosition, 0.0, 1.0);
}
