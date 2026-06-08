/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Platform-Independent Render Queue
 * =================================
 * Provides a deferred rendering system that works across all graphics backends.
 * Collects semantic render commands, sorts them for optimal rendering, and
 * provides them to RenderContext for execution via the backend IRenderer.
 */

#ifndef AGENUI_RENDER_QUEUE_H
#define AGENUI_RENDER_QUEUE_H

#include "RenderCommand.h"
#include <vector>
#include <algorithm>

namespace AgenUIEngine::Core {

/**
 * @brief Platform-independent render queue
 *
 * Collects render commands (semantic data only) and sorts them for optimal
 * rendering order. The RenderContext executes these commands via IRenderer.
 */
class RenderQueue {
public:
    RenderQueue() = default;
    ~RenderQueue() = default;

    // -------------------------------------------------------------------------
    // Queue Management
    // -------------------------------------------------------------------------

    void beginQueue();
    void endQueue();
    void clear();

    // -------------------------------------------------------------------------
    // Drawing Commands (collect semantic commands)
    // -------------------------------------------------------------------------

    void drawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec3& color);
    void drawRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                         float cornerRadius, const glm::vec3& color, float alpha = 1.0f);
    void drawGlassRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                              float cornerRadius, const glm::vec3& color);
    void drawPolygon(const glm::vec2& center, const std::vector<glm::vec2>& vertices,
                     const glm::vec3& color, float alpha = 1.0f);
    void drawCircle(const glm::vec2& center, float radius,
                    const glm::vec3& color, float alpha = 1.0f);
    void drawText(const std::string& text, const glm::vec2& pos,
                  uint32_t fontSize, const glm::vec3& color,
                  float glowWidth = 0.0f, float glowIntensity = 0.0f,
                  const std::string& fontFamily = "default",
                  bool hasGradient = false,
                  const glm::vec3& gradientEndColor = glm::vec3(0.0f),
                  float strokeWidth = 0.0f,
                  const glm::vec3& strokeColor = glm::vec3(0.0f),
                  int gradientDirection = 0);
    void drawMultiLineText(const std::string& text, const glm::vec2& pos,
                           uint32_t fontSize, const glm::vec3& color,
                           float maxWidth, float lineHeight);
    void drawTextPartial(const std::string& key, const glm::vec3& color,
                          uint32_t visibleChars);
    void drawImage(const std::string& imagePath, const glm::vec2& pos,
                   const glm::vec2& size, float rotation = 0.0f,
                   const std::vector<glm::vec2>& clipVertices = {},
                   const glm::vec2& clipCenter = {});

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    size_t getCommandCount() const { return m_commands.size(); }
    const std::vector<RenderCommand>& getCommands() const { return m_commands; }
    bool isEmpty() const { return m_commands.empty(); }

private:
    void sortCommands();

    std::vector<RenderCommand> m_commands;
    bool m_collecting = false;
    uint64_t m_insertionCounter = 0;
};

} // namespace AgenUIEngine::Core

#endif // AGENUI_RENDER_QUEUE_H
