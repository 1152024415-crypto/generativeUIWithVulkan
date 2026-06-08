#version 450

// Input from vertex shader
layout(location = 0) in vec2 fragTexCoord;

// Output
layout(location = 0) out vec4 outColor;

// Texture sampler
layout(binding = 0) uniform sampler2D imageTexture;

void main() {
    // Flip Y coordinate for Vulkan texture coordinates
    vec2 flippedTexCoord = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);

    // Sample the texture
    vec4 texColor = texture(imageTexture, flippedTexCoord);

    // Alpha test with low threshold
    if (texColor.a < 0.05) {
        discard;
    }

    // Swap R/B for BGRA swapchain (IDENTITY swizzle on ImageView)
    outColor = vec4(texColor.b, texColor.g, texColor.r, 1.0);
}
