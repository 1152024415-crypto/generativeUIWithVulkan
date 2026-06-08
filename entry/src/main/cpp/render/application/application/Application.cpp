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

#include "application/Application.h"
#include "dsl/DslRenderRegistry.h"
#include "dsl/DslRenderCommand.h"
#include "dsl/GameDslRender.h"
#include "dsl/MdParser.h"
#include "dsl/MdStyleMapper.h"
#include "FontConfig.h"
#include "agenui_engine/modules/text/FontManager.h"
#include "logger_common.h"
#include "perf_timer.h"
#include "../agenui_engine/thirdparty/glm/glm.hpp"
#include "../agenui_engine/thirdparty/glm/gtc/matrix_transform.hpp"
#include "agenui_engine/thirdparty/json.hpp"
#include <cmath>
#include <cstdio>
#include <chrono>
#include <rawfile/raw_file.h>
#include <hitrace/trace.h>


namespace application {

// Helper function to parse vec2 from a JSON array [x, y]
static glm::vec2 ParseVec2(const nlohmann::json& j) {
    if (j.is_array() && j.size() >= 2) {
        return glm::vec2(j[0].get<float>(), j[1].get<float>());
    }
    return glm::vec2(0.0f, 0.0f);
}

// Helper function to parse hex color to vec3
static glm::vec3 ParseColor(const std::string& hexColor) {
    glm::vec3 result(1.0f, 1.0f, 1.0f);

    std::string temp = hexColor;
    // Remove # if present
    if (!temp.empty() && temp[0] == '#') {
        temp = temp.substr(1);
    }

    // Parse RGB
    if (temp.length() == 6) {
        result.r = std::stoul(temp.substr(0, 2), nullptr, 16) / 255.0f;
        result.g = std::stoul(temp.substr(2, 2), nullptr, 16) / 255.0f;
        result.b = std::stoul(temp.substr(4, 2), nullptr, 16) / 255.0f;
    }

    return result;
}

static bool ContainsMarkdownStructuralChar(const std::string& text)
{
    for (char c : text) {
        switch (c) {
            case '\n':
            case '\r':
            case '#':
            case '*':
            case '_':
            case '`':
            case '[':
            case ']':
            case '(':
            case ')':
            case '|':
            case '>':
            case '-':
            case '+':
            case '!':
                return true;
            default:
                break;
        }
    }
    return false;
}

static bool TryAppendMarkdownTextBlocks(
    std::vector<AgenUIEngine::Core::TextBlock>& textBlocks,
    const std::string& oldText,
    const std::string& newText)
{
    if (oldText.empty() || newText.size() <= oldText.size() ||
        newText.compare(0, oldText.size(), oldText) != 0) {
        return false;
    }

    std::string suffix = newText.substr(oldText.size());
    if (suffix.empty() || ContainsMarkdownStructuralChar(suffix)) {
        return false;
    }

    if (textBlocks.empty()) {
        return false;
    }

    auto& block = textBlocks.back();
    if (block.skipRender || block.isHorizontalRule || block.isTableRow || block.segments.empty()) {
        return false;
    }

    block.segments.back().text += suffix;
    return true;
}

void Application::SetupWindow(NativeWindow* nativeWindow)
{
    m_window = nativeWindow;
}

void Application::SetSurfaceSize(int width, int height)
{
    m_width = width;
    m_height = height;
    m_surfaceWidth = width;
    m_surfaceHeight = height;
    // Don't recreate swapchain here — it will be recreated in RequestRender()
    // when m_shouldRecreate is set. Recreating during surface transition causes
    // SIGSEGV in vkCreateSwapchainKHR.
}

Application::Application()
{
}

Application::~Application()
{
    m_context.reset();
}

bool Application::InitRenderer()
{
    // Create render context using factory (API-agnostic)
    m_context = AgenUIEngine::Core::RenderFactory::createRenderContext(
        AgenUIEngine::Core::GraphicsAPI::Vulkan
    );

    if (!m_context) {
        LOGE("Failed to create render context");
        return false;
    }

    // Initialize the renderer through the context
    AgenUIEngine::Core::RendererInitParams params;
    params.nativeWindow = m_window;
    params.width = m_width;
    params.height = m_height;

    if (!m_context->initialize(params)) {
        LOGE("Failed to initialize render context");
        return false;
    }

    // Set resource manager for HarmonyOS rawfile access
    if (m_resourceManager) {
        m_context->setResourceManager(m_resourceManager);
    }

    // Create all rendering pipelines (rect, rounded rect, text, image)
    if (!m_context->createPipelines()) {
        LOGE("Failed to create rendering pipelines");
        return false;
    }

    // Initialize fonts for text rendering (bootstrap: creates FontManager, atlas, text pipeline)
    const char* fontName = "fonts/HarmonyOS_Sans_SC.ttf";  // HarmonyOS Sans SC
    if (!m_context->initializeFonts(m_resourceManager, fontName)) {
        LOGW("Failed to load font '%s', text rendering will not work", fontName);
    } else {
        LOGI("Successfully loaded font '%s'", fontName);
    }

    m_inited = true;

    // Load fonts from config (uses sandbox paths, needs m_inited=true)
    loadFontsFromConfig();

    return true;
}

bool Application::IsInited() const
{
    return m_inited;
}

void Application::SetRecreateSwapChain()
{
    m_shouldRecreate = true;
}

void Application::SetResourceManager(NativeResourceManager* resourceManager)
{
    m_resourceManager = resourceManager;
    // If context is already created, set it immediately
    if (m_context) {
        m_context->setResourceManager(resourceManager);
    }
}

void Application::ParseUIDescriptor(const std::string& descriptor)
{
    if (m_explicitGameMode) {
        try {
            auto json = nlohmann::json::parse(descriptor, nullptr, false);
            m_gameMode = true;
            m_pendingUIDescriptor.clear();
            m_hasPendingDescriptor = false;
            m_gameStandardCommands.clear();

            float designW = m_designWidth > 0 ? static_cast<float>(m_designWidth) : static_cast<float>(m_width);
            float designH = m_designHeight > 0 ? static_cast<float>(m_designHeight) : static_cast<float>(m_height);

            // GameModule handles targets/crosshair/HUD — use CustomV2DslRender
            // directly to avoid GameDslRender matching and duplicating targets.
            dsl::ParseContext parseCtx;
            parseCtx.designWidth = designW;
            parseCtx.designHeight = designH;
            static const dsl::CustomV2DslRender v2Renderer;
            m_gameStandardCommands = v2Renderer.Parse(json, parseCtx);

            // Extract game config for GameModule
            auto gameConfig = dsl::GameDslRender::ExtractGameConfig(json);
            nlohmann::json gcJson;
            if (!gameConfig.targetConfig.empty()) gcJson["target"] = gameConfig.targetConfig;
            if (!gameConfig.crosshairConfig.empty()) gcJson["crosshair"] = gameConfig.crosshairConfig;
            gcJson["updateDataModel"]["duration"] = gameConfig.duration;

            // Extract background image path for fragment texturing
            if (json.contains("updateComponents") && json["updateComponents"].contains("components")) {
                for (const auto& comp : json["updateComponents"]["components"]) {
                    if (comp.value("type", std::string("")) == "img" && comp.contains("src")) {
                        gcJson["_backgroundSrc"] = comp["src"].get<std::string>();
                        break;
                    }
                }
            }

            m_gameModule = std::make_unique<game::GameModule>();
            m_gameModule->InitFromStyle(gcJson.dump(), designW, designH);
            LOGI("Game mode initialized with design %.0fx%.0f, %zu standard commands",
                 designW, designH, m_gameStandardCommands.size());

            // Pre-cache all glyphs the game will use to avoid run-time atlas upload stalls
            if (m_context) {
                // Scale font size to match render resolution
                float swapW = static_cast<float>(m_width);
                float swapH = static_cast<float>(m_height);
                float refW = m_designWidth > 0 ? static_cast<float>(m_designWidth) : swapW;
                float refH = m_designHeight > 0 ? static_cast<float>(m_designHeight) : swapH;
                float scale = (refW / refH < swapW / swapH) ? (swapH / refH) : (swapW / refW);
                // Cache at a few font sizes used by the game (48 HUD, 36 combo, ~72 labels)
                static const std::string gameChars =
                    "Score: 0123456789Combo xs"
                    "GOODGREATEXCELLENTBULLSEYE"
                    "MISS+-GamOvri"
                    "!.,:";
                uint32_t sizes[] = {36, 48, 56, 72, 88};
                for (uint32_t sz : sizes) {
                    m_context->precacheText(gameChars, static_cast<uint32_t>(sz * scale));
                }
            }

            return;
        }
        catch (const nlohmann::json::exception& e) {
            LOGE("ParseUIDescriptor: game JSON parse failed - %s", e.what());
            return;
        }
    }

    m_gameMode = false;
    m_gameModule.reset();

    LOGI("ParseUIDescriptor: Storing descriptor with length %zu", descriptor.length());
    m_pendingUIDescriptor = descriptor;
    m_hasPendingDescriptor = true;
    m_dslCommandsCached = false;  // Invalidate cache — new descriptor needs re-parse
    m_pressedComponentId.clear();  // Clear pressed state on new content

    // Clear stream text animations and layout cache for new descriptor
    m_streamTextAnims.clear();
    if (m_context) m_context->clearTextLayoutCache();
}

void Application::UpdateStreamTextContent(const std::string& componentId, const std::string& text)
{
    for (auto& anim : m_streamTextAnims) {
        if (anim.key != componentId) continue;

        const std::string oldText = anim.text;
        anim.text = text;
        if (anim.styledText) {
            if (!TryAppendMarkdownTextBlocks(anim.textBlocks, oldText, anim.text)) {
                auto mdBlocks = dsl::parseMarkdown(anim.text);
                anim.textBlocks = dsl::mapMdToTextBlocks(mdBlocks);
            }
        }
        anim.prepared = false;
        anim.finished = false;
        anim.visibleGlyphs = 0;
        anim.totalGlyphs = 0;

        LOGD("UpdateStreamText: key='%s' textLen=%zu", componentId.c_str(), text.size());
        return;
    }
    LOGW("UpdateStreamText: anim '%s' not found", componentId.c_str());
}

// Single frame rendering triggered by NAPI
bool Application::RequestRender()
{
    if (!IsInited()) {
        LOGE("RequestRender: Not inited!");
        return false;
    }

    // Don't render after surface has been destroyed.
    if (m_surfaceDestroyed) {
        LOGI("RequestRender: surface destroyed, skipping");
        return false;
    }

    // Don't render during surface transition (screen rotation).
    // The surface may be invalid, causing SIGSEGV in Vulkan API calls.
    if (m_surfaceTransitioning) {
        LOGI("RequestRender: skipping, surface transitioning");
        return false;
    }

    if (m_shouldRecreate) {
        // Propagate requested dimensions to VkRenderer before swapchain recreation.
        m_context->updateSurfaceSize(m_width, m_height);
        m_context->recreateSwapChain();

        // Read back the ACTUAL swapchain extent after recreation.
        int actualW = m_context->getWidth();
        int actualH = m_context->getHeight();

        LOGI("RequestRender: recreate done, actual=%dx%d (requested=%dx%d, landscape=%d)",
             actualW, actualH, m_width, m_height, m_isLandscape);

        // If actual extent is 0 (recreation failed), skip and retry next time.
        if (actualW <= 0 || actualH <= 0) {
            LOGE("RequestRender: swapchain recreation produced invalid extent, will retry");
            return false;
        }

        // Update surface dimensions to actual swapchain extent.
        // Do NOT update m_designWidth/m_designHeight here — those are set by
        // SetDesignDimensions and should reflect the logical design space (e.g.,
        // landscape 2400x1276), not the physical swapchain extent which may differ
        // slightly due to surface capability clamping.
        m_width = actualW;
        m_height = actualH;
        m_shouldRecreate = false;
    }

    // Set coordinate mapping to the content's design dimensions.
    // m_designWidth/m_designHeight are set by SetDesignDimensions (from JS) and
    // reflect the logical design space: portrait (1276x2400) or landscape (2400x1276).
    // pixelToNDC uses these to map DSL pixel coordinates to NDC correctly.
    m_context->setCoordinateMapping(
        m_designWidth > 0 ? static_cast<int>(m_designWidth) : m_width,
        m_designHeight > 0 ? static_cast<int>(m_designHeight) : m_height);

    // Set background color based on mode
    if (m_gameMode) {
        m_context->setClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    } else {
        m_context->setClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    m_context->beginFrame();

    // --- Draw commands in DSL order ---
    // When a glass rect is encountered:
    //   1. flush everything drawn so far
    //   2. copy framebuffer → bg texture (for refraction)
    //   3. draw glass rect
    //   4. flush, then resume with LOAD_OP_LOAD for subsequent elements
    m_context->beginQueue();

    // Helper: flush current batch, draw one glass rect, resume
    auto drawGlassInline = [&](const glm::vec2& pos, const glm::vec2& size,
                                float radius, const glm::vec3& color) {
        // Flush everything drawn before this glass rect
        m_context->endQueue();
        m_context->flushQueue();

        // Copy current framebuffer to bg texture, then draw glass
        m_context->prepareGlassPass();
        m_context->beginQueue();
        LOGI("Glass: rect at (%.0f,%.0f) size (%.0f,%.0f) radius=%.0f",
             pos.x, pos.y, size.x, size.y, radius);
        m_context->drawGlassRoundedRect(pos, size, radius, color);
        m_context->endQueue();
        m_context->flushQueue();

        // Resume: LOAD_OP_LOAD render pass for elements after glass
        m_context->prepareGlassPass();
        m_context->beginQueue();
    };

    // Calculate real delta time for frame-rate-independent animation
    auto now = std::chrono::steady_clock::now();
    float deltaTimeMs = 16.0f;  // Default fallback
    if (m_lastFrameTime.time_since_epoch().count() > 0) {
        deltaTimeMs = std::chrono::duration<float, std::milli>(now - m_lastFrameTime).count();
        // Clamp to avoid large jumps after pause/resume
        deltaTimeMs = std::min(deltaTimeMs, 100.0f);
    }
    m_lastFrameTime = now;

    // Update animation time every frame (shared by glass floating + flow_text decorators)
    m_animTime += deltaTimeMs / 1000.0f;

    // Update stream text (typewriter) animations with real delta time
    for (auto& anim : m_streamTextAnims) {
        if (!anim.finished) {
            if (anim.speedMs <= 0.0f) {
                // No speed set (LLM streaming): render all content instantly
                // once layout is prepared. visibleGlyphs is set after
                // prepareTextLayout() so totalGlyphs is accurate.
                if (anim.prepared) {
                    anim.visibleGlyphs = anim.totalGlyphs;
                    anim.finished = true;
                }
            } else {
                anim.elapsedMs += deltaTimeMs;
                anim.visibleGlyphs = std::min(
                    static_cast<uint32_t>(anim.elapsedMs / anim.speedMs),
                    anim.totalGlyphs);
                if (anim.visibleGlyphs >= anim.totalGlyphs)
                    anim.finished = true;
            }
        }
    }

    // Game mode rendering path
    if (m_gameMode && m_gameModule) {
        using Clock = std::chrono::steady_clock;
        auto t_phase1_start = Clock::now();
        // Phase 1: Render background image (must be drawn first, before game elements)
        // Flush the background in its own queue so it's always the bottom layer.
        for (const auto& cmd : m_gameStandardCommands) {
            switch (cmd.type) {
                case dsl::DslRenderCommand::Type::Image:
                    m_context->drawImage(cmd.imagePath, cmd.pos, cmd.size);
                    break;
                default:
                    break;
            }
        }
        m_context->endQueue();
        m_context->flushQueue();

        // Phase 2: Render game elements (targets, crosshair, particles, text, etc.)
        auto t_phase2_start = Clock::now();
        float phase1Ms = std::chrono::duration<float, std::milli>(t_phase2_start - t_phase1_start).count();
        m_context->beginQueue();
        for (const auto& cmd : m_gameStandardCommands) {
            switch (cmd.type) {
                case dsl::DslRenderCommand::Type::Image:
                    break;  // Already rendered in phase 1
                case dsl::DslRenderCommand::Type::Rect:
                    m_context->drawRoundedRect(cmd.pos, cmd.size, cmd.radius,
                        glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b), cmd.color.a);
                    break;
                case dsl::DslRenderCommand::Type::Circle:
                    m_context->drawCircle(cmd.pos, cmd.circleRadius,
                        glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b), cmd.color.a);
                    break;
                case dsl::DslRenderCommand::Type::Text:
                    m_context->drawText(cmd.text, cmd.pos, cmd.fontSize,
                        glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b),
                        cmd.fontFamily);
                    break;
                default:
                    break;
            }
        }

        // Render dynamic game elements
        m_gameModule->RenderFrame(m_context.get());

        // Flush remaining game commands (the actual GPU submission)
        m_context->endQueue();
        m_context->flushQueue();

        if (g_perfEnabled) {
            auto t_phase2_end = Clock::now();
            float phase2Ms = std::chrono::duration<float, std::milli>(t_phase2_end - t_phase2_start).count();
            if (phase1Ms + phase2Ms > 5.0f) {
                PERF_LOG("[PERF] game frame: phase1(bg)=%.1f phase2(draw)=%.1f total=%.1f",
                         phase1Ms, phase2Ms, phase1Ms + phase2Ms);
            }
        }
    }
    // Process pending UI descriptor if available (non-game mode)
    else if (!m_pendingUIDescriptor.empty()) {

        // --- Cached DSL parsing: only re-parse when descriptor changes ---
        if (!m_dslCommandsCached) {
            dsl::ParseContext parseCtx;
            parseCtx.designWidth = static_cast<float>(m_designWidth > 0 ? m_designWidth : m_width);
            parseCtx.designHeight = static_cast<float>(m_designHeight > 0 ? m_designHeight : m_height);
            parseCtx.autoFix = m_autoFixEnabled;
            parseCtx.density = m_density;

            OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:ParseDSL", "");
            PerfTimer _("AgenUI:ParseDSL");

            try {
                m_cachedDslCommands = dsl::DslRenderRegistry::Parse(m_pendingUIDescriptor, parseCtx);
            } catch (const nlohmann::json::exception& e) {
                LOGE("DSL registry parse error: %s", e.what());
                m_cachedDslCommands.clear();
            }
            OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
            m_dslCommandsCached = true;
        }

        const auto& dslCommands = m_cachedDslCommands;

        if (!dslCommands.empty()) {
            LOGI("RequestRender: DSL registry returned %zu commands", dslCommands.size());
            m_renderedCommands = dslCommands;  // Store for hit testing

            // DSL rendering: draw each element in definition order.
            OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:DrawCommands", "");
            for (const auto& cmd : dslCommands) {
                switch (cmd.type) {
                    case dsl::DslRenderCommand::Type::Image:
                        // Flush any non-Image commands queued so far, draw the Image,
                        // then start a fresh queue for subsequent commands.
                        m_context->endQueue();
                        m_context->flushQueue();
                        m_context->beginQueue();
                        LOGI("DSL: image '%s' at (%.0f,%.0f) rot=%.1f clip=%zu",
                             cmd.imagePath.c_str(), cmd.pos.x, cmd.pos.y,
                             cmd.rotation, cmd.clipVertices.size());
                        m_context->drawImage(cmd.imagePath, cmd.pos, cmd.size,
                            cmd.rotation, cmd.clipVertices, cmd.clipCenter);
                        m_context->endQueue();
                        m_context->flushQueue();
                        m_context->beginQueue();
                        break;
                    case dsl::DslRenderCommand::Type::Rect:
                        if (cmd.glass) {
                            // Floating animation for glass rect position
                            float floatX = cmd.componentId.empty() ? std::sin(m_animTime * 0.8f) * 30.0f : 0.0f;
                            float floatY = cmd.componentId.empty() ? std::cos(m_animTime * 0.6f) * 20.0f : 0.0f;
                            glm::vec2 animPos = cmd.pos + glm::vec2(floatX, floatY);
                            drawGlassInline(animPos, cmd.size, cmd.radius,
                                glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b));
                        } else {
                            LOGI("DSL: rect at (%.0f,%.0f) size (%.0f,%.0f) radius=%.0f",
                                 cmd.pos.x, cmd.pos.y, cmd.size.x, cmd.size.y, cmd.radius);
                            m_context->drawRoundedRect(cmd.pos, cmd.size, cmd.radius,
                                glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b));
                        }
                        // Click animation overlay: subtle dark press feedback.
                        if (!m_pressedComponentId.empty() &&
                            cmd.componentId == m_pressedComponentId && cmd.action.empty() == false) {
                            m_context->drawRoundedRect(cmd.pos, cmd.size, cmd.radius,
                                glm::vec3(0.0f, 0.0f, 0.0f), 0.12f);
                        }
                        break;
                    case dsl::DslRenderCommand::Type::Polygon:
                        LOGI("DSL: polygon center=(%.0f,%.0f) vertices=%zu",
                             cmd.polygonCenter.x, cmd.polygonCenter.y, cmd.polygonVertices.size());
                        m_context->drawPolygon(cmd.polygonCenter, cmd.polygonVertices,
                            glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b), cmd.color.a);
                        break;
                    case dsl::DslRenderCommand::Type::Circle:
                        LOGI("DSL: circle center=(%.0f,%.0f) radius=%.0f",
                             cmd.pos.x, cmd.pos.y, cmd.circleRadius);
                        m_context->drawCircle(cmd.pos, cmd.circleRadius,
                            glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b), cmd.color.a);
                        if (!m_pressedComponentId.empty() &&
                            cmd.componentId == m_pressedComponentId && cmd.action.empty() == false) {
                            m_context->drawCircle(cmd.pos, cmd.circleRadius,
                                glm::vec3(0.0f, 0.0f, 0.0f), 0.12f);
                        }
                        break;
                    case dsl::DslRenderCommand::Type::Text:
                        LOGI("DSL: text '%s' at (%.0f,%.0f) fontSize=%u fontFamily='%s' glowW=%.1f glowI=%.2f grad=%d strokeW=%.1f stream=%d",
                             cmd.text.c_str(), cmd.pos.x, cmd.pos.y, cmd.fontSize,
                             cmd.fontFamily.c_str(), cmd.glowWidth, cmd.glowIntensity,
                             cmd.hasGradient, cmd.strokeWidth, cmd.streamText);
                        if (cmd.streamText) {
                            // Typewriter effect: register animation instead of immediate draw
                            StreamTextAnim* existingAnim = nullptr;
                            for (auto& a : m_streamTextAnims) {
                                if (a.key == cmd.componentId) { existingAnim = &a; break; }
                            }
                            if (!existingAnim) {
                                StreamTextAnim anim;
                                anim.key = cmd.componentId;
                                anim.text = cmd.text;
                                anim.pos = cmd.pos;
                                anim.fontSize = cmd.fontSize;
                                anim.fontFamily = cmd.fontFamily;
                                anim.color = glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b);
                                anim.speedMs = cmd.streamTextSpeed;
                                anim.maxWidth = cmd.streamTextMaxWidth;
                                anim.styledText = cmd.markdown;
                                if (cmd.markdown) {
                                    auto mdBlocks = dsl::parseMarkdown(cmd.text);
                                    OH_HiTrace_CountTraceEx(HITRACE_LEVEL_INFO, "AgenUI:MdBlockCount",
                                                            static_cast<int64_t>(mdBlocks.size()));
                                    anim.textBlocks = dsl::mapMdToTextBlocks(mdBlocks);
                                }
                                m_streamTextAnims.push_back(anim);
                                existingAnim = &m_streamTextAnims.back();
                            }
                            if (!existingAnim->prepared) {
                                if (existingAnim->styledText) {
                                    LOGD("[Styled] App: prepareStyledText key='%s' blocks=%zu",
                                         existingAnim->key.c_str(), existingAnim->textBlocks.size());
                                    existingAnim->totalGlyphs = m_context->prepareTextLayout(
                                        existingAnim->key, existingAnim->textBlocks, existingAnim->pos,
                                        existingAnim->fontSize, existingAnim->maxWidth);
                                    LOGD("[Styled] App: result totalGlyphs=%u", existingAnim->totalGlyphs);
                                } else {
                                    existingAnim->totalGlyphs = m_context->prepareTextLayout(
                                        existingAnim->key, existingAnim->text, existingAnim->pos,
                                        existingAnim->fontSize, existingAnim->fontFamily,
                                        existingAnim->maxWidth);
                                }
                                existingAnim->prepared = true;
                                // speed=0: show all glyphs immediately after layout
                                if (existingAnim->speedMs <= 0.0f) {
                                    existingAnim->visibleGlyphs = existingAnim->totalGlyphs;
                                    existingAnim->finished = true;
                                }
                            }
                            // Queue the partial text draw (maintains z-order with other queued commands)
                            m_context->drawTextPartial(existingAnim->key, existingAnim->color,
                                                        existingAnim->visibleGlyphs);
                        } else {
                            if (cmd.glowWidth > 0.0f) {
                                m_context->setTextGlow(cmd.glowWidth, cmd.glowIntensity);
                            }
                            if (cmd.hasGradient) {
                                m_context->setTextGradient(true, cmd.gradientEndColor, cmd.gradientDirection);
                            }
                            if (cmd.strokeWidth > 0.0f) {
                                m_context->setTextStroke(cmd.strokeWidth, cmd.strokeColor);
                            }
                            m_context->drawText(cmd.text, cmd.pos, cmd.fontSize,
                                glm::vec3(cmd.color.r, cmd.color.g, cmd.color.b),
                                cmd.fontFamily);
                            if (cmd.glowWidth > 0.0f) {
                                m_context->setTextGlow(0.0f, 0.0f);  // Reset glow
                            }
                            if (cmd.hasGradient) {
                                m_context->setTextGradient(false, glm::vec3(0.0f), 0);
                            }
                            if (cmd.strokeWidth > 0.0f) {
                                m_context->setTextStroke(0.0f, glm::vec3(0.0f));
                            }
                        }
                        break;
                }
            }
            OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
        } else {
        // --- Fallback: inline JSON parser (handles flow_text, multiline) ---
        OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:DrawCommands", "");
        try {
            auto objects = nlohmann::json::parse(m_pendingUIDescriptor, nullptr, true, false, true);

            for (size_t objIdx = 0; objIdx < objects.size(); objIdx++) {
                const auto& obj = objects[objIdx];
                std::string type = obj.value("type", "");

                if (type == "rect") {
                    glm::vec2 pos = ParseVec2(obj["pos"]);
                    glm::vec2 size = ParseVec2(obj["size"]);
                    std::string colorStr = obj.value("Color", "#FFFFFF");
                    glm::vec3 color = ParseColor(colorStr);
                    float radius = obj.value("radius", 0.0f);
                    bool glass = obj.value("glass", false);
                    if (glass) {
                        // Floating animation for glass rect position
                        float floatX = std::sin(m_animTime * 0.8f) * 30.0f;
                        float floatY = std::cos(m_animTime * 0.6f) * 20.0f;
                        glm::vec2 animPos = pos + glm::vec2(floatX, floatY);
                        drawGlassInline(animPos, size, radius, color);
                    } else {
                        LOGI("Drawing rect at (%.0f,%.0f) size (%.0f,%.0f)", pos.x, pos.y, size.x, size.y);
                        m_context->drawRoundedRect(pos, size, radius, color);
                    }

                } else if (type == "polygon") {
                    if (!obj.contains("center") || !obj.contains("vertices")) continue;
                    glm::vec2 center = ParseVec2(obj["center"]);
                    std::vector<glm::vec2> vertices;
                    if (obj["vertices"].is_array()) {
                        for (const auto& v : obj["vertices"]) {
                            if (v.is_array() && v.size() >= 2) {
                                vertices.push_back(glm::vec2(v[0].get<float>(), v[1].get<float>()));
                            }
                        }
                    }
                    if (vertices.size() < 3) continue;
                    std::string colorStr = obj.value("Color", "#FFFFFF");
                    glm::vec3 color = ParseColor(colorStr);
                    LOGI("Drawing polygon center=(%.0f,%.0f) vertices=%zu",
                         center.x, center.y, vertices.size());
                    m_context->drawPolygon(center, vertices, color);

                } else if (type == "text") {
                    std::string content = obj.value("Content", "");
                    glm::vec2 pos = ParseVec2(obj["pos"]);
                    uint32_t fontSize = obj.value("fontSize", 32);
                    std::string colorStr = obj.value("Color", "#FFFFFF");
                    glm::vec3 color = ParseColor(colorStr);
                    float glowWidth = obj.value("glowWidth", 0.0f);
                    float glowIntensity = obj.value("glowIntensity", 0.0f);

                    // Check for streamText (typewriter effect)
                    bool hasStreamText = obj.contains("streamText");
                    float streamSpeed = 0.0f;
                    float streamMaxWidth = 0.0f;
                    if (hasStreamText && obj["streamText"].is_object()) {
                        streamSpeed = obj["streamText"].value("speed", 0.0f);
                        streamMaxWidth = obj["streamText"].value("maxWidth", 0.0f);
                    } else if (hasStreamText && obj["streamText"].is_number()) {
                        streamSpeed = obj["streamText"].get<float>();
                    }

                    if (hasStreamText && !content.empty()) {
                        LOGI("Drawing stream text '%s' at (%.0f,%.0f) speed=%.0f",
                             content.c_str(), pos.x, pos.y, streamSpeed);
                        std::string animKey = "inline_" + std::to_string(objIdx);
                        StreamTextAnim* existingAnim = nullptr;
                        for (auto& a : m_streamTextAnims) {
                            if (a.key == animKey) { existingAnim = &a; break; }
                        }
                        if (!existingAnim) {
                            StreamTextAnim anim;
                            anim.key = animKey;
                            anim.text = content;
                            anim.pos = pos;
                            anim.fontSize = fontSize;
                            anim.color = color;
                            anim.speedMs = streamSpeed;
                            anim.maxWidth = streamMaxWidth;
                            m_streamTextAnims.push_back(anim);
                            existingAnim = &m_streamTextAnims.back();
                        }
                        if (!existingAnim->prepared) {
                            existingAnim->totalGlyphs = m_context->prepareTextLayout(
                                existingAnim->key, existingAnim->text, existingAnim->pos,
                                existingAnim->fontSize, "default",
                                existingAnim->maxWidth);
                            existingAnim->prepared = true;
                            // speed=0: show all glyphs immediately after layout
                            if (existingAnim->speedMs <= 0.0f) {
                                existingAnim->visibleGlyphs = existingAnim->totalGlyphs;
                                existingAnim->finished = true;
                            }
                        }
                        // Queue the partial text draw (maintains z-order)
                        m_context->drawTextPartial(existingAnim->key, existingAnim->color,
                                                    existingAnim->visibleGlyphs);
                    } else {
                        LOGI("Drawing text '%s' at (%.0f,%.0f)", content.c_str(), pos.x, pos.y);
                        if (glowWidth > 0.0f) {
                            m_context->setTextGlow(glowWidth, glowIntensity);
                        }
                        m_context->drawText(content, pos, fontSize, color);
                        if (glowWidth > 0.0f) {
                            m_context->setTextGlow(0.0f, 0.0f);
                        }
                    }

                } else if (type == "multiline") {
                    std::string content = obj.value("Content", "");
                    glm::vec2 pos = ParseVec2(obj["pos"]);
                    uint32_t fontSize = obj.value("fontSize", 32);
                    std::string colorStr = obj.value("Color", "#FFFFFF");
                    glm::vec3 color = ParseColor(colorStr);
                    float maxWidth = obj.value("maxWidth", 900.0f);
                    float lineHeight = obj.value("lineHeight", 60.0f);

                    LOGI("Drawing multiline text '%s' at (%.0f,%.0f), maxWidth=%.0f, lineHeight=%.0f",
                         content.c_str(), pos.x, pos.y, maxWidth, lineHeight);
                    m_context->drawMultiLineText(content, pos, fontSize, color, maxWidth, lineHeight);

                } else if (type == "img") {
                    std::string src = obj.value("src", "");
                    glm::vec2 pos = ParseVec2(obj["pos"]);
                    glm::vec2 size = ParseVec2(obj["size"]);

                    // Optional rotation (degrees)
                    float imgRotation = obj.value("rotation", 0.0f);

                    // Optional polygon clip vertices
                    std::vector<glm::vec2> imgClipVerts;
                    if (obj.contains("clipVertices") && obj["clipVertices"].is_array()) {
                        for (const auto& v : obj["clipVertices"]) {
                            if (v.is_array() && v.size() >= 2) {
                                imgClipVerts.push_back(glm::vec2(v[0].get<float>(), v[1].get<float>()));
                            }
                        }
                    }

                    // Optional clip center
                    glm::vec2 imgClipCenter(0.0f, 0.0f);
                    if (obj.contains("clipCenter")) {
                        imgClipCenter = ParseVec2(obj["clipCenter"]);
                    }

                    LOGI("Drawing image '%s' at (%.0f,%.0f) rot=%.1f clip=%zu",
                         src.c_str(), pos.x, pos.y, imgRotation, imgClipVerts.size());
                    m_context->drawImage(src, pos, size, imgRotation, imgClipVerts, imgClipCenter);

                } else if (type == "flow_text") {
                    std::string content = obj.value("Content", "");
                    glm::vec2 pos = ParseVec2(obj["pos"]);
                    uint32_t fontSize = obj.value("fontSize", 32);
                    std::string colorStr = obj.value("Color", "#FFFFFF");
                    glm::vec3 textColor = ParseColor(colorStr);
                    float maxWidth = obj.value("maxWidth", 1000.0f);
                    float lineHeight = obj.value("lineHeight", 60.0f);

                    // Parse decorators (orbs) from JSON
                    m_decorators.clear();
                    if (obj.contains("orbs") && obj["orbs"].is_array()) {
                        for (const auto& orbObj : obj["orbs"]) {
                            FlowDecorator dec;
                            dec.cx = orbObj.value("cx", 0.0f);
                            dec.cy = orbObj.value("cy", 0.0f);
                            dec.radius = orbObj.value("radius", 0.0f);
                            dec.color = ParseColor(orbObj.value("Color", "#FFFFFF"));

                            // Shape configuration (optional, defaults to RoundedRect)
                            std::string shapeStr = orbObj.value("shape", "");
                            if (shapeStr == "Image") dec.shape = DecoratorShape::Image;
                            else if (shapeStr == "Sphere") dec.shape = DecoratorShape::Sphere;
                            else dec.shape = DecoratorShape::RoundedRect;  // default

                            dec.cornerRadius = orbObj.value("cornerRadius", 0.0f);
                            dec.src = orbObj.value("src", "");
                            dec.metallic = orbObj.value("metallic", 0.0f);
                            dec.roughness = orbObj.value("roughness", 0.5f);

                            // Animation (optional, defaults to Float)
                            std::string animStr = orbObj.value("animation", "");
                            if (animStr == "None") dec.animation = DecoratorAnimation::None;
                            else if (animStr == "Pulse") dec.animation = DecoratorAnimation::Pulse;
                            else if (animStr == "Spin") dec.animation = DecoratorAnimation::Spin;
                            else if (animStr == "Orbit") dec.animation = DecoratorAnimation::Orbit;
                            else dec.animation = DecoratorAnimation::Float;  // default

                            dec.animSpeed = orbObj.value("animSpeed", 1.0f);
                            dec.animAmplitude = orbObj.value("animAmplitude", 10.0f);

                            // Initialize runtime state to base position
                            dec.currentCx = dec.cx;
                            dec.currentCy = dec.cy;
                            dec.currentScale = 1.0f;
                            dec.currentRotation = 0.0f;

                            m_decorators.push_back(dec);
                        }
                    }

                    // --- Animation dispatch ---
                    for (auto& dec : m_decorators) {
                        float speed = dec.animSpeed;
                        float amp = dec.animAmplitude;

                        switch (dec.animation) {
                        case DecoratorAnimation::None:
                            dec.currentCx = dec.cx;
                            dec.currentCy = dec.cy;
                            dec.currentScale = 1.0f;
                            dec.currentRotation = 0.0f;
                            break;

                        case DecoratorAnimation::Float:
                            // Sin/cos floating (preserves original behavior)
                            dec.currentCx = dec.cx + std::sin(m_animTime * speed + dec.cy * 0.01f) * amp;
                            dec.currentCy = dec.cy + std::cos(m_animTime * speed * 0.7f + dec.cx * 0.01f) * amp * 0.625f;
                            dec.currentScale = 1.0f;
                            dec.currentRotation = 0.0f;
                            break;

                        case DecoratorAnimation::Pulse:
                            // Scale pulse ±20%
                            dec.currentCx = dec.cx;
                            dec.currentCy = dec.cy;
                            dec.currentScale = 1.0f + 0.2f * std::sin(m_animTime * speed * 2.0f);
                            dec.currentRotation = 0.0f;
                            break;

                        case DecoratorAnimation::Spin:
                            // Y-axis rotation, no position change
                            dec.currentCx = dec.cx;
                            dec.currentCy = dec.cy;
                            dec.currentScale = 1.0f;
                            dec.currentRotation = m_animTime * speed;
                            break;

                        case DecoratorAnimation::Orbit:
                            // Circular orbit around (cx, cy)
                            {
                                float angle = m_animTime * speed;
                                dec.currentCx = dec.cx + std::cos(angle) * amp;
                                dec.currentCy = dec.cy + std::sin(angle) * amp;
                            }
                            dec.currentScale = 1.0f;
                            dec.currentRotation = 0.0f;
                            break;
                        }
                    }

                    // --- Rendering dispatch ---
                    for (const auto& dec : m_decorators) {
                        switch (dec.shape) {
                        case DecoratorShape::RoundedRect:
                            {
                                float sz = dec.radius * 2.0f * dec.currentScale;
                                glm::vec2 decPos(dec.currentCx - dec.radius * dec.currentScale,
                                                 dec.currentCy - dec.radius * dec.currentScale);
                                glm::vec2 decSize(sz, sz);
                                m_context->drawRoundedRect(decPos, decSize, dec.cornerRadius, dec.color);
                            }
                            break;

                        case DecoratorShape::Image:
                            {
                                float sz = dec.radius * 2.0f * dec.currentScale;
                                glm::vec2 decPos(dec.currentCx - dec.radius * dec.currentScale,
                                                 dec.currentCy - dec.radius * dec.currentScale);
                                glm::vec2 decSize(sz, sz);
                                m_context->drawImage(dec.src, decPos, decSize);
                            }
                            break;

                        case DecoratorShape::Sphere:
                            {
                                // Sphere decorator rendered as a circle
                                m_context->drawCircle(
                                    glm::vec2(dec.currentCx, dec.currentCy),
                                    dec.radius * dec.currentScale,
                                    dec.color, 1.0f);
                            }
                            break;
                        }
                    }

                    // --- Flow text layout with decorator avoidance ---
                    struct Segment { float start, end; };

                    auto subtractInterval = [](std::vector<Segment>& segs, float a, float b) {
                        std::vector<Segment> result;
                        for (const auto& s : segs) {
                            if (b <= s.start || a >= s.end) {
                                result.push_back(s);
                            } else {
                                if (a > s.start) result.push_back({s.start, a});
                                if (b < s.end) result.push_back({b, s.end});
                            }
                        }
                        segs = result;
                    };

                    auto utf8Advance = [](const std::string& str, size_t byteOffset, size_t charCount) -> size_t {
                        size_t pos = byteOffset;
                        for (size_t i = 0; i < charCount && pos < str.length(); i++) {
                            unsigned char c = static_cast<unsigned char>(str[pos]);
                            if (c < 0x80) pos += 1;
                            else if (c < 0xE0) pos += 2;
                            else if (c < 0xF0) pos += 3;
                            else pos += 4;
                        }
                        return pos;
                    };

                    float y = pos.y;
                    size_t charByteOffset = 0;
                    float textPadding = fontSize * 0.3f;

                    while (charByteOffset < content.length()) {
                        std::vector<Segment> segments = {{pos.x, pos.x + maxWidth}};

                        // Use animated positions (currentCx/currentCy) for avoidance,
                        // but base radius (dec.radius) for the avoidance area
                        for (const auto& dec : m_decorators) {
                            float lineCenterY = y + lineHeight * 0.5f;
                            float dy = lineCenterY - dec.currentCy;
                            float effectiveRadius = dec.radius + textPadding;
                            if (std::fabs(dy) < effectiveRadius) {
                                float dx = std::sqrt(effectiveRadius * effectiveRadius - dy * dy);
                                subtractInterval(segments, dec.currentCx - dx, dec.currentCx + dx);
                            }
                        }

                        for (const auto& seg : segments) {
                            float segWidth = seg.end - seg.start;
                            if (segWidth < fontSize * 0.5f) continue;

                            int charsInSeg = static_cast<int>(segWidth / static_cast<float>(fontSize));
                            if (charsInSeg <= 0) continue;

                            size_t newOffset = utf8Advance(content, charByteOffset, charsInSeg);
                            if (newOffset == charByteOffset) break;
                            std::string substr = content.substr(charByteOffset, newOffset - charByteOffset);

                            m_context->drawText(substr, glm::vec2(seg.start, y), fontSize, textColor);
                            charByteOffset = newOffset;
                        }

                        if (segments.empty()) {
                            int charsForFullLine = static_cast<int>(maxWidth / static_cast<float>(fontSize));
                            charByteOffset = utf8Advance(content, charByteOffset, charsForFullLine);
                        }

                        y += lineHeight;
                    }
                }
            }
        } catch (const nlohmann::json::exception& e) {
            LOGE("Failed to parse UI descriptor: %s", e.what());
        }
        OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);
        } // end else (fallback inline parser)

        // NOTE: Don't set m_hasPendingDescriptor = false here
        // We want to keep re-processing the descriptor for continuous animation
    }

    // Flush remaining commands
    m_context->endQueue();
    OH_HiTrace_StartTraceEx(HITRACE_LEVEL_INFO, "AgenUI:FlushQueue", "");
    m_context->flushQueue();
    OH_HiTrace_FinishTraceEx(HITRACE_LEVEL_INFO);

    m_context->endFrame();

    return true;
}

// Set touch-based rotation angles from ArkTS UI
void Application::SetRotation(float rotationX, float rotationY)
{
    m_touchRotationX = rotationX;
    m_touchRotationY = rotationY;
}

void Application::SetLandscapeMode(bool landscape)
{
    LOGI("SetLandscapeMode: %s, current size: %d x %d",
         landscape ? "true" : "false", m_width, m_height);
    m_isLandscape = landscape;
}

void Application::SetSurfaceTransitioning(bool transitioning)
{
    LOGI("SetSurfaceTransitioning: %s", transitioning ? "true" : "false");
    m_surfaceTransitioning = transitioning;
}

void Application::SetSurfaceDestroyed(bool destroyed)
{
    LOGI("SetSurfaceDestroyed: %s", destroyed ? "true" : "false");
    m_surfaceDestroyed = destroyed;
}

std::string Application::GetGameState() const
{
    nlohmann::json state;
    if (m_gameMode && m_gameModule) {
        state["score"] = m_gameModule->GetScore();
        state["combo"] = m_gameModule->GetCombo();
        state["timeLeft"] = static_cast<int>(m_gameModule->GetTimeLeft());
        state["isOver"] = m_gameModule->IsGameOver();
    }
    return state.dump();
}

void Application::SetAutoFixEnabled(bool enabled)
{
    LOGI("SetAutoFixEnabled: %s", enabled ? "true" : "false");
    m_autoFixEnabled = enabled;
}

void Application::SetRenderMode(bool gameMode)
{
    LOGI("SetRenderMode: %s", gameMode ? "game" : "ui");
    m_explicitGameMode = gameMode;
    if (!gameMode && m_gameMode) {
        m_gameMode = false;
        m_gameModule.reset();
        m_gameStandardCommands.clear();
    }
}

void Application::HandleTouchEvent(float touchX, float touchY)
{
    if (!IsInited()) return;
    if (m_surfaceDestroyed || m_surfaceTransitioning) return;

    // Game mode touch handling
    if (m_gameMode && m_gameModule) {
        float designW = m_designWidth > 0 ? static_cast<float>(m_designWidth) : static_cast<float>(m_width);
        float designH = m_designHeight > 0 ? static_cast<float>(m_designHeight) : static_cast<float>(m_height);
        float screenW = m_surfaceWidth > 0 ? static_cast<float>(m_surfaceWidth) : static_cast<float>(m_width);
        float screenH = m_surfaceHeight > 0 ? static_cast<float>(m_surfaceHeight) : static_cast<float>(m_height);
        if (screenW <= 0 || screenH <= 0) return;

        float scaleX = designW / screenW;
        float scaleY = designH / screenH;
        float designX = touchX * scaleX;
        float designY = touchY * scaleY;
        m_gameModule->HandleTouch(designX, designY);
        return;
    }

    // Normal touch handling
    if (m_renderedCommands.empty() || m_actionInProgress) return;

    // Reverse-map screen pixel coordinates to design coordinates.
    // Touch events use screen pixels; commands use design pixel coordinates
    // that were mapped via setCoordinateMapping(designWidth, designHeight).
    float designW = m_designWidth > 0 ? static_cast<float>(m_designWidth) : static_cast<float>(m_width);
    float designH = m_designHeight > 0 ? static_cast<float>(m_designHeight) : static_cast<float>(m_height);
    float screenW = static_cast<float>(m_width);
    float screenH = static_cast<float>(m_height);

    if (screenW <= 0 || screenH <= 0) return;

    float scaleX = designW / screenW;
    float scaleY = designH / screenH;
    float designX = touchX * scaleX;
    float designY = touchY * scaleY;

    LOGI("HandleTouchEvent: screen(%.0f,%.0f) -> design(%.0f,%.0f), cmds=%zu",
         touchX, touchY, designX, designY, m_renderedCommands.size());

    // Reverse-iterate (top z-order first) to find the topmost clickable rect/circle
    for (int i = static_cast<int>(m_renderedCommands.size()) - 1; i >= 0; i--) {
        const auto& cmd = m_renderedCommands[i];
        if (cmd.action.empty()) continue;

        bool hit = false;
        if (cmd.type == dsl::DslRenderCommand::Type::Rect) {
            // Hit test: designX/Y inside [pos, pos+size]
            hit = (designX >= cmd.pos.x && designX <= cmd.pos.x + cmd.size.x &&
                   designY >= cmd.pos.y && designY <= cmd.pos.y + cmd.size.y);
        } else if (cmd.type == dsl::DslRenderCommand::Type::Circle) {
            // Hit test: distance from center <= radius
            float dx = designX - cmd.pos.x;
            float dy = designY - cmd.pos.y;
            hit = (dx * dx + dy * dy <= cmd.circleRadius * cmd.circleRadius);
        }

        if (hit) {
            LOGI("HandleTouchEvent: HIT component '%s' action='%s'",
                 cmd.componentId.c_str(), cmd.action.c_str());

            // Set pressed state for click animation overlay
            m_pressedComponentId = cmd.componentId;

            // Trigger re-render to show the click animation
            RequestRender();

            // Dispatch action callback to ArkTS
            if (m_actionCallback) {
                m_actionCallback(cmd.action, m_pendingUIDescriptor);
            }
            return;
        }
    }
}

void Application::SetActionCallback(ActionCallback callback)
{
    m_actionCallback = std::move(callback);
}

void Application::HandleTouchMove(float touchX, float touchY)
{
    if (!IsInited() || !m_gameMode || !m_gameModule) return;

    float designW = m_designWidth > 0 ? static_cast<float>(m_designWidth) : static_cast<float>(m_width);
    float designH = m_designHeight > 0 ? static_cast<float>(m_designHeight) : static_cast<float>(m_height);
    float screenW = m_surfaceWidth > 0 ? static_cast<float>(m_surfaceWidth) : static_cast<float>(m_width);
    float screenH = m_surfaceHeight > 0 ? static_cast<float>(m_surfaceHeight) : static_cast<float>(m_height);
    if (screenW <= 0 || screenH <= 0) return;

    float scaleX = designW / screenW;
    float scaleY = designH / screenH;
    m_gameModule->UpdateCrosshair(touchX * scaleX, touchY * scaleY);
}

void Application::ClearPressedState()
{
    if (!m_pressedComponentId.empty()) {
        m_pressedComponentId.clear();
        RequestRender();
    }
}

void Application::SetActionInProgress(bool inProgress)
{
    m_actionInProgress = inProgress;
    LOGI("SetActionInProgress: %s", inProgress ? "true" : "false");
}

void Application::SetDesignDimensions(int width, int height)
{
    LOGI("SetDesignDimensions: %d x %d (design was %u x %u, surface was %d x %d)",
         width, height, m_designWidth, m_designHeight, m_width, m_height);

    // Update BOTH design dimensions and surface dimensions.
    // designWidth/designHeight define the coordinate space that pixelToNDC uses
    // for mapping DSL pixel coordinates to NDC. When switching to landscape,
    // the JS side passes landscape dimensions (max x min), so we must update
    // these to match — otherwise the coordinate mapping stays at the portrait
    // default (1276x2400) and all landscape content is mispositioned.
    m_designWidth = static_cast<uint32_t>(width);
    m_designHeight = static_cast<uint32_t>(height);
    m_width = width;
    m_height = height;
    m_shouldRecreate = true;
}

void Application::SetDensity(float density)
{
    LOGI("SetDensity: %.2f (was %.2f)", density, m_density);
    m_density = density;
}

void Application::loadFontsFromConfig()
{
    if (!m_inited) return;

    AgenUIEngine::FontManager* fm = m_context->getFontManager();
    if (!fm) return;

    // Sandbox paths where Index.ets copies rawfile resources
    static const std::vector<std::string> searchPaths = {
        "/data/storage/el2/base/haps/entry/files/rawfile/",
        "/data/storage/el2/base/files/rawfile/"
    };

    // Helper: find a file in sandbox search paths, return full path or empty
    auto findSandboxFile = [&](const std::string& relativePath) -> std::string {
        for (const auto& dir : searchPaths) {
            std::string fullPath = dir + relativePath;
            FILE* f = fopen(fullPath.c_str(), "rb");
            if (f) {
                fclose(f);
                return fullPath;
            }
        }
        return "";
    };

    // Helper: read entire file into buffer
    auto readFile = [](const std::string& path) -> std::vector<uint8_t> {
        std::vector<uint8_t> data;
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return data;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            data.resize(sz);
            fread(data.data(), 1, sz, f);
        }
        fclose(f);
        return data;
    };

    // Read font_config.json from sandbox (try fonts/ subdirectory first, then root)
    std::string cfgPath = findSandboxFile("fonts/font_config.json");
    if (cfgPath.empty()) {
        cfgPath = findSandboxFile("font_config.json");
    }
    if (cfgPath.empty()) {
        LOGW("FontConfig: font_config.json not found in sandbox");
        // Fallback: try to register the default font directly
        std::string defaultPath = findSandboxFile("fonts/HarmonyOS_Sans_SC.ttf");
        if (defaultPath.empty()) {
            defaultPath = findSandboxFile("HarmonyOS_Sans_SC.ttf");
        }
        if (!defaultPath.empty()) {
            fm->registerFont("default", defaultPath);
        }
        return;
    }

    auto cfgBuf = readFile(cfgPath);
    if (cfgBuf.empty()) return;

    FontConfig fontConfig;
    if (!fontConfig.parseFromJson(std::string(cfgBuf.begin(), cfgBuf.end())) || !fontConfig.isValid()) return;

    // Clear old glyph caches before re-registering fonts
    fm->clearAllCaches();

    // Register fonts in the specified load order
    int registeredCount = 0;
    for (const auto& fontId : fontConfig.getLoadOrder()) {
        const FontEntryConfig* entry = fontConfig.findById(fontId);
        if (!entry) continue;

        // Try config path first, then strip "fonts/" prefix as fallback
        std::string sandboxPath = findSandboxFile(entry->path);
        if (sandboxPath.empty()) {
            // Try just the filename without "fonts/" prefix
            std::string filename = entry->path;
            size_t slashPos = filename.find_last_of('/');
            if (slashPos != std::string::npos) {
                filename = filename.substr(slashPos + 1);
            }
            sandboxPath = findSandboxFile(filename);
        }

        if (sandboxPath.empty()) continue;

        auto fontData = readFile(sandboxPath);
        if (fontData.empty()) continue;

        int faceIdx = (entry->faceIndex < 0) ? -1 : entry->faceIndex;
        if (fm->registerFontFromMemory(entry->id, fontData.data(), fontData.size(), faceIdx)) {
            registeredCount++;
        }
    }

    // Set alias mappings
    fm->setAliases(fontConfig.getAliases());
    LOGI("FontConfig: %d fonts registered, %d aliases set",
         registeredCount, (int)fontConfig.getAliases().size());
}

bool Application::HasActiveAnimations() const {
    for (const auto& anim : m_streamTextAnims) {
        if (!anim.finished) return true;
    }
    return false;
}

} // namespace application
