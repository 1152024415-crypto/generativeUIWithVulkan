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

#ifndef DSL_RENDER_COMMAND_H
#define DSL_RENDER_COMMAND_H

#include <string>
#include <vector>
#include "../agenui_engine/thirdparty/glm/glm.hpp"

namespace application::dsl {

/**
 * Unified intermediate representation (IR) for all DSL formats.
 * Each parser converts its native format into this common structure,
 * which Application.cpp then renders via m_core->queueXxx().
 *
 * New DSL formats should map their components to these types.
 * If a new component type is needed (e.g., Video), add it here
 * and handle it in Application.cpp's render switch.
 */
struct DslRenderCommand {
    enum class Type { Rect, Polygon, Circle, Text, Image };

    Type type = Type::Rect;

    // Common fields
    glm::vec2 pos{0.0f, 0.0f};     // NDC coordinates
    glm::vec2 size{0.0f, 0.0f};    // NDC dimensions
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};

    // Rect specific
    float radius = 0.0f;

    // Glass effect
    bool glass = false;

    // Polygon specific
    std::vector<glm::vec2> polygonVertices;  // perimeter vertices in pixel coords
    glm::vec2 polygonCenter{0.0f, 0.0f};    // fan center in pixel coords

    // Circle specific
    float circleRadius = 0.0f;

    // Text specific
    std::string text;
    uint32_t fontSize = 32;
    std::string fontFamily;      ///< logical font ID (e.g. "default", "bold", "mono")
    float glowWidth = 0.0f;      ///< Glow radius in pixels (0 = no glow)
    float glowIntensity = 0.0f;  ///< Glow brightness (0-1)
    bool hasGradient = false;     ///< Whether gradient fill is enabled
    glm::vec3 gradientEndColor{0.0f};  ///< Gradient end color (RGB)
    int gradientDirection = 0;   ///< 0=vertical, 1=horizontal
    float strokeWidth = 0.0f;    ///< Stroke width in pixels (0 = no stroke)
    glm::vec3 strokeColor{0.0f}; ///< Stroke color (RGB)

    // Stream text (typewriter effect)
    bool streamText = false;            ///< Enable typewriter animation
    float streamTextSpeed = 0.0f;       ///< Milliseconds per character (0 = instant, paced by caller)
    float streamTextMaxWidth = 0.0f;    ///< Auto-wrap line width in pixels (0 = no wrap)
    bool markdown = false;              ///< Content is Markdown (multi-font/style rendering)

    // Image specific
    std::string imagePath;
    float rotation = 0.0f;                                  // degrees, around center
    std::vector<glm::vec2> clipVertices;                    // polygon clip vertices (pixel coords)
    glm::vec2 clipCenter{0.0f, 0.0f};                      // polygon clip center (pixel coords)

    // Interaction
    std::string componentId;  // Original component id from V2 DSL JSON
    std::string action;       // "resort" for internal shuffle, or instruction for LLM callback
};

} // namespace application::dsl

#endif // DSL_RENDER_COMMAND_H
