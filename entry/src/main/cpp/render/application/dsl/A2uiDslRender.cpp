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

#include "dsl/A2uiDslRender.h"
#include "dsl/CssStyleConverter.h"
#include "dsl/DslRenderRegistry.h"
#include "logger_common.h"
#include "agenui_engine/thirdparty/glm/glm.hpp"
#include <yoga/Yoga.h>
#include <algorithm>
#include <cmath>
#include <functional>

namespace application {
namespace dsl {

// ===== Data Model Path Resolution =====

nlohmann::json A2uiDslRender::ResolvePath(const nlohmann::json& root, const std::string& path) {
    if (path.empty() || path == "/") return root;
    nlohmann::json current = root;
    std::string segment;
    std::istringstream ss(path.substr(1));
    while (std::getline(ss, segment, '/')) {
        if (segment.empty()) continue;
        if (current.is_object() && current.contains(segment)) {
            current = current[segment];
        } else if (current.is_array()) {
            try {
                size_t idx = std::stoul(segment);
                if (idx < current.size()) current = current[idx];
                else return nlohmann::json();
            } catch (...) { return nlohmann::json(); }
        } else {
            return nlohmann::json();
        }
    }
    return current;
}

std::string A2uiDslRender::ResolveDynamicString(const nlohmann::json& value, const nlohmann::json& dataModel) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number()) return std::to_string(value.get<double>());
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    if (value.is_object() && value.contains("path")) {
        std::string path = value["path"].get<std::string>();
        auto resolved = ResolvePath(dataModel, path);
        if (resolved.is_null()) return "";
        if (resolved.is_string()) return resolved.get<std::string>();
        return resolved.dump();
    }
    return "";
}

// ===== Color Parsing =====

glm::vec4 A2uiDslRender::ParseColor(const std::string& hexColor) {
    std::string temp = hexColor;
    if (!temp.empty() && temp[0] == '#') temp = temp.substr(1);
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    if (temp.length() == 8) {
        a = std::stoul(temp.substr(0, 2), nullptr, 16) / 255.0f;
        temp = temp.substr(2);
    }
    if (temp.length() == 6) {
        r = std::stoul(temp.substr(0, 2), nullptr, 16) / 255.0f;
        g = std::stoul(temp.substr(2, 2), nullptr, 16) / 255.0f;
        b = std::stoul(temp.substr(4, 2), nullptr, 16) / 255.0f;
    }
    return glm::vec4(r, g, b, a);
}

glm::vec3 A2uiDslRender::ParseColorRGB(const std::string& hexColor) {
    auto c = ParseColor(hexColor);
    return glm::vec3(c.r, c.g, c.b);
}

// ===== Layout Helpers (static) =====

std::string A2uiDslRender::GetStyleString(const nlohmann::json& comp, const std::string& key, const std::string& defaultVal) {
    if (comp.contains("styles") && comp["styles"].is_object() && comp["styles"].contains(key)) {
        auto& s = comp["styles"][key];
        if (s.is_string()) return s.get<std::string>();
    }
    return defaultVal;
}

float A2uiDslRender::GetStylePx(const nlohmann::json& comp, const std::string& key, float defaultVal) {
    std::string s = GetStyleString(comp, key, "");
    if (s.empty()) return defaultVal;
    size_t pos = s.find("px");
    if (pos != std::string::npos) {
        try { return std::stof(s.substr(0, pos)); } catch (...) { return defaultVal; }
    }
    return defaultVal;
}

uint32_t A2uiDslRender::GetFontSizeFromVariant(const std::string& variant) {
    if (variant == "h1") return 90;
    if (variant == "h2") return 60;
    if (variant == "h3") return 50;
    if (variant == "h4") return 42;
    if (variant == "h5") return 36;
    if (variant == "body") return 32;
    if (variant == "caption") return 28;
    return 32;
}

float A2uiDslRender::MeasureTextWidth(const std::string& text, uint32_t fontSize) {
    static const float kAsciiAdvance[95] = {
        // space  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /
           9.0f,10.0f,15.0f,20.0f,19.0f,28.0f,28.0f, 9.0f,11.0f,11.0f,15.0f,24.0f, 8.0f,13.0f, 8.0f,14.0f,
        // 0    1    2    3    4    5    6    7    8    9
          19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,
        // :    ;    <    =    >    ?    @
           8.0f, 8.0f,24.0f,24.0f,24.0f,16.0f,34.0f,
        // A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    P    Q    R    S    T    U    V    W    X    Y    Z
          23.0f,20.0f,22.0f,25.0f,17.0f,17.0f,24.0f,24.0f, 9.0f,13.0f,20.0f,17.0f,31.0f,26.0f,27.0f,19.0f,27.0f,21.0f,19.0f,19.0f,24.0f,22.0f,33.0f,21.0f,19.0f,20.0f,
        // [    \    ]    ^    _    `
          10.0f,13.0f,10.0f,24.0f,14.0f, 9.0f,
        // a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    p    q    r    s    t    u    v    w    x    y    z
          17.0f,21.0f,16.0f,21.0f,18.0f,11.0f,21.0f,20.0f, 8.0f, 8.0f,18.0f, 9.0f,30.0f,20.0f,21.0f,21.0f,21.0f,13.0f,15.0f,12.0f,20.0f,17.0f,25.0f,16.0f,17.0f,16.0f,
        // {    |    }    ~
          11.0f, 9.0f,11.0f,24.0f
    };

    float scale = fontSize / 32.0f;
    float width = 0.0f;

    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 128) {
            if (c >= 32 && c <= 126) {
                width += kAsciiAdvance[c - 32] * scale;
            }
            i++;
        } else {
            size_t byteCount = 1;
            if      ((c & 0xE0) == 0xC0) byteCount = 2;
            else if ((c & 0xF0) == 0xE0) byteCount = 3;
            else if ((c & 0xF8) == 0xF0) byteCount = 4;

            uint32_t cp = 0;
            if (byteCount == 2 && i + 1 < text.size()) {
                cp = (static_cast<uint32_t>(text[i] & 0x1F) << 6) |
                      static_cast<uint32_t>(text[i+1] & 0x3F);
            } else if (byteCount == 3 && i + 2 < text.size()) {
                cp = (static_cast<uint32_t>(text[i] & 0x0F) << 12) |
                     (static_cast<uint32_t>(text[i+1] & 0x3F) << 6) |
                      static_cast<uint32_t>(text[i+2] & 0x3F);
            } else if (byteCount == 4 && i + 3 < text.size()) {
                cp = (static_cast<uint32_t>(text[i] & 0x07) << 18) |
                     (static_cast<uint32_t>(text[i+1] & 0x3F) << 12) |
                     (static_cast<uint32_t>(text[i+2] & 0x3F) << 6) |
                      static_cast<uint32_t>(text[i+3] & 0x3F);
            }

            if (cp == 0x2014)       width += 34.0f * scale;
            else if (cp == 0x2026)  width += 26.0f * scale;
            else if (cp == 0x00B7)  width += 8.0f * scale;
            else                    width += static_cast<float>(fontSize);

            i += byteCount;
        }
    }
    return width;
}

// ===== Yoga Measure Callback =====

struct MeasureContext {
    const nlohmann::json* comp;
    const nlohmann::json* dataModel;
};

static YGSize YogaMeasureCallback(YGNodeRef node, float width, YGMeasureMode widthMode,
                                   float height, YGMeasureMode heightMode) {
    auto* ctx = static_cast<const MeasureContext*>(YGNodeGetContext(node));
    if (!ctx || !ctx->comp) return {0, 0};
    const auto& comp = *ctx->comp;
    const nlohmann::json& dataModel = ctx->dataModel ? *ctx->dataModel : nlohmann::json::object();

    std::string type = comp.value("component", std::string(""));

    float w = A2uiDslRender::GetStylePx(comp, "width", 0);
    float h = A2uiDslRender::GetStylePx(comp, "height", 0);

    if (type == "Text") {
        std::string text = A2uiDslRender::ResolveDynamicString(comp.value("text", nlohmann::json("")), dataModel);
        std::string variant = comp.value("variant", std::string("body"));
        uint32_t fontSize = A2uiDslRender::GetStylePx(comp, "font-size", 0);
        if (fontSize == 0) fontSize = A2uiDslRender::GetFontSizeFromVariant(variant);

        // Split by newlines and measure the widest line (not total width)
        int lineCount = 1;
        if (w == 0 || h == 0) {
            float maxLineWidth = 0.0f;
            size_t start = 0;
            for (size_t i = 0; i <= text.size(); i++) {
                if (i == text.size() || text[i] == '\n') {
                    std::string line = text.substr(start, i - start);
                    float lw = A2uiDslRender::MeasureTextWidth(line, fontSize);
                    if (lw > maxLineWidth) maxLineWidth = lw;
                    start = i + 1;
                    if (i < text.size()) lineCount++;
                }
            }
            if (w == 0) w = maxLineWidth;
        }

        if (h == 0) {
            h = fontSize * 1.3f * lineCount;
        }
    } else if (type == "Image") {
        if (w == 0) w = 100;
        if (h == 0) h = 100;
    } else if (type == "Divider") {
        if (w == 0) w = (widthMode == YGMeasureModeUndefined) ? 1000 : width;
        if (h == 0) h = A2uiDslRender::GetStylePx(comp, "height", 1.0f);
    } else if (type == "Icon") {
        if (w == 0) w = 24;
        if (h == 0) h = 24;
    }

    return {static_cast<float>(w), static_cast<float>(h)};
}

// ===== Yoga Tree Builder =====

void A2uiDslRender::BuildYogaTree(
    const std::string& rootId,
    const std::unordered_map<std::string, nlohmann::json>& componentMap,
    float designWidth, float designHeight,
    const nlohmann::json& dataModel,
    std::unordered_map<std::string, LayoutBox>& boxes) const
{
    std::vector<YGNodeRef> allNodes;
    std::unordered_map<std::string, YGNodeRef> nodeMap;
    std::vector<MeasureContext> measureContexts;
    measureContexts.reserve(componentMap.size());

    for (const auto& [id, comp] : componentMap) {
        YGNodeRef node = YGNodeNew();
        allNodes.push_back(node);
        nodeMap[id] = node;

        std::string type = comp.value("component", std::string(""));

        measureContexts.push_back({&comp, &dataModel});
        YGNodeSetContext(node, &measureContexts.back());

        bool isContainer = (type == "Column" || type == "Row" || type == "Card" || type == "Button");

        // Component-type defaults
        if (type == "Column") {
            YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumn);
        } else if (type == "Row") {
            YGNodeStyleSetFlexDirection(node, YGFlexDirectionRow);
        } else if (type == "Button") {
            YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumn);
            YGNodeStyleSetAlignSelf(node, YGAlignFlexStart);
        }

        // Apply CSS styles
        if (comp.contains("styles") && comp["styles"].is_object()) {
            CssStyleConverter::applyStyles(comp["styles"], node);
        } else if (type == "Card" || type == "Button") {
            YGNodeStyleSetPadding(node, YGEdgeAll, 16.0f);
        }

        // Leaf nodes need measure function
        if (!isContainer) {
            YGNodeSetMeasureFunc(node, YogaMeasureCallback);
        }
    }

    // Build parent-child relationships
    for (const auto& [id, comp] : componentMap) {
        if (!nodeMap.count(id)) continue;
        YGNodeRef parent = nodeMap[id];
        std::string type = comp.value("component", std::string(""));

        if (comp.contains("children") && comp["children"].is_array()) {
            uint32_t idx = 0;
            for (const auto& childRef : comp["children"]) {
                if (!childRef.is_string()) continue;
                std::string childId = childRef.get<std::string>();
                if (nodeMap.count(childId)) {
                    YGNodeInsertChild(parent, nodeMap[childId], idx++);
                }
            }
        }
        if ((type == "Card" || type == "Button") && comp.contains("child") && comp["child"].is_string()) {
            std::string childId = comp["child"].get<std::string>();
            if (nodeMap.count(childId)) {
                YGNodeInsertChild(parent, nodeMap[childId], 0);
            }
        }
    }

    YGNodeRef rootNode = nodeMap[rootId];
    if (!rootNode) {
        for (auto n : allNodes) YGNodeFree(n);
        return;
    }

    // Root always fills the design space and centers content vertically by default.
    YGNodeStyleSetWidth(rootNode, designWidth);
    YGNodeStyleSetHeight(rootNode, designHeight);
    YGNodeStyleSetJustifyContent(rootNode, YGJustifyCenter);

    // Run layout
    YGNodeCalculateLayout(rootNode, designWidth, designHeight, YGDirectionLTR);

    // Read results — Yoga returns positions relative to parent, so we must
    // traverse the tree and accumulate offsets to get absolute coordinates.
    std::function<void(const std::string&, float, float)> readAbsolute;
    readAbsolute = [&](const std::string& compId, float parentX, float parentY) {
        auto it = nodeMap.find(compId);
        if (it == nodeMap.end()) return;
        YGNodeRef node = it->second;

        float relX = YGNodeLayoutGetLeft(node);
        float relY = YGNodeLayoutGetTop(node);

        LayoutBox box;
        box.componentId = compId;
        box.componentType = componentMap.count(compId) ? componentMap.at(compId).value("component", std::string("")) : "";
        box.x = parentX + relX;
        box.y = parentY + relY;
        box.width = YGNodeLayoutGetWidth(node);
        box.height = YGNodeLayoutGetHeight(node);
        boxes[compId] = box;

        if (!componentMap.count(compId)) return;
        const auto& comp = componentMap.at(compId);
        std::string type = comp.value("component", std::string(""));

        if (comp.contains("children") && comp["children"].is_array()) {
            for (const auto& childRef : comp["children"]) {
                if (!childRef.is_string()) continue;
                readAbsolute(childRef.get<std::string>(), box.x, box.y);
            }
        }
        if ((type == "Card" || type == "Button") && comp.contains("child") && comp["child"].is_string()) {
            readAbsolute(comp["child"].get<std::string>(), box.x, box.y);
        }
    };

    readAbsolute(rootId, 0.0f, 0.0f);

    // Debug: log root and first-level children for centering diagnostics
    if (boxes.count(rootId)) {
        auto& rb = boxes.at(rootId);
        LOGI("A2uiDslRender: root box=(%.0f,%.0f,%.0f,%.0f)", rb.x, rb.y, rb.width, rb.height);
        if (componentMap.count(rootId)) {
            const auto& rootComp = componentMap.at(rootId);
            if (rootComp.contains("children") && rootComp["children"].is_array()) {
                for (const auto& cid : rootComp["children"]) {
                    if (!cid.is_string()) continue;
                    std::string id = cid.get<std::string>();
                    if (boxes.count(id)) {
                        auto& cb = boxes.at(id);
                        LOGI("A2uiDslRender: child '%s' box=(%.0f,%.0f,%.0f,%.0f)", id.c_str(), cb.x, cb.y, cb.width, cb.height);
                    }
                }
            }
            if ((rootComp.value("component", "") == "Card" || rootComp.value("component", "") == "Button")
                && rootComp.contains("child") && rootComp["child"].is_string()) {
                std::string id = rootComp["child"].get<std::string>();
                if (boxes.count(id)) {
                    auto& cb = boxes.at(id);
                    LOGI("A2uiDslRender: child '%s' box=(%.0f,%.0f,%.0f,%.0f)", id.c_str(), cb.x, cb.y, cb.width, cb.height);
                }
            }
        }
    }

    // Cleanup
    for (auto n : allNodes) YGNodeFree(n);
}

// ===== Component to DslRenderCommand =====

bool A2uiDslRender::ConvertComponent(const nlohmann::json& comp, const LayoutBox& box,
                                   const std::unordered_map<std::string, nlohmann::json>& componentMap,
                                   const std::unordered_map<std::string, LayoutBox>& boxes,
                                   const nlohmann::json& dataModel,
                                   std::vector<DslRenderCommand>& commands,
                                   float containerWidth, float containerHeight,
                                   float layoutScale) const {
    std::string type = comp.value("component", std::string(""));

    if (type == "Column" || type == "Row") {
        // Draw background if specified
        std::string bgColor = GetStyleString(comp, "background-color", "");
        float borderRadius = GetStylePx(comp, "border-radius", 0) * layoutScale;
        if (!bgColor.empty()) {
            DslRenderCommand bg;
            bg.type = DslRenderCommand::Type::Rect;
            bg.pos = glm::vec2(box.x, box.y);
            bg.size = glm::vec2(box.width, box.height);
            bg.color = ParseColor(bgColor);
            bg.radius = borderRadius;
            commands.push_back(bg);
        }

        if (comp.contains("children") && comp["children"].is_array()) {
            for (const auto& childId : comp["children"]) {
                if (!childId.is_string()) continue;
                std::string cid = childId.get<std::string>();
                if (componentMap.count(cid) && boxes.count(cid)) {
                    ConvertComponent(componentMap.at(cid), boxes.at(cid), componentMap, boxes, dataModel, commands, containerWidth, containerHeight, layoutScale);
                }
            }
        }
        return true;
    }

    if (type == "Card" || type == "Button") {
        std::string bgColor = GetStyleString(comp, "background-color", "#FFFFFF");
        float borderRadius = GetStylePx(comp, "border-radius", 12.0f) * layoutScale;

        DslRenderCommand cmd;
        cmd.type = DslRenderCommand::Type::Rect;
        cmd.pos = glm::vec2(box.x, box.y);
        cmd.size = glm::vec2(box.width, box.height);
        cmd.color = ParseColor(bgColor);
        cmd.radius = borderRadius;
        commands.push_back(cmd);

        std::string childId;
        if (comp.contains("child") && comp["child"].is_string()) {
            childId = comp["child"].get<std::string>();
        }
        if (!childId.empty() && componentMap.count(childId) && boxes.count(childId)) {
            const auto& childComp = componentMap.at(childId);
            LayoutBox childBox = boxes.at(childId);
            // Button: compute actual text metrics and center within button
            if (type == "Button" && childComp.value("component", "") == "Text") {
                std::string text = ResolveDynamicString(childComp.value("text", nlohmann::json("")), dataModel);
                uint32_t fsRaw = GetStylePx(childComp, "font-size", 0);
                if (fsRaw == 0) fsRaw = GetFontSizeFromVariant(childComp.value("variant", std::string("body")));
                uint32_t fs = static_cast<uint32_t>(fsRaw * layoutScale);
                float tw = MeasureTextWidth(text, fs);
                float th = fs * 1.3f;
                childBox.x = box.x + (box.width - tw) * 0.5f;
                childBox.y = box.y + (box.height - th) * 0.5f + fs * 0.1f;
                childBox.width = tw;
                childBox.height = th;
            }
            ConvertComponent(childComp, childBox, componentMap, boxes, dataModel, commands, containerWidth, containerHeight, layoutScale);
        }
        return true;
    }

    if (type == "Text") {
        nlohmann::json textVal = comp.value("text", nlohmann::json(""));
        std::string text = ResolveDynamicString(textVal, dataModel);
        if (text.empty()) return false;

        std::string variant = comp.value("variant", std::string("body"));
        uint32_t fontSizeRaw = GetStylePx(comp, "font-size", 0);
        if (fontSizeRaw == 0) fontSizeRaw = GetFontSizeFromVariant(variant);
        uint32_t fontSize = static_cast<uint32_t>(fontSizeRaw * layoutScale);

        // Render background if specified (e.g., badge-style Text with bg)
        std::string bgColor = GetStyleString(comp, "background-color", "");
        if (!bgColor.empty()) {
            DslRenderCommand bg;
            bg.type = DslRenderCommand::Type::Rect;
            bg.pos = glm::vec2(box.x, box.y);
            bg.size = glm::vec2(box.width, box.height);
            bg.color = ParseColor(bgColor);
            bg.radius = GetStylePx(comp, "border-radius", 0) * layoutScale;
            commands.push_back(bg);
        }

        std::string textColor = GetStyleString(comp, "color", "#000000");
        glm::vec3 color = ParseColorRGB(textColor);

        // Determine text start position — offset by padding if background present
        float textX = box.x;
        float textY = box.y;
        if (!bgColor.empty()) {
            std::string padStr = GetStyleString(comp, "padding", "");
            if (!padStr.empty()) {
                float pv[4] = {0};
                int pi = 0;
                std::istringstream pss(padStr);
                std::string pt;
                while (pss >> pt && pi < 4) {
                    size_t ppx = pt.find("px");
                    if (ppx != std::string::npos) {
                        try { pv[pi++] = std::stof(pt.substr(0, ppx)); } catch (...) {}
                    }
                }
                textY += pv[0] * layoutScale; // top padding
                textX += ((pi >= 4) ? pv[3] : (pi >= 2) ? pv[1] : pv[0]) * layoutScale; // left padding
            }
        }

        float lineY = textY;
        float lineHeight = fontSize * 1.3f;
        size_t start = 0;
        size_t newlinePos;

        while ((newlinePos = text.find('\n', start)) != std::string::npos) {
            std::string line = text.substr(start, newlinePos - start);
            if (!line.empty()) {
                DslRenderCommand cmd;
                cmd.type = DslRenderCommand::Type::Text;
                cmd.text = line;
                cmd.pos = glm::vec2(textX, lineY);
                cmd.fontSize = fontSize;
                cmd.color = glm::vec4(color, 1.0f);
                commands.push_back(cmd);
            }
            lineY += lineHeight;
            start = newlinePos + 1;
        }
        std::string lastLine = text.substr(start);
        if (!lastLine.empty()) {
            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Text;
            cmd.text = lastLine;
            cmd.pos = glm::vec2(textX, lineY);
            cmd.fontSize = fontSize;
            cmd.color = glm::vec4(color, 1.0f);
            commands.push_back(cmd);
        }
        return true;
    }

    if (type == "Image") {
        std::string url = ResolveDynamicString(comp.value("url", nlohmann::json("")), dataModel);
        if (url.empty()) return false;

        DslRenderCommand cmd;
        cmd.type = DslRenderCommand::Type::Image;
        cmd.imagePath = url;
        cmd.pos = glm::vec2(box.x, box.y);
        cmd.size = glm::vec2(box.width, box.height);
        cmd.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        commands.push_back(cmd);
        return true;
    }

    if (type == "Divider") {
        std::string colorStr = GetStyleString(comp, "color", "#CCCCCC");
        float thickness = GetStylePx(comp, "height", 1.0f) * layoutScale;

        DslRenderCommand cmd;
        cmd.type = DslRenderCommand::Type::Rect;
        cmd.pos = glm::vec2(box.x, box.y);
        cmd.size = glm::vec2(box.width, thickness);
        cmd.color = ParseColor(colorStr);
        cmd.radius = 0.0f;
        commands.push_back(cmd);
        return true;
    }

    LOGW("A2uiDslRender: Unsupported component type '%s'", type.c_str());
    return false;
}

// ===== Parse entry point =====

bool A2uiDslRender::CanParse(const nlohmann::json& root) const {
    if (!root.is_object()) return false;
    if (root.contains("updateComponents")) {
        if (root["updateComponents"].contains("components") && root["updateComponents"]["components"].is_array()
            && !root["updateComponents"]["components"].empty()) {
            auto& first = root["updateComponents"]["components"][0];
            if (first.is_object() && first.contains("type") && !first.contains("component")) {
                return false;
            }
        }
        return true;
    }
    if (root.contains("version")) {
        std::string v = root["version"].get<std::string>();
        if (v.find("v0.9") != std::string::npos || v.find("v0.8") != std::string::npos) return true;
    }
    return false;
}

std::vector<DslRenderCommand> A2uiDslRender::Parse(
    const nlohmann::json& root,
    const ParseContext& ctx
) const {
    std::vector<DslRenderCommand> commands;

    nlohmann::json updateComp;
    if (root.contains("updateComponents") && root["updateComponents"].is_object()) {
        updateComp = root["updateComponents"];
    } else {
        LOGE("A2uiDslRender: No updateComponents found");
        return {};
    }

    nlohmann::json effectiveDataModel = ctx.dataModel;
    if (root.contains("updateDataModel") && root["updateDataModel"].is_object()) {
        auto& udm = root["updateDataModel"];
        if (udm.contains("value")) {
            effectiveDataModel = udm["value"];
            LOGI("A2uiDslRender: Loaded data model from updateDataModel (%zu top-level keys)",
                 effectiveDataModel.is_object() ? effectiveDataModel.size() : 0);
        }
    }

    std::unordered_map<std::string, nlohmann::json> componentMap;
    if (updateComp.contains("components") && updateComp["components"].is_array()) {
        for (const auto& comp : updateComp["components"]) {
            if (comp.is_object() && comp.contains("id")) {
                std::string id = comp["id"].get<std::string>();
                componentMap[id] = comp;
            }
        }
    }

    if (componentMap.empty()) {
        LOGE("A2uiDslRender: No components found");
        return {};
    }

    std::string rootId = "root";
    if (!componentMap.count("root")) {
        rootId = componentMap.begin()->first;
        LOGW("A2uiDslRender: No 'root' component found, using '%s'", rootId.c_str());
    }

    float designWidth = ctx.designWidth;
    float designHeight = ctx.designHeight;

    // A2UI CSS values are in "a2ui" units (a2ui = vp × 2).
    // Our design dimensions are physical pixels (vp × density).
    // Scale factor = density / 2 converts a2ui → physical pixels.
    // We run Yoga in a2ui space, then scale output to physical pixels.
    float layoutScale = 1.0f;
    float density = ctx.density;
    if (density > 0.0f) {
        layoutScale = density / 2.0f;
    }

    float yogaW = designWidth / layoutScale;
    float yogaH = designHeight / layoutScale;

    LOGI("A2uiDslRender: design=%.0fx%.0f density=%.2f scale=%.2f yoga=%.0fx%.0f",
         designWidth, designHeight, density, layoutScale, yogaW, yogaH);

    // Build Yoga tree and run layout in a2ui space
    std::unordered_map<std::string, LayoutBox> boxes;
    BuildYogaTree(rootId, componentMap, yogaW, yogaH, effectiveDataModel, boxes);

    // Scale all boxes from a2ui to physical pixels
    if (layoutScale != 1.0f) {
        for (auto& [id, box] : boxes) {
            box.x *= layoutScale;
            box.y *= layoutScale;
            box.width *= layoutScale;
            box.height *= layoutScale;
        }
    }

    if (boxes.empty()) {
        LOGE("A2uiDslRender: Yoga layout produced no results");
        return {};
    }

    // Convert layout boxes to render commands
    if (componentMap.count(rootId) && boxes.count(rootId)) {
        ConvertComponent(componentMap.at(rootId), boxes.at(rootId), componentMap, boxes, effectiveDataModel, commands, designWidth, designHeight, layoutScale);
    }

    LOGI("A2uiDslRender: Parsed %zu commands from %zu components", commands.size(), componentMap.size());
    return commands;
}

// Self-registration
static bool g_registered = [](){
    DslRenderRegistry::Register(std::make_unique<A2uiDslRender>());
    return true;
}();

} // namespace dsl
} // namespace application
