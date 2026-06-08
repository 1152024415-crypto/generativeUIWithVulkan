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

#include "dsl/CustomDslRender.h"
#include "dsl/DslRenderRegistry.h"
#include "logger_common.h"
#include "agenui_engine/thirdparty/glm/glm.hpp"
#include <sstream>

namespace application::dsl {

// ─── Helpers (migrated from Application.cpp) ───

static glm::vec3 ParseColor(const std::string& hexColor) {
    glm::vec3 result(1.0f, 1.0f, 1.0f);
    std::string temp = hexColor;
    if (!temp.empty() && temp[0] == '#') {
        temp = temp.substr(1);
    }
    if (temp.length() == 8) {
        temp = temp.substr(2);
    }
    if (temp.length() == 6) {
        result.r = std::stoul(temp.substr(0, 2), nullptr, 16) / 255.0f;
        result.g = std::stoul(temp.substr(2, 2), nullptr, 16) / 255.0f;
        result.b = std::stoul(temp.substr(4, 2), nullptr, 16) / 255.0f;
    }
    return result;
}

static glm::vec4 ParseColorAlpha(const std::string& hexColor) {
    std::string temp = hexColor;
    if (!temp.empty() && temp[0] == '#') {
        temp = temp.substr(1);
    }
    float alpha = 1.0f;
    if (temp.length() == 8) {
        alpha = std::stoul(temp.substr(0, 2), nullptr, 16) / 255.0f;
        temp = temp.substr(2);
    }
    glm::vec3 rgb = ParseColor("#" + temp);
    return glm::vec4(rgb, alpha);
}

static glm::vec2 ParseVec2(const nlohmann::json& j) {
    if (j.is_array() && j.size() >= 2) {
        return glm::vec2(j[0].get<float>(), j[1].get<float>());
    }
    return glm::vec2(0.0f, 0.0f);
}

// ─── CustomDslRender Implementation ───

bool CustomDslRender::CanParse(const nlohmann::json& root) const {
    // Reject formats handled by other renderers
    if (root.is_object()) {
        if (root.contains("updateComponents") || root.contains("version")) {
            return false;
        }
    }
    if (!root.is_array() || root.empty()) return false;
    return root[0].is_object() && root[0].contains("type");
}

std::vector<DslRenderCommand> CustomDslRender::Parse(
    const nlohmann::json& root,
    const ParseContext& ctx
) const {
    std::vector<DslRenderCommand> commands;

    // Support both top-level array and top-level object with "elements" array
    nlohmann::json elements;
    if (root.is_array()) {
        elements = root;
    } else if (root.is_object()) {
        elements = root.value("elements", nlohmann::json::array());
    } else {
        LOGE("CustomDslRender: Invalid JSON root type");
        return {};
    }

    // --- First pass: detect DSL canvas size from all elements ---
    float canvasMaxX = 0.0f, canvasMaxY = 0.0f;
    for (const auto& elem : elements) {
        if (!elem.is_object()) continue;
        auto posIt = elem.find("pos");
        auto sizeIt = elem.find("size");
        if (posIt != elem.end() && sizeIt != elem.end()) {
            glm::vec2 pos = ParseVec2(*posIt);
            glm::vec2 size = ParseVec2(*sizeIt);
            canvasMaxX = std::max(canvasMaxX, pos.x + size.x);
            canvasMaxY = std::max(canvasMaxY, pos.y + size.y);
        }
    }
    // Only scale if there's a full-screen background element (pos=[0,0] covering the canvas).
    // Portrait cases have no [0,0] background — they intentionally don't fill the screen,
    // so scaling would distort them.  Landscape cases have a [0,0] full-screen rect/img.
    float scaleX = 1.0f, scaleY = 1.0f;
    if (canvasMaxX > 0 && canvasMaxY > 0) {
        bool hasFullScreenBg = false;
        for (const auto& elem : elements) {
            if (!elem.is_object()) continue;
            std::string type = elem.value("type", std::string(""));
            if (type != "rect" && type != "img") continue;
            auto posIt = elem.find("pos");
            auto sizeIt = elem.find("size");
            if (posIt == elem.end() || sizeIt == elem.end()) continue;
            glm::vec2 pos = ParseVec2(*posIt);
            glm::vec2 size = ParseVec2(*sizeIt);
            if (pos.x == 0.0f && pos.y == 0.0f &&
                size.x >= canvasMaxX * 0.9f && size.y >= canvasMaxY * 0.9f) {
                hasFullScreenBg = true;
                break;
            }
        }
        if (hasFullScreenBg) {
            scaleX = ctx.designWidth / canvasMaxX;
            scaleY = ctx.designHeight / canvasMaxY;
        }
    }
    LOGI("CustomDslRender: Canvas %.0fx%.0f → design %.0fx%.0f (scaleX=%.4f, scaleY=%.4f)%s",
         canvasMaxX, canvasMaxY, ctx.designWidth, ctx.designHeight, scaleX, scaleY,
         (scaleX != 1.0f || scaleY != 1.0f) ? " [SCALED]" : " [RAW]");

    for (const auto& elem : elements) {
        if (!elem.is_object()) continue;

        std::string type = elem.value("type", std::string(""));

        if (type == "rect") {
            auto posIt = elem.find("pos");
            auto sizeIt = elem.find("size");
            if (posIt == elem.end() || sizeIt == elem.end()) continue;

            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Rect;
            glm::vec2 rawPos = ParseVec2(*posIt);
            glm::vec2 rawSize = ParseVec2(*sizeIt);
            cmd.pos = glm::vec2(rawPos.x * scaleX, rawPos.y * scaleY);
            cmd.size = glm::vec2(rawSize.x * scaleX, rawSize.y * scaleY);
            std::string colorStr = elem.value("Color", std::string("#FFFFFF"));
            cmd.color = ParseColorAlpha(colorStr);
            cmd.radius = elem.value("radius", 0.0f) * std::min(scaleX, scaleY);

            // Parse glass flag
            cmd.glass = elem.value("glass", false);

            commands.push_back(cmd);

        } else if (type == "polygon") {
            auto centerIt = elem.find("center");
            auto vertsIt = elem.find("vertices");
            if (centerIt == elem.end() || vertsIt == elem.end() || !vertsIt->is_array()) continue;

            glm::vec2 rawCenter = ParseVec2(*centerIt);
            std::vector<glm::vec2> rawVerts;
            for (const auto& v : *vertsIt) {
                if (v.is_array() && v.size() >= 2) {
                    rawVerts.push_back(glm::vec2(v[0].get<float>(), v[1].get<float>()));
                }
            }
            if (rawVerts.size() < 3) continue;

            std::string colorStr = elem.value("Color", std::string("#FFFFFF"));
            glm::vec3 color = ParseColor(colorStr);

            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Polygon;
            cmd.polygonCenter = glm::vec2(rawCenter.x * scaleX, rawCenter.y * scaleY);
            for (auto& v : rawVerts) {
                cmd.polygonVertices.push_back(glm::vec2(v.x * scaleX, v.y * scaleY));
            }
            cmd.color = glm::vec4(color, 1.0f);
            commands.push_back(cmd);

        } else if (type == "circle") {
            auto posIt = elem.find("pos");
            if (posIt == elem.end()) continue;

            glm::vec2 rawPos = ParseVec2(*posIt);
            float rawRadius = elem.value("circleRadius", 0.0f);
            std::string colorStr = elem.value("Color", std::string("#FFFFFF"));
            glm::vec4 colorAlpha = ParseColorAlpha(colorStr);

            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Circle;
            cmd.pos = glm::vec2(rawPos.x * scaleX, rawPos.y * scaleY);
            cmd.circleRadius = rawRadius * std::min(scaleX, scaleY);
            cmd.color = colorAlpha;
            commands.push_back(cmd);

        } else if (type == "text") {
            std::string content = elem.value("Content", std::string(""));
            auto posIt = elem.find("pos");
            if (posIt == elem.end()) continue;

            glm::vec2 rawPos = ParseVec2(*posIt);
            uint32_t fontSize = elem.value("fontSize", 32);
            std::string colorStr = elem.value("Color", std::string("#FFFFFF"));
            glm::vec3 color = ParseColor(colorStr);
            std::string fontFamily = elem.value("fontFamily", std::string("default"));

            // Apply scaling
            glm::vec2 pos = glm::vec2(rawPos.x * scaleX, rawPos.y * scaleY);
            float scaledFontSize = fontSize * std::min(scaleX, scaleY);

            // Read glow parameters
            float glowWidth = elem.value("glowWidth", 0.0f);
            float glowIntensity = elem.value("glowIntensity", 0.0f);

            // Read gradient parameters
            bool hasGradient = false;
            glm::vec3 gradientEndColor(0.0f);
            int gradientDirection = 0;
            auto gradIt = elem.find("gradient");
            if (gradIt != elem.end() && gradIt->is_object()) {
                std::string endColorStr = gradIt->value("endColor", std::string(""));
                if (!endColorStr.empty()) {
                    gradientEndColor = ParseColor(endColorStr);
                    hasGradient = true;
                }
                std::string dirStr = gradIt->value("direction", std::string("vertical"));
                gradientDirection = (dirStr == "horizontal") ? 1 : 0;
            }

            // Read stroke parameters
            float strokeWidth = 0.0f;
            glm::vec3 strokeColorVal(0.0f);
            auto strokeIt = elem.find("stroke");
            if (strokeIt != elem.end() && strokeIt->is_object()) {
                strokeWidth = strokeIt->value("width", 0.0f);
                std::string strokeColorStr = strokeIt->value("color", std::string(""));
                if (!strokeColorStr.empty()) {
                    strokeColorVal = ParseColor(strokeColorStr);
                }
            }

            // Read streamText parameters (typewriter effect)
            bool streamText = false;
            float streamTextSpeed = 80.0f;
            float streamTextMaxWidth = 0.0f;
            if (elem.contains("streamText")) {
                streamText = true;
                if (elem["streamText"].is_object()) {
                    streamTextSpeed = elem["streamText"].value("speed", 0.0f);
                    streamTextMaxWidth = elem["streamText"].value("maxWidth", 0.0f);
                } else if (elem["streamText"].is_number()) {
                    streamTextSpeed = elem["streamText"].get<float>();
                }
            }
            std::string componentId = elem.value("id", std::string(""));

            if (streamText) {
                // Stream text: single command with full text (newlines handled by renderer)
                DslRenderCommand cmd;
                cmd.type = DslRenderCommand::Type::Text;
                cmd.text = content;
                cmd.pos = glm::vec2(pos.x, pos.y);
                cmd.fontSize = static_cast<uint32_t>(scaledFontSize);
                cmd.color = glm::vec4(color, 1.0f);
                cmd.fontFamily = fontFamily;
                cmd.glowWidth = glowWidth;
                cmd.glowIntensity = glowIntensity;
                cmd.hasGradient = hasGradient;
                cmd.gradientEndColor = gradientEndColor;
                cmd.gradientDirection = gradientDirection;
                cmd.strokeWidth = strokeWidth;
                cmd.strokeColor = strokeColorVal;
                cmd.streamText = true;
                cmd.streamTextSpeed = streamTextSpeed;
                cmd.streamTextMaxWidth = streamTextMaxWidth;
                cmd.componentId = componentId.empty() ? ("stream_" + std::to_string(commands.size())) : componentId;
                commands.push_back(cmd);
            } else {
            // Handle multi-line text: split by '\n' and create separate commands
            float lineY = pos.y;
            float lineHeight = scaledFontSize * 1.3f;
            size_t start = 0;
            size_t newlinePos;

            while ((newlinePos = content.find('\n', start)) != std::string::npos) {
                std::string line = content.substr(start, newlinePos - start);
                if (!line.empty()) {
                    DslRenderCommand cmd;
                    cmd.type = DslRenderCommand::Type::Text;
                    cmd.text = line;
                    cmd.pos = glm::vec2(pos.x, lineY);
                    cmd.fontSize = static_cast<uint32_t>(scaledFontSize);
                    cmd.color = glm::vec4(color, 1.0f);
                    cmd.fontFamily = fontFamily;
                    cmd.glowWidth = glowWidth;
                    cmd.glowIntensity = glowIntensity;
                    cmd.hasGradient = hasGradient;
                    cmd.gradientEndColor = gradientEndColor;
                    cmd.gradientDirection = gradientDirection;
                    cmd.strokeWidth = strokeWidth;
                    cmd.strokeColor = strokeColorVal;
                    commands.push_back(cmd);
                }
                lineY += lineHeight;
                start = newlinePos + 1;
            }
            // Last line (or only line if no '\n')
            std::string lastLine = content.substr(start);
            if (!lastLine.empty()) {
                DslRenderCommand cmd;
                cmd.type = DslRenderCommand::Type::Text;
                cmd.text = lastLine;
                cmd.pos = glm::vec2(pos.x, lineY);
                cmd.fontSize = static_cast<uint32_t>(scaledFontSize);
                cmd.color = glm::vec4(color, 1.0f);
                cmd.fontFamily = fontFamily;
                cmd.glowWidth = glowWidth;
                cmd.glowIntensity = glowIntensity;
                cmd.hasGradient = hasGradient;
                cmd.gradientEndColor = gradientEndColor;
                cmd.gradientDirection = gradientDirection;
                cmd.strokeWidth = strokeWidth;
                cmd.strokeColor = strokeColorVal;
                commands.push_back(cmd);
            }
            } // end else (non-stream multi-line text)

        } else if (type == "img") {
            std::string src = elem.value("src", std::string(""));
            auto posIt = elem.find("pos");
            auto sizeIt = elem.find("size");
            if (posIt == elem.end() || sizeIt == elem.end()) continue;

            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Image;
            cmd.imagePath = src;
            glm::vec2 rawPos = ParseVec2(*posIt);
            glm::vec2 rawSize = ParseVec2(*sizeIt);
            cmd.pos = glm::vec2(rawPos.x * scaleX, rawPos.y * scaleY);
            cmd.size = glm::vec2(rawSize.x * scaleX, rawSize.y * scaleY);
            cmd.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            // Optional rotation (degrees)
            cmd.rotation = elem.value("rotation", 0.0f);

            // Optional polygon clip vertices
            auto clipVertsIt = elem.find("clipVertices");
            if (clipVertsIt != elem.end() && clipVertsIt->is_array()) {
                for (const auto& v : *clipVertsIt) {
                    if (v.is_array() && v.size() >= 2) {
                        float vx = v[0].get<float>() * scaleX;
                        float vy = v[1].get<float>() * scaleY;
                        cmd.clipVertices.push_back(glm::vec2(vx, vy));
                    }
                }
            }

            // Optional clip center
            auto clipCenterIt = elem.find("clipCenter");
            if (clipCenterIt != elem.end()) {
                glm::vec2 rawCenter = ParseVec2(*clipCenterIt);
                cmd.clipCenter = glm::vec2(rawCenter.x * scaleX, rawCenter.y * scaleY);
            }

            commands.push_back(cmd);

        }
    }

    LOGI("CustomDslRender: Parsed %zu commands from %zu elements", commands.size(), elements.size());
    return commands;
}

// Self-registration (OCP: no need to modify DslRenderRegistry.cpp)
static bool g_registered = [](){
    DslRenderRegistry::Register(std::make_unique<CustomDslRender>());
    return true;
}();

} // namespace application::dsl
