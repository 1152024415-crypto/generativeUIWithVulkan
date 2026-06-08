/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Render Context Implementation
 */

#include "core/RenderContext.h"

// Global debug logging switch (defined here, declared in logger_common.h)
bool g_agenuiDebugEnabled = false;

// Performance profiling switch (defined here, declared in logger_common.h)
bool g_perfEnabled = false;

namespace AgenUIEngine::Core {

// -----------------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------------

RenderContext::RenderContext(std::unique_ptr<IRenderer> renderer)
    : m_renderer(std::move(renderer)) {
}

RenderContext::~RenderContext() {
    if (m_renderer) {
        m_renderer->cleanup();
    }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

bool RenderContext::initialize(const RendererInitParams& params) {
    if (!m_renderer) return false;
    return m_renderer->initialize(params);
}

void RenderContext::beginFrame() {
    if (m_renderer) m_renderer->beginFrame();
}

void RenderContext::endFrame() {
    if (m_renderer) m_renderer->endFrame();
}

// -----------------------------------------------------------------------------
// Queue-based rendering
// -----------------------------------------------------------------------------

void RenderContext::beginQueue() {
    m_queue.beginQueue();
    m_stats.clear();
}

void RenderContext::endQueue() {
    m_queue.endQueue();
}

void RenderContext::flushQueue() {
    if (!m_renderer) return;
    m_stats.clear();
    m_renderer->executeCommandBatch(m_queue.getCommands(), m_stats);
    m_queue.clear();
}

void RenderContext::prepareGlassPass() {
    if (m_renderer) {
        m_renderer->prepareGlassPass();
    }
}

// -----------------------------------------------------------------------------
// Drawing commands (forward to queue)
// -----------------------------------------------------------------------------

void RenderContext::drawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec3& color) {
    m_queue.drawRect(pos, size, color);
}

void RenderContext::drawRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                                     float cornerRadius, const glm::vec3& color, float alpha) {
    m_queue.drawRoundedRect(pos, size, cornerRadius, color, alpha);
}

void RenderContext::drawGlassRoundedRect(const glm::vec2& pos, const glm::vec2& size,
                                          float cornerRadius, const glm::vec3& color) {
    m_queue.drawGlassRoundedRect(pos, size, cornerRadius, color);
}

void RenderContext::drawPolygon(const glm::vec2& center, const std::vector<glm::vec2>& vertices,
                                const glm::vec3& color, float alpha) {
    m_queue.drawPolygon(center, vertices, color, alpha);
}

void RenderContext::drawCircle(const glm::vec2& center, float radius,
                                const glm::vec3& color, float alpha) {
    m_queue.drawCircle(center, radius, color, alpha);
}

void RenderContext::drawText(const std::string& text, const glm::vec2& pos,
                              uint32_t fontSize, const glm::vec3& color,
                              const std::string& fontFamily) {
    m_queue.drawText(text, pos, fontSize, color, m_pendingGlowWidth, m_pendingGlowIntensity, fontFamily,
                      m_pendingHasGradient, m_pendingGradientEndColor,
                      m_pendingStrokeWidth, m_pendingStrokeColor,
                      m_pendingGradientDirection);
}

void RenderContext::drawMultiLineText(const std::string& text, const glm::vec2& pos,
                                       uint32_t fontSize, const glm::vec3& color,
                                       float maxWidth, float lineHeight) {
    m_queue.drawMultiLineText(text, pos, fontSize, color, maxWidth, lineHeight);
}

void RenderContext::drawTextPartial(const std::string& key, const glm::vec3& color,
                                      uint32_t visibleChars) {
    m_queue.drawTextPartial(key, color, visibleChars);
}

void RenderContext::drawImage(const std::string& imagePath, const glm::vec2& pos, const glm::vec2& size,
                               float rotation, const std::vector<glm::vec2>& clipVertices,
                               const glm::vec2& clipCenter) {
    m_queue.drawImage(imagePath, pos, size, rotation, clipVertices, clipCenter);
}

// -----------------------------------------------------------------------------
// State settings
// -----------------------------------------------------------------------------

void RenderContext::setTextGlow(float glowWidth, float glowIntensity) {
    m_pendingGlowWidth = glowWidth;
    m_pendingGlowIntensity = glowIntensity;
}

void RenderContext::setTextGradient(bool hasGradient, const glm::vec3& endColor, int direction) {
    m_pendingHasGradient = hasGradient;
    m_pendingGradientEndColor = endColor;
    m_pendingGradientDirection = direction;
}

void RenderContext::setTextStroke(float width, const glm::vec3& color) {
    m_pendingStrokeWidth = width;
    m_pendingStrokeColor = color;
}

// -----------------------------------------------------------------------------
// Setup (forwarded to renderer)
// -----------------------------------------------------------------------------

void RenderContext::setClearColor(float r, float g, float b, float a) {
    if (m_renderer) m_renderer->setClearColor(r, g, b, a);
}

void RenderContext::setResourceManager(void* rm) {
    if (m_renderer) m_renderer->setResourceManager(rm);
}

bool RenderContext::initializeFonts(void* rm, const std::string& fontName) {
    if (!m_renderer) return false;
    return m_renderer->initializeFonts(rm, fontName);
}

bool RenderContext::createPipelines() {
    if (!m_renderer) return false;
    return m_renderer->createPipelines();
}

void RenderContext::recreateSwapChain() {
    if (m_renderer) m_renderer->recreateSwapChain();
}

void RenderContext::updateSurfaceSize(int width, int height) {
    if (m_renderer) m_renderer->updateSurfaceSize(width, height);
}

void RenderContext::setCoordinateMapping(int width, int height) {
    if (m_renderer) m_renderer->setCoordinateMapping(width, height);
}

// -----------------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------------

int RenderContext::getWidth() const {
    return m_renderer ? m_renderer->getWidth() : 0;
}

int RenderContext::getHeight() const {
    return m_renderer ? m_renderer->getHeight() : 0;
}

const FrameStats& RenderContext::getRenderStats() const {
    return m_stats;
}

AgenUIEngine::FontManager* RenderContext::getFontManager() {
    return m_renderer ? m_renderer->getFontManager() : nullptr;
}

void RenderContext::precacheText(const std::string& text, uint32_t fontSize) {
    if (m_renderer) m_renderer->precacheText(text, fontSize);
}

// -----------------------------------------------------------------------------
// Stream text support (typewriter effect)
// -----------------------------------------------------------------------------

uint32_t RenderContext::prepareTextLayout(const std::string& key, const std::string& text,
                                            const glm::vec2& position, uint32_t fontSize,
                                            const std::string& fontFamily,
                                            float maxWidth) {
    if (!m_renderer) return 0;
    return m_renderer->prepareTextLayout(key, text, position, fontSize, fontFamily, maxWidth);
}

uint32_t RenderContext::prepareTextLayout(const std::string& key,
                                            const std::vector<TextBlock>& blocks,
                                            const glm::vec2& position, uint32_t baseFontSize,
                                            float maxWidth) {
    if (!m_renderer) return 0;
    return m_renderer->prepareTextLayout(key, blocks, position, baseFontSize, maxWidth);
}

void RenderContext::clearTextLayoutCache() {
    if (m_renderer) m_renderer->clearTextLayoutCache();
}

} // namespace AgenUIEngine::Core
