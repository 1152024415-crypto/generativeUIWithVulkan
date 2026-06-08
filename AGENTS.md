# GenerativeUIFroMMX - OpenHarmony XComponent Vulkan Rendering Engine

## Project Overview
OpenHarmony application demonstrating advanced Vulkan rendering via XComponent with:
- Rounded rectangle rendering using SDF (Signed Distance Fields)
- Text rendering with Chinese font support (Microsoft YaHei)
- Multi-pipeline architecture (rectangle, rounded rectangle, text)
- Resource loading from rawfile via ResourceManager

## Key Directories
- `entry/src/main/cpp/` - Native C++ implementation
  - `render/agenui_engine/` - Core Vulkan rendering engine (VkRenderer)
  - `render/plugin_*` - XComponent integration layer
  - `vulkan/` - Vulkan initialization and rendering loop
- `entry/src/main/ets/` - ArkTS/UI layer
  - `pages/Index.ets` - Application entry point and resource loading
- `resources/rawfile/` - Shaders (.spv) and fonts (.ttc)

## Critical Implementation Details

### Vulkan Coordinate System
- **Y-axis is inverted vs OpenGL**: Y=-1 is TOP, Y=+1 is BOTTOM
- All position calculations must account for this inversion

### Descriptor Pool Management (CRITICAL)
- Rounded rectangle pipeline uses **persistent descriptor set**
- **NEVER** call `vkResetDescriptorPool` on `m_roundedRectDescriptorPool` in `beginFrame()`
- Only reset per-frame descriptor pools: `if (m_descriptorPool != VK_NULL_HANDLE) vkResetDescriptorPool(m_device, m_descriptorPool, 0);`

### Text Rendering Requirements
- Character advance/width normalization: `glyph.value / swapChainExtent.width * 2.0f`
- Vertical centering requires calculating text bounding box, not individual glyph metrics
- Chinese font support validation: Check glyph for U+4F60 ("你") has width > 0

### Shader Loading
Shaders loaded from TWO locations in order:
1. ResourceManager (APK packaged)  
2. Filesystem: `/data/storage/el2/base/files/rawfile/[shaderName]`

### Resource Copying (Index.ets)
At runtime, Index.ets copies from `resources/rawfile` to app sandbox:
- Shaders: `*.spv` files
- Font: `msyh.ttc`

### Build & Dev Commands
- IDE: DevEco Studio 4.0+ required
- Target SDK: API 10+ (sample uses API 11)
- Signing: Uses `signature/default_*.p12` with password in build-profile.json5

### Common Pitfalls
1. **Black screen/reset issue**: Caused by resetting persistent descriptor pool
2. **Text misalignment**: Using fontSize instead of swapChainExtent for normalization
3. **Missing Chinese text**: Font doesn't contain required glyphs (test with U+4F60)
4. **Shader not found**: Resources not copied to sandbox by Index.ets