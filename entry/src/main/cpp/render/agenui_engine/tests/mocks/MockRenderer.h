/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Mock IRenderer for unit testing RenderContext.
 */

#ifndef AGENUI_TESTS_MOCK_RENDERER_H
#define AGENUI_TESTS_MOCK_RENDERER_H

#include "core/IGraphicsAPI.h"
#include <gmock/gmock.h>

using namespace AgenUIEngine::Core;

class MockRenderer : public IRenderer {
public:
    // Lifecycle
    MOCK_METHOD(bool, initialize, (const RendererInitParams&), (override));
    MOCK_METHOD(void, beginFrame, (), (override));
    MOCK_METHOD(void, endFrame, (), (override));
    MOCK_METHOD(void, cleanup, (), (override));
    MOCK_METHOD(bool, isInitialized, (), (const, override));

    // High-Level Draw Primitives
    MOCK_METHOD(void, drawRect, (const glm::vec2&, const glm::vec2&, const glm::vec3&), (override));
    MOCK_METHOD(void, drawRoundedRect, (const glm::vec2&, const glm::vec2&, float, const glm::vec3&, float), (override));
    MOCK_METHOD(void, drawGlassRoundedRect, (const glm::vec2&, const glm::vec2&, float, const glm::vec3&), (override));
    MOCK_METHOD(void, drawPolygon, (const glm::vec2&, const std::vector<glm::vec2>&, const glm::vec3&, float), (override));
    MOCK_METHOD(void, drawCircle, (const glm::vec2&, float, const glm::vec3&, float), (override));
    MOCK_METHOD(void, drawText, (const std::string&, const glm::vec2&, uint32_t, const glm::vec3&, const std::string&), (override));
    MOCK_METHOD(void, drawMultiLineText, (const std::string&, const glm::vec2&, uint32_t, const glm::vec3&, float, float), (override));
    MOCK_METHOD(void, setTextGlow, (float, float), (override));
    MOCK_METHOD(void, setTextGradient, (bool, const glm::vec3&, int), (override));
    MOCK_METHOD(void, setTextStroke, (float, const glm::vec3&), (override));
    MOCK_METHOD(void, drawImage, (const std::string&, const glm::vec2&, const glm::vec2&,
                float, const std::vector<glm::vec2>&, const glm::vec2&), (override));
    // Setup
    MOCK_METHOD(void, setClearColor, (float, float, float, float), (override));
    MOCK_METHOD(void, setResourceManager, (void*), (override));
    MOCK_METHOD(bool, initializeFonts, (void*, const std::string&), (override));
    MOCK_METHOD(bool, createPipelines, (), (override));

    // Queries
    MOCK_METHOD(int, getWidth, (), (const, override));
    MOCK_METHOD(int, getHeight, (), (const, override));
    MOCK_METHOD(GraphicsAPI, getAPI, (), (const, override));
    MOCK_METHOD(void, waitIdle, (), (override));
    MOCK_METHOD(void, recreateSwapChain, (), (override));
    MOCK_METHOD(void, updateSurfaceSize, (int, int), (override));
    MOCK_METHOD(void, setCoordinateMapping, (int, int), (override));
    MOCK_METHOD(void, prepareGlassPass, (), (override));

    // FontManager access (returns nullptr by default)
    MOCK_METHOD(AgenUIEngine::FontManager*, getFontManager, (), (override));
};

#endif // AGENUI_TESTS_MOCK_RENDERER_H
