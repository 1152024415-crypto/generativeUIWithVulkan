/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Render Context (Facade)
 * ======================
 * The main interface that Application code interacts with.
 * Combines RenderQueue (command collection + sorting) with IRenderer (backend execution).
 *
 * Architecture: Application -> RenderContext -> IRenderer -> BackendVulkan
 */

#ifndef AGENUI_RENDER_CONTEXT_H
#define AGENUI_RENDER_CONTEXT_H

#include "IGraphicsAPI.h"
#include "RenderQueue.h"
#include "RenderCommand.h"
#include <memory>
#include <string>

namespace AgenUIEngine { class FontManager; }

namespace AgenUIEngine::Core {

/**
 * @brief Render context facade
 *
 * This is the single object that Application owns. It provides:
 * - Lifecycle management (initialize, beginFrame, endFrame, cleanup)
 * - Queue-based batched rendering (beginQueue, drawXxx, endQueue, flushQueue)
 * - Setup methods (pipelines, fonts, shaders) - forwarded to renderer
 * - Query methods (dimensions, stats)
 *
 * All draw commands must be issued between beginQueue/endQueue and
 * executed via flushQueue. There is no immediate rendering mode.
 */
class RenderContext {
public:
    explicit RenderContext(std::unique_ptr<IRenderer> renderer);
    ~RenderContext();

    // -------------------------------------------------------------------------
    // Lifecycle (forwarded to renderer)
    // -------------------------------------------------------------------------

    bool initialize(const RendererInitParams& params);
    void beginFrame();
    void endFrame();
    void cleanup();
    bool isInitialized() const;

    // -------------------------------------------------------------------------
    // Queue-based rendering (uses internal RenderQueue)
    // -------------------------------------------------------------------------

    void beginQueue();
    void endQueue();
    void flushQueue();
    void prepareGlassPass();    // Copy framebuffer to background texture for glass refraction

    // -------------------------------------------------------------------------
    // Drawing commands (stored in queue, executed on flushQueue)
    // Must be called between beginQueue() and endQueue().
    // -------------------------------------------------------------------------

    void drawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec3& color);
    void drawRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                         float radius, const glm::vec3& color, float alpha = 1.0f);
    void drawGlassRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                              float radius, const glm::vec3& color);
    void drawPolygon(const glm::vec2& center, const std::vector<glm::vec2>& vertices,
                     const glm::vec3& color, float alpha = 1.0f);
    void drawCircle(const glm::vec2& center, float radius,
                    const glm::vec3& color, float alpha = 1.0f);
    void drawText(const std::string& text, const glm::vec2& pos,
                  uint32_t fontSize, const glm::vec3& color,
                  const std::string& fontFamily = "default");
    void drawMultiLineText(const std::string& text, const glm::vec2& pos,
                           uint32_t fontSize, const glm::vec3& color,
                           float maxWidth, float lineHeight);
    void drawTextPartial(const std::string& key, const glm::vec3& color,
                          uint32_t visibleChars);
    void drawImage(const std::string& imagePath, const glm::vec2& pos, const glm::vec2& size,
                   float rotation = 0.0f,
                   const std::vector<glm::vec2>& clipVertices = {},
                   const glm::vec2& clipCenter = {});

    // -------------------------------------------------------------------------
    // State settings (affects subsequent draws in the queue)
    // -------------------------------------------------------------------------

    void setTextGlow(float glowWidth, float glowIntensity);
    void setTextGradient(bool hasGradient, const glm::vec3& endColor, int direction = 0);
    void setTextStroke(float width, const glm::vec3& color);

    // -------------------------------------------------------------------------
    // Setup (forwarded to renderer)
    // -------------------------------------------------------------------------

    void setClearColor(float r, float g, float b, float a);
    void setResourceManager(void* rm);
    bool initializeFonts(void* rm, const std::string& fontName);
    bool createPipelines();
    void recreateSwapChain();
    void updateSurfaceSize(int width, int height);
    void setCoordinateMapping(int width, int height);

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    int getWidth() const;
    int getHeight() const;
    const FrameStats& getRenderStats() const;
    AgenUIEngine::FontManager* getFontManager();

    /**
     * @brief Pre-cache all glyphs for a given text to avoid run-time atlas upload stalls
     */
    void precacheText(const std::string& text, uint32_t fontSize);

    // -------------------------------------------------------------------------
    // Stream text support (typewriter effect)
    // -------------------------------------------------------------------------

    /** Plain text layout (convenience wrapper) */
    uint32_t prepareTextLayout(const std::string& key, const std::string& text,
                                const glm::vec2& position, uint32_t fontSize,
                                const std::string& fontFamily = "default",
                                float maxWidth = 0.0f);

    /** Multi-styled text layout (accepts pre-parsed blocks) */
    uint32_t prepareTextLayout(const std::string& key,
                                const std::vector<TextBlock>& blocks,
                                const glm::vec2& position, uint32_t baseFontSize,
                                float maxWidth = 0.0f);

    void clearTextLayoutCache();

private:
    std::unique_ptr<IRenderer> m_renderer;
    RenderQueue m_queue;
    FrameStats m_stats;
    float m_pendingGlowWidth = 0.0f;
    float m_pendingGlowIntensity = 0.0f;
    bool m_pendingHasGradient = false;
    glm::vec3 m_pendingGradientEndColor{0.0f};
    int m_pendingGradientDirection = 0;
    float m_pendingStrokeWidth = 0.0f;
    glm::vec3 m_pendingStrokeColor{0.0f};
};

} // namespace AgenUIEngine::Core

#endif // AGENUI_RENDER_CONTEXT_H
