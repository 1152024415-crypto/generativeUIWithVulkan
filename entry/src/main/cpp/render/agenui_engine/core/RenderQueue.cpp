/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Platform-Independent Render Queue Implementation
 */

#include "core/RenderQueue.h"

namespace AgenUIEngine::Core {

// -----------------------------------------------------------------------------
// Queue Management
// -----------------------------------------------------------------------------

void RenderQueue::beginQueue() {
    m_commands.clear();
    m_collecting = true;
    m_insertionCounter = 0;
}

void RenderQueue::endQueue() {
    m_collecting = false;
    sortCommands();
}

void RenderQueue::clear() {
    m_commands.clear();
}

// -----------------------------------------------------------------------------
// Drawing Commands
// -----------------------------------------------------------------------------

void RenderQueue::drawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec3& color) {
    RenderCommand cmd;
    cmd.type = DrawType::Rectangle;
    cmd.position = pos;
    cmd.size = size;
    cmd.color = color;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                                   float cornerRadius, const glm::vec3& color, float alpha) {
    RenderCommand cmd;
    cmd.type = DrawType::RoundedRectangle;
    cmd.position = pos;
    cmd.size = size;
    cmd.color = color;
    cmd.alpha = alpha;
    cmd.cornerRadius = cornerRadius;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawGlassRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                                        float cornerRadius, const glm::vec3& color) {
    RenderCommand cmd;
    cmd.type = DrawType::GlassRoundedRectangle;
    cmd.position = pos;
    cmd.size = size;
    cmd.color = color;
    cmd.cornerRadius = cornerRadius;
    cmd.transparent = true;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawPolygon(const glm::vec2& center, const std::vector<glm::vec2>& vertices,
                               const glm::vec3& color, float alpha) {
    if (vertices.size() < 3) return;
    RenderCommand cmd;
    cmd.type = DrawType::Polygon;
    cmd.polygonCenter = center;
    cmd.polygonVertices = vertices;
    cmd.color = color;
    cmd.alpha = alpha;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawCircle(const glm::vec2& center, float radius,
                              const glm::vec3& color, float alpha) {
    RenderCommand cmd;
    cmd.type = DrawType::Circle;
    cmd.position = center;
    cmd.circleRadius = radius;
    cmd.color = color;
    cmd.alpha = alpha;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawText(const std::string& text, const glm::vec2& pos,
                            uint32_t fontSize, const glm::vec3& color,
                            float glowWidth, float glowIntensity,
                            const std::string& fontFamily,
                            bool hasGradient,
                            const glm::vec3& gradientEndColor,
                            float strokeWidth,
                            const glm::vec3& strokeColor,
                            int gradientDirection) {
    RenderCommand cmd;
    cmd.type = DrawType::Text;
    cmd.text = text;
    cmd.position = pos;
    cmd.fontSize = fontSize;
    cmd.color = color;
    cmd.glowWidth = glowWidth;
    cmd.glowIntensity = glowIntensity;
    cmd.fontFamily = fontFamily;
    cmd.hasGradient = hasGradient;
    cmd.gradientEndColor = gradientEndColor;
    cmd.gradientDirection = gradientDirection;
    cmd.strokeWidth = strokeWidth;
    cmd.strokeColor = strokeColor;
    cmd.transparent = true;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawMultiLineText(const std::string& text, const glm::vec2& pos,
                                     uint32_t fontSize, const glm::vec3& color,
                                     float maxWidth, float lineHeight) {
    RenderCommand cmd;
    cmd.type = DrawType::MultiLineText;
    cmd.text = text;
    cmd.position = pos;
    cmd.fontSize = fontSize;
    cmd.color = color;
    cmd.maxWidth = maxWidth;
    cmd.lineHeight = lineHeight;
    cmd.transparent = true;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawTextPartial(const std::string& key, const glm::vec3& color,
                                    uint32_t visibleChars) {
    RenderCommand cmd;
    cmd.type = DrawType::TextPartial;
    cmd.textPartialKey = key;
    cmd.color = color;
    cmd.visibleChars = visibleChars;
    cmd.transparent = true;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

void RenderQueue::drawImage(const std::string& imagePath, const glm::vec2& pos,
                             const glm::vec2& size, float rotation,
                             const std::vector<glm::vec2>& clipVertices,
                             const glm::vec2& clipCenter) {
    RenderCommand cmd;
    cmd.type = DrawType::Image;
    cmd.imagePath = imagePath;
    cmd.position = pos;
    cmd.size = size;
    cmd.rotation = rotation;
    cmd.clipVertices = clipVertices;
    cmd.clipCenter = clipCenter;
    cmd.insertionOrder = m_insertionCounter++;
    m_commands.push_back(cmd);
}

// -----------------------------------------------------------------------------
// Internal
// -----------------------------------------------------------------------------

void RenderQueue::sortCommands() {
    // Compute sort keys based on current state (depth may have been modified after insertion).
    // This ensures the sort reflects the latest depth values.
    for (size_t i = 0; i < m_commands.size(); ++i) {
        auto& cmd = m_commands[i];
        if (cmd.transparent) {
            cmd.sortKey = SortKey::makeTransparent(cmd.depth, cmd.insertionOrder);
        } else {
            cmd.sortKey = SortKey::makeOpaque(cmd.type, cmd.depth, cmd.insertionOrder);
        }
    }
    // Sort by SortKey: opaque grouped by DrawType then front-to-back,
    // transparent grouped after opaque then back-to-front.
    std::sort(m_commands.begin(), m_commands.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            return a.sortKey < b.sortKey;
        });
}

} // namespace AgenUIEngine::Core
