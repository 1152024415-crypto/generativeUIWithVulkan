/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Render Factory Implementation
 * ============================
 * Factory for creating RenderContext instances based on GraphicsAPI
 */

#include "core/IGraphicsAPI.h"
#include "core/RenderContext.h"
#include "backend/vulkan/VkRenderer.h"
#include <memory>
#include <vector>
#include <algorithm>

namespace AgenUIEngine {
namespace Core {

// -------------------------------------------------------------------------
// RenderFactory Implementation
// -------------------------------------------------------------------------

std::unique_ptr<RenderContext> RenderFactory::createRenderContext(GraphicsAPI api) {
    switch (api) {
        case GraphicsAPI::Vulkan: {
            std::unique_ptr<IRenderer> renderer = std::make_unique<AgenUIEngine::VkRenderer>();
            return std::make_unique<RenderContext>(std::move(renderer));
        }

        case GraphicsAPI::OpenGL_ES: {
            // TODO: Create OpenGL ES renderer (future phase)
            return nullptr;
        }

        case GraphicsAPI::DirectX12: {
            // TODO: Create DirectX 12 renderer (future phase)
            return nullptr;
        }

        case GraphicsAPI::Metal: {
            // TODO: Create Metal renderer (future phase)
            return nullptr;
        }

        default:
            return nullptr;
    }
}

std::vector<GraphicsAPI> RenderFactory::getSupportedAPIs() {
    std::vector<GraphicsAPI> apis;
    apis.push_back(GraphicsAPI::Vulkan);
    return apis;
}

bool RenderFactory::isAPISupported(GraphicsAPI api) {
    auto supportedAPIs = getSupportedAPIs();
    return std::find(supportedAPIs.begin(), supportedAPIs.end(), api) != supportedAPIs.end();
}

} // namespace Core
} // namespace AgenUIEngine
