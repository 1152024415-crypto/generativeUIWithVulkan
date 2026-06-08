# XComponent with Vulkan - Enhanced Rendering Engine

## Introduction

The XComponent component is a drawing component that can be used to meet developers' complex custom drawing requirements, such as camera preview stream display and game rendering.

The component is divided into `surface` type and `component` type, which can be specified by the `type` field. The `surface` type supports developers to pass relevant data to the XComponent's own surface for rendering.

Based on the "Native C++" template, this sample demonstrates XComponent calling Vulkan API to complete **advanced graphics rendering**, including:
- Rounded Rectangle Rendering
- Text Rendering
- Chinese Font Support
- Multi-Pipeline Rendering System

## Effect Preview

After opening the application, four rounded rectangles of different colors are displayed in the four corners of the screen, with Chinese text centered in each rectangle:

| Position | Color | Text |
|----------|-------|------|
| Top-Left | Blue | "左上" |
| Top-Right | Red | "右上" |
| Bottom-Left | Green | "左下" |
| Bottom-Right | Orange | "右下" |

Click the 'stop/start' button to control rendering pause/resume.

## Project Structure

```
entry/src/main/
|---cpp
|   |---common
|   |   |---logger_common.h                          // Hilog logging macros
|   |---render
|   |   |---agenui_engine                            // AgenUI rendering engine
|   |   |   |---include
|   |   |   |   |---FontManager.h                    // Font manager
|   |   |   |   |---TextAtlas.h                      // Text texture atlas
|   |   |   |   |---vulkan
|   |   |   |   |   |---VkRenderer.h                 // Vulkan renderer core
|   |   |   |---src
|   |   |   |   |---FontManager.cpp
|   |   |   |   |---TextAtlas.cpp
|   |   |   |   |---vulkan
|   |   |   |   |   |---VkRenderer.cpp               // Rendering implementation (~3500 lines)
|   |   |   |---shaders
|   |   |   |   |---simple_rect.*                    // Simple rectangle shaders
|   |   |   |   |---simple_rounded_rect.*            // Rounded rectangle shaders
|   |   |   |   |---text_*                           // Text rendering shaders
|   |   |   |---thirdparty
|   |   |       |---freetype                         // FreeType font library
|   |   |       |---glm                              // GLM math library
|   |   |---vulkan
|   |   |   |---vulkan_example.h                     // Vulkan example class
|   |   |   |---vulkan_example.cpp                   // Application rendering logic
|   |   |---plugin_manager.h                         // XComponent integration
|   |   |---plugin_manager.cpp
|   |   |---plugin_render.h                          // Vulkan backend integration
|   |   |---plugin_render.cpp
|   |---CMakeLists.txt
|   |---plugin.cpp                                   // NAPI registration
|---ets
|   |---entryability
|   |   |---EntryAbility.ts                          // Application entry point
|   |---pages
|   |   |---Index.ets                                // UI layout and resource loading
|---resources
|   |---rawfile
|       |---shaders/
|       |   |---simple_rect.vert.spv
|       |   |---simple_rect.frag.spv
|       |   |---simple_rounded_rect.vert.spv
|       |   |---simple_rounded_rect.frag.spv
|       |   |---text_vert.spv
|       |   |---text_frag.spv
|       |---msyh.ttc                                 // Microsoft YaHei (Chinese support)
```

## Architecture Design

### Module Layers

```
┌─────────────────────────────────────────────────────────┐
│                    ArkTS UI Layer                       │
│                    (Index.ets)                          │
└────────────────────────┬────────────────────────────────┘
                         │ XComponent
┌────────────────────────▼────────────────────────────────┐
│                   NAPI Bridge Layer                     │
│              (plugin.cpp / PluginManager)               │
└────────────────────────┬────────────────────────────────┘
                         │ NativeWindow
┌────────────────────────▼────────────────────────────────┐
│                 Plugin Render Layer                     │
│            (plugin_render.cpp / VulkanExample)          │
└────────────────────────┬────────────────────────────────┘
                         │ Render Commands
┌────────────────────────▼────────────────────────────────┐
│                AgenUI Engine (VkRenderer)               │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │ Rect Pipeline│ Rounded Rect │  Text Pipeline       │ │
│  │              │   Pipeline   │                      │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
│  ┌────────────────────────────────────────────────────┐ │
│  │         FontManager (FreeType)                     │ │
│  └────────────────────────────────────────────────────┘ │
└────────────────────────┬────────────────────────────────┘
                         │ Vulkan API
┌────────────────────────▼────────────────────────────────┐
│                    Vulkan Driver                        │
└─────────────────────────────────────────────────────────┘
```

### Core Components

**1. VkRenderer (AgenUIEngine)**
- Location: `entry/src/main/cpp/render/agenui_engine/src/vulkan/VkRenderer.cpp`
- Function: Complete Vulkan rendering pipeline implementation
- Pipeline Types:
  - `createRectanglePipeline()` - Simple rectangles
  - `createRoundedRectPipeline()` - Rounded rectangles
  - `createTextPipeline()` - Text rendering

**2. FontManager**
- Location: `entry/src/main/cpp/render/agenui_engine/src/FontManager.cpp`
- Function: Font loading and glyph rasterization
- Supports Chinese font detection (test character U+4F60 "你")

**3. TextAtlas**
- Location: `entry/src/main/cpp/render/agenui_engine/src/TextAtlas.cpp`
- Function: Packs glyphs into texture atlas for optimized rendering

## Implementation Details

### XComponent and Vulkan Integration

#### NAPI Registration

```cpp
// entry/src/main/cpp/plugin.cpp
static napi_module SampleModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,  // NAPI registration function
    .nm_modname = "nativerender",  // so name
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
    napi_module_register(&SampleModule);
}
```

#### PluginManager Initialization

```cpp
// entry/src/main/cpp/render/plugin_manager.cpp
bool PluginManager::Init(napi_env env, napi_value exports)
{
    // Get NativeXComponent pointer
    napi_value exportInstance = nullptr;
    OH_NativeXComponent *nativeXComponent = nullptr;
    napi_status status = napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));

    // Get XComponentId and create PluginRender
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    int32_t ret = OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize);

    std::string id(idStr);
    auto context = PluginManager::GetInstance();
    context->SetNativeXComponent(id, nativeXComponent);
    auto render = context->GetRender(id);
    render->Export(env, exports);
    render->SetNativeXComponent(nativeXComponent);
    return true;
}
```

#### PluginRender Event Callbacks

```cpp
// entry/src/main/cpp/render/plugin_render.cpp
OH_NativeXComponent_Callback *PluginRender::GetNXComponentCallback()
{
    return &PluginRender::callback_;
}

PluginRender::PluginRender(std::string &id) : id_(id), component_(nullptr)
{
    auto renderCallback = PluginRender::GetNXComponentCallback();
    renderCallback->OnSurfaceCreated = OnSurfaceCreatedCB;
    renderCallback->OnSurfaceChanged = OnSurfaceChangedCB;
    renderCallback->OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    renderCallback->DispatchTouchEvent = DispatchTouchEventCB;
}

// Export methods to JS
napi_value PluginRender::Export(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "stopOrStart", nullptr, PluginRender::NapiStopMovingOrRestart, nullptr, nullptr, nullptr,
            napi_default, nullptr}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
```

#### Vulkan Backend Initialization

```cpp
// entry/src/main/cpp/render/plugin_render.cpp
void PluginRender::OnSurfaceCreated(OH_NativeXComponent *component, void *window)
{
    if (vulkanExample_ == nullptr) {
        vulkanExample_ = std::make_unique<vkExample::VulkanExample>();

        // Set custom shader loader
        auto* pluginManager = PluginManager::GetInstance();
        NativeResourceManager* resourceManager = pluginManager->GetResourceManager();
        vulkanExample_->SetResourceManager(resourceManager);
        vulkanExample_->SetShaderLoader([resourceManager](const char* shaderName) -> std::vector<char> {
            // Load shaders from ResourceManager or filesystem
            // ...
        });

        vulkanExample_->SetupWindow(static_cast<OHNativeWindow *>(window));
        vulkanExample_->InitVulkan();
        renderThread_ = std::thread(std::bind(&PluginRender::RenderThread, this));
    }
}
```

### Vulkan Rendering Pipeline

#### VkRenderer Initialization Flow

```cpp
// entry/src/main/cpp/render/vulkan/vulkan_example.cpp
bool VulkanExample::InitVulkan()
{
    m_core = std::make_unique<AgenUIEngine::VkRenderer>();

    // Set shader loader
    m_core->setShaderLoader(m_shaderLoader);

    // Initialize Vulkan environment
    if (!m_core->initialize(m_window, m_width, m_height)) {
        return false;
    }

    // Create multi-pipeline system
    m_core->createRectanglePipeline();        // Rectangle pipeline
    m_core->createRoundedRectPipeline();      // Rounded rectangle pipeline
    m_core->createTextPipeline();             // Text pipeline

    // Initialize Chinese font
    const char* fontName = "msyh.ttc";  // Microsoft YaHei
    m_core->initializeFonts(m_resourceManager, fontName);

    return true;
}
```

#### Rendering Loop

```cpp
// entry/src/main/cpp/render/vulkan/vulkan_example.cpp
void VulkanExample::RenderLoop()
{
    m_core->beginFrame();

    // Top-left - Blue rounded rectangle + "左上"
    m_core->drawRoundedRectangle(
        glm::vec2(-0.85f, -0.65f),  // Position (bottom-left corner)
        glm::vec2(0.7f, 0.3f),       // Size
        0.08f,                        // Corner radius
        glm::vec3(0.2f, 0.4f, 0.8f)  // Color (blue)
    );
    m_core->drawText("左上", glm::vec2(-0.5f, -0.45f), 50, glm::vec3(1.0f));

    // Top-right, bottom-left, bottom-right...

    m_core->endFrame();
}
```

### Text Rendering System

#### Coordinate System

**Vulkan NDC (Normalized Device Coordinates):**
- X ∈ [-1, 1], -1 is left, +1 is right
- Y ∈ [-1, 1], **-1 is top**, +1 is bottom (opposite of OpenGL)

#### Character Spacing Calculation

Critical fix: Character advance and width use the same normalization method

```cpp
// entry/src/main/cpp/render/agenui_engine/src/vulkan/VkRenderer.cpp:3503
// Wrong: advance / fontSize * 2.0f
// Correct:
float advanceWidth = static_cast<float>(glyph.advance) / static_cast<float>(m_swapChainExtent.width) * 2.0f;
float charWidth = static_cast<float>(glyph.width) / static_cast<float>(m_swapChainExtent.width) * 2.0f;
```

#### Text Vertical Centering

```cpp
// Calculate text bounding box
float maxBitmapTop = glyphBitmapTops[0];
float minBottom = glyphBitmapTops[0] - glyphHeights[0];
for (size_t i = 1; i < glyphs.size(); ++i) {
    if (glyphBitmapTops[i] > maxBitmapTop) maxBitmapTop = glyphBitmapTops[i];
    float bottom = glyphBitmapTops[i] - glyphHeights[i];
    if (bottom < minBottom) minBottom = bottom;
}
float boundingBoxCenterY = (maxBitmapTop + minBottom) / 2.0f;
float baselineY = -boundingBoxCenterY;  // Baseline position for bounding box centering
```

#### Chinese Font Loading

```cpp
// entry/src/main/cpp/render/agenui_engine/src/vulkan/VkRenderer.cpp:2896
// Test if font supports Chinese (character U+4F60 "你")
const Glyph* testGlyph = m_fontManager->getGlyph(0x4F60, 64);
if (testGlyph != nullptr && testGlyph->width > 0) {
    foundChineseFont = true;
    LOGI("Font '%s' supports Chinese", fontName.c_str());
    break;
}
```

### Shader System

The project uses pre-compiled SPIR-V shaders (.spv files), copied to the application sandbox directory by Index.ets at runtime:

| Shader | Purpose |
|--------|---------|
| simple_rect.vert.spv/frag.spv | Simple rectangle rendering |
| simple_rounded_rect.vert.spv/frag.spv | Rounded rectangle rendering with SDF |
| text_vert.spv/frag.spv | Text rendering |

#### Rounded Rectangle SDF Implementation

The `simple_rounded_rect` shaders implement **Inigo Quilez's SDF formula** for smooth rounded corners:

```glsl
// Fragment shader: simple_rounded_rect.frag
float sdRoundedRect(vec2 p, vec2 size, float r) {
    vec2 q = abs(p) - size + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 center = rect.pos + rect.size * 0.5;
    vec2 p = pixel_pos - center;
    vec2 half_size = rect.size * 0.5;
    float dist = sdRoundedRect(p, half_size, rect.radius);

    // Anti-aliased edge using fwidth
    float aa = fwidth(dist) * 2.0;
    float alpha = 1.0 - smoothstep(-aa, aa, dist);
    outColor = vec4(rect.color.rgb, rect.color.a * alpha);
}
```

**Push Constants Data Structure (40 bytes):**
```cpp
struct RectData {
    vec2 pos;     // Rectangle position (8 bytes)
    vec2 size;    // Rectangle size (8 bytes)
    vec4 color;   // RGBA color (16 bytes)
    float radius; // Corner radius (4 bytes)
    float is_text;// Text flag (4 bytes)
};
```

**Important: Descriptor Pool Management**

The rounded rectangle pipeline uses a **persistent descriptor set** allocated once during pipeline creation. Do NOT reset the descriptor pool in `beginFrame()`:

```cpp
// ❌ WRONG: This invalidates the descriptor set
if (m_roundedRectDescriptorPool != VK_NULL_HANDLE) {
    vkResetDescriptorPool(m_device, m_roundedRectDescriptorPool, 0);
}

// ✅ CORRECT: Do NOT reset persistent descriptor pools
// Only reset pools that allocate new descriptor sets each frame
if (m_descriptorPool != VK_NULL_HANDLE) {
    vkResetDescriptorPool(m_device, m_descriptorPool, 0);
}
// NOTE: m_roundedRectDescriptorSet is reused every frame
```

### Resource Loading

#### ArkTS Layer Resource Copying

```typescript
// entry/src/main/ets/pages/Index.ets
aboutToAppear(): void {
    let path = getContext(this).filesDir + '/rawfile';
    if (!fs.accessSync(path)) {
        fs.mkdirSync(path);
    }

    // Copy shaders
    let buffer = getContext(this).resourceManager.getRawFileContentSync('shaders/text_vert.spv');
    let file = fs.openSync(path + '/text_vert.spv', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
    fs.writeSync(file.fd, buffer.buffer);

    // Copy Chinese font
    buffer = getContext(this).resourceManager.getRawFileContentSync('msyh.ttc');
    file = fs.openSync(path + '/msyh.ttc', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
    fs.writeSync(file.fd, buffer.buffer);
}
```

#### Native Layer Shader Loading

```cpp
// entry/src/main/cpp/render/plugin_render.cpp:160
vulkanExample_->SetShaderLoader([resourceManager](const char* shaderName) -> std::vector<char> {
    // Method 1: Load from ResourceManager first
    if (resourceManager) {
        RawFile* rawFile = OH_ResourceManager_OpenRawFile(resourceManager, shaderName);
        if (rawFile) {
            long fileSize = OH_ResourceManager_GetRawFileSize(rawFile);
            std::vector<char> buffer(fileSize);
            OH_ResourceManager_ReadRawFile(rawFile, buffer.data(), fileSize);
            OH_ResourceManager_CloseRawFile(rawFile);
            return buffer;
        }
    }

    // Method 2: Load from filesystem
    std::string filePath = "/data/storage/el2/base/files/rawfile/" + fileName;
    FILE* file = fopen(filePath.c_str(), "rb");
    // ...
});
```

## Dependencies

| Library | Purpose | Version |
|---------|---------|---------|
| Vulkan | Graphics rendering API | - |
| FreeType | Font rasterization | 2.12.1 |
| GLM | Math library | - |
| libvulkan.so | OpenHarmony Vulkan driver | - |

## Permissions

This sample does not involve special system permissions.

## Constraints and Limitations

1. **Device Requirements**: This sample requires the device's underlying driver to implement [Vulkan API interface](https://gitee.com/openharmony/docs/tree/master/en/application-dev/reference/native-lib). Some development boards may not support it (requires vendor driver implementation).

2. **SDK Version**: This sample is based on OpenHarmony API 11 SDK(4.1.7.5).
   To run on a HarmonyOS project, ensure the HarmonyOS SDK version is API10 or above:
   - Create a new HarmonyOS Native C++ project
   - Copy the `AppScope/resources/rawfile` directory of this project to the new project
   - Delete the `entry/src` directory of the new project and copy the `entry` directory of this project to the new project

3. **IDE Version**: This sample requires DevEco Studio version (4.0 Release) or higher to compile and run.

## Download

To download this project separately, execute the following command:

```bash
git init
git config core.sparsecheckout true
echo code/BasicFeature/Native/NdkVulkan/ > .git/info/sparse-checkout
git remote add origin https://gitee.com/openharmony/applications_app_samples.git
git pull origin master
```

## Modification History

| Date | Changes |
|------|---------|
| 2024 | Original version: Triangle rendering example |
| 2024 | Added AgenUIEngine rendering engine |
| 2024 | Implemented rounded rectangle rendering pipeline |
| 2024 | Implemented text rendering pipeline |
| 2024 | Added Chinese font support (msyh.ttc) |
| 2024 | Fixed font spacing calculation issue |
| 2024 | Implemented text bounding box centering |
| 2024 | Cleaned up redundant shader files |
| 2024 | Implemented SDF-based rounded rectangle rendering with push constants |
| 2024 | Fixed descriptor pool management issue (GPU error/black screen bug) |

## Technical Highlights

1. **SDF Rounded Rectangles**: Inigo Quilez's formula for G2 continuous curvature corners
2. **Push Constants Optimization**: 40-byte push constants for efficient rectangle data passing
3. **Anti-aliasing**: fwidth()-based adaptive anti-aliasing for smooth edges
4. **Descriptor Pool Management**: Persistent descriptor sets vs per-frame allocation
5. **Font Spacing Normalization**: Character advance and width normalized relative to screen width
6. **Vulkan Coordinate System**: Y-axis direction is opposite to OpenGL, Y=-1 is top
7. **Text Centering**: Achieve accurate vertical centering by calculating overall bounding box
8. **Font Detection**: Test target character set support after loading fonts
9. **Multi-Pipeline Architecture**: Different graphic types use independent Vulkan pipelines
10. **Resource Management**: Shaders and fonts loaded from rawfile via ResourceManager
