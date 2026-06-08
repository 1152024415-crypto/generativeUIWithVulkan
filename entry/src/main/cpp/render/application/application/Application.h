/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include "agenui_engine/core/IGraphicsAPI.h"
#include "agenui_engine/core/RenderContext.h"
#include "dsl/DslRenderCommand.h"
#include <rawfile/raw_file_manager.h>
#include <native_window/external_window.h>
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <chrono>
#include "game/GameModule.h"

namespace application {

// Decorator shape types for flow_text ornaments
enum class DecoratorShape {
    RoundedRect,
    Image,
    Sphere
};

// Decorator animation types
enum class DecoratorAnimation {
    None,
    Float,
    Pulse,
    Spin,
    Orbit
};

// Flow text decorator data (replaces FlowOrb)
struct FlowDecorator {
    // Core (backward-compatible with old FlowOrb JSON)
    float cx, cy, radius;
    glm::vec3 color;

    // Shape configuration
    DecoratorShape shape = DecoratorShape::RoundedRect;
    float cornerRadius = 20.0f;
    std::string src;        // image source path (for Image shape)
    float metallic = 0.0f;  // PBR metallic factor (for Sphere)
    float roughness = 0.5f; // PBR roughness factor (for Sphere)

    // Animation
    DecoratorAnimation animation = DecoratorAnimation::Float;
    float animSpeed = 1.0f;
    float animAmplitude = 40.0f;

    // Runtime state (updated each frame by animation)
    float currentCx, currentCy;
    float currentScale = 1.0f;
    float currentRotation = 0.0f;
};

class Application {
public:
    Application();
    ~Application();

    bool InitRenderer();
    void SetupWindow(NativeWindow* nativeWindow);
    void SetSurfaceSize(int width, int height);
    bool RequestRender();  // Returns true if frame was actually rendered
    bool IsInited() const;
    void SetRecreateSwapChain();
    void SetResourceManager(NativeResourceManager* resourceManager);
    void ParseUIDescriptor(const std::string& descriptor);
    void SetRotation(float rotationX, float rotationY);
    void SetLandscapeMode(bool landscape);
    void SetAutoFixEnabled(bool enabled);
    void SetRenderMode(bool gameMode);
    void SetSurfaceTransitioning(bool transitioning);
    void SetDesignDimensions(int width, int height);
    void SetDensity(float density);
    void SetSurfaceDestroyed(bool destroyed);
    bool IsGameMode() const { return m_gameMode; }
    bool HasActiveAnimations() const;
    std::string GetGameState() const;

    // Stream text incremental update (skips full DSL parse)
    void UpdateStreamTextContent(const std::string& componentId, const std::string& text);

    // Touch interaction
    void HandleTouchEvent(float touchX, float touchY);
    void HandleTouchMove(float touchX, float touchY);
    void ClearPressedState();
    void SetActionInProgress(bool inProgress);
    using ActionCallback = std::function<void(const std::string& action, const std::string& currentDSL)>;
    void SetActionCallback(ActionCallback callback);

private:
    void loadFontsFromConfig();  // ResourceManager-driven font registration
    bool m_inited = false;
    bool m_shouldRecreate = false;

    // AgenUIEngine render context (facade over queue + renderer)
    std::unique_ptr<AgenUIEngine::Core::RenderContext> m_context;

    // Window
    NativeWindow* m_window = nullptr;
    int m_width = 1024;
    int m_height = 1024;
    int m_surfaceWidth = 0;   // Actual XComponent surface size (for touch mapping)
    int m_surfaceHeight = 0;

    // ResourceManager for loading rawfile resources (fonts, etc.)
    NativeResourceManager* m_resourceManager = nullptr;

    // Pending UI descriptor to render
    std::string m_pendingUIDescriptor;
    bool m_hasPendingDescriptor = false;

    // Cached parsed DSL commands (avoids re-parsing every frame)
    std::vector<dsl::DslRenderCommand> m_cachedDslCommands;
    bool m_dslCommandsCached = false;

    // Design dimensions for DSL coordinate mapping
    uint32_t m_designWidth = 1276;
    uint32_t m_designHeight = 2400;
    float m_density = 0.0f;

    // Landscape mode state
    bool m_isLandscape = false;
    bool m_surfaceTransitioning = false;  // True during screen rotation
    bool m_surfaceDestroyed = false;      // True after surface destroyed, blocks all Vulkan
    bool m_autoFixEnabled = false;         // Auto-fix for V2 DSL overflow/overlap
    bool m_explicitGameMode = false;        // Set by ArkTS routing; avoids descriptor sniffing

    // Touch-based rotation state (set from ArkTS UI)
    float m_touchRotationX = 0.3f;  // Initial tilt angle
    float m_touchRotationY = 0.0f;  // Initial horizontal rotation

    // Flow text decorator state
    std::vector<FlowDecorator> m_decorators;
    float m_animTime = 0.0f;
    std::chrono::steady_clock::time_point m_lastFrameTime;

    // Stream text (typewriter effect) animation state
    struct StreamTextAnim {
        std::string key;
        uint32_t totalGlyphs = 0;
        uint32_t visibleGlyphs = 0;
        float elapsedMs = 0.0f;
        float speedMs = 0.0f;
        float maxWidth = 0.0f;
        bool prepared = false;
        bool finished = false;
        bool styledText = false;          // true when using TextBlock path (markdown, etc.)
        glm::vec2 pos;
        uint32_t fontSize = 0;
        std::string fontFamily;
        std::string text;                 // raw text (for plain path)
        std::vector<AgenUIEngine::Core::TextBlock> textBlocks; // pre-parsed blocks (for styled path)
        glm::vec3 color;
    };
    std::vector<StreamTextAnim> m_streamTextAnims;

    // Touch interaction
    std::vector<dsl::DslRenderCommand> m_renderedCommands;
    ActionCallback m_actionCallback;
    std::string m_pressedComponentId;  // Currently pressed component (for click animation)
    bool m_actionInProgress = false;   // Prevents re-entry while LLM is processing

    // Game mode
    std::unique_ptr<game::GameModule> m_gameModule;
    bool m_gameMode = false;
    std::vector<dsl::DslRenderCommand> m_gameStandardCommands;  // Cached standard DSL commands
};

} // namespace application
#endif // APPLICATION_H
