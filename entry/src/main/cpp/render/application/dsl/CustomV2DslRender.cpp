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

#include "dsl/CustomV2DslRender.h"
#include "dsl/DslRenderRegistry.h"
#include "logger_common.h"
#include "agenui_engine/thirdparty/glm/glm.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>

namespace application::dsl {

// ===== ID Conversion Helper =====

static std::string JsonIdToString(const nlohmann::json& v) {
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<int>());
    if (v.is_number_unsigned()) return std::to_string(v.get<unsigned int>());
    return "";
}

// ===== Cached layout context (built once, passed to all layout functions) =====

struct LayoutContext {
    std::unordered_map<std::string, nlohmann::json>& componentMap;
    std::unordered_map<std::string, std::vector<std::string>> childrenMap;
    std::unordered_map<std::string, std::string> childToParent;
    std::unordered_map<std::string, std::pair<float, float>> measureCache;

    explicit LayoutContext(std::unordered_map<std::string, nlohmann::json>& cm)
        : componentMap(cm) { RebuildChildMaps(); }

    void RebuildChildMaps() {
        childrenMap.clear();
        childToParent.clear();
        for (const auto& [id, comp] : componentMap) {
            auto& ch = childrenMap[id];
            if (comp.contains("children") && comp["children"].is_array()) {
                for (const auto& c : comp["children"]) {
                    std::string cid = JsonIdToString(c);
                    if (!cid.empty()) {
                        if (!childToParent.count(cid)) childToParent[cid] = id;
                        ch.push_back(std::move(cid));
                    }
                }
            }
        }
    }
};

static const std::string kEmpty;

static uint32_t ResolveFontSize(const nlohmann::json& comp, uint32_t fallback = 40) {
    if (comp.contains("fontSize") && comp["fontSize"].is_number()) {
        return static_cast<uint32_t>(std::max(1.0f, comp["fontSize"].get<float>()));
    }
    std::string variant = comp.value("variant", std::string(""));
    if (variant == "display") return 132;
    if (variant == "h1") return 126;
    if (variant == "h2") return 96;
    if (variant == "h3") return 64;
    if (variant == "h4") return 52;
    if (variant == "h5") return 44;
    if (variant == "body") return 40;
    if (variant == "caption") return 36;
    if (variant == "micro") return 32;
    return fallback;
}

// ===== Color Parsing =====

glm::vec4 CustomV2DslRender::ParseColor(const std::string& hexColor) {
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

// ===== Text Measurement =====

float CustomV2DslRender::MeasureTextWidth(const std::string& text, uint32_t fontSize) {
    static const float kAsciiAdvance[95] = {
        9.0f,10.0f,15.0f,20.0f,19.0f,28.0f,28.0f, 9.0f,11.0f,11.0f,15.0f,24.0f, 8.0f,13.0f, 8.0f,14.0f,
       19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,19.0f,
        8.0f, 8.0f,24.0f,24.0f,24.0f,16.0f,34.0f,
       23.0f,20.0f,22.0f,25.0f,17.0f,17.0f,24.0f,24.0f, 9.0f,13.0f,20.0f,17.0f,31.0f,26.0f,27.0f,19.0f,27.0f,21.0f,19.0f,19.0f,24.0f,22.0f,33.0f,21.0f,19.0f,20.0f,
       10.0f,13.0f,10.0f,24.0f,14.0f, 9.0f,
       17.0f,21.0f,16.0f,21.0f,18.0f,11.0f,21.0f,20.0f, 8.0f, 8.0f,18.0f, 9.0f,30.0f,20.0f,21.0f,21.0f,21.0f,13.0f,15.0f,12.0f,20.0f,17.0f,25.0f,16.0f,17.0f,16.0f,
       11.0f, 9.0f,11.0f,24.0f
    };

    float scale = fontSize / 32.0f;
    float width = 0.0f;

    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 128) {
            if (c >= 32 && c <= 126) width += kAsciiAdvance[c - 32] * scale;
            i++;
        } else {
            size_t byteCount = 1;
            if      ((c & 0xE0) == 0xC0) byteCount = 2;
            else if ((c & 0xF0) == 0xE0) byteCount = 3;
            else if ((c & 0xF8) == 0xF0) byteCount = 4;

            uint32_t cp = 0;
            if (byteCount == 2 && i + 1 < text.size())
                cp = (static_cast<uint32_t>(text[i] & 0x1F) << 6) | static_cast<uint32_t>(text[i+1] & 0x3F);
            else if (byteCount == 3 && i + 2 < text.size())
                cp = (static_cast<uint32_t>(text[i] & 0x0F) << 12) | (static_cast<uint32_t>(text[i+1] & 0x3F) << 6) | static_cast<uint32_t>(text[i+2] & 0x3F);
            else if (byteCount == 4 && i + 3 < text.size())
                cp = (static_cast<uint32_t>(text[i] & 0x07) << 18) | (static_cast<uint32_t>(text[i+1] & 0x3F) << 12) | (static_cast<uint32_t>(text[i+2] & 0x3F) << 6) | static_cast<uint32_t>(text[i+3] & 0x3F);

            if (cp == 0x2014) width += 34.0f * scale;
            else if (cp == 0x2026) width += 26.0f * scale;
            else if (cp == 0x00B7) width += 8.0f * scale;
            else width += static_cast<float>(fontSize);
            i += byteCount;
        }
    }
    return width;
}

// ===== Data Path Resolution =====

std::string CustomV2DslRender::ResolveDataPath(const nlohmann::json& dataModel, const std::string& path) {
    if (path.empty() || !dataModel.is_object()) return "";
    nlohmann::json current = dataModel;
    std::string segment;
    std::istringstream ss(path.substr(1));
    while (std::getline(ss, segment, '/')) {
        if (segment.empty()) continue;
        if (current.is_object() && current.contains(segment)) {
            current = current[segment];
        } else if (current.is_array()) {
            try {
                size_t idx = std::stoul(segment);
                if (idx < current.size()) current = current[idx]; else return "";
            } catch (...) { return ""; }
        } else return "";
    }
    if (current.is_string()) return current.get<std::string>();
    if (current.is_number()) return std::to_string(current.get<double>());
    if (current.is_boolean()) return current.get<bool>() ? "true" : "false";
    return current.dump();
}

std::string CustomV2DslRender::ResolveContent(const nlohmann::json& comp, const nlohmann::json& dataModel) {
    if (comp.contains("dataPath") && comp["dataPath"].is_string()) {
        std::string resolved = ResolveDataPath(dataModel, comp["dataPath"].get<std::string>());
        if (!resolved.empty()) return resolved;
    }
    if (comp.contains("content")) {
        const auto& v = comp["content"];
        if (v.is_string()) return v.get<std::string>();
        if (v.is_number()) return std::to_string(v.get<double>());
    }
    return "";
}

// ===== Size Parsing =====

static std::tuple<float, bool> ParseSizeValue(const nlohmann::json& val) {
    if (val.is_number()) return {val.get<float>(), false};
    if (val.is_string()) {
        std::string s = val.get<std::string>();
        if (s.size() > 1 && s.back() == '%') {
            try { return {std::stof(s.substr(0, s.size() - 1)) / 100.0f, true}; } catch (...) {}
        }
        try { return {std::stof(s), false}; } catch (...) {}
    }
    return {0, false};
}

static glm::vec2 ParseVec2(const nlohmann::json& j) {
    if (j.is_array() && j.size() >= 2) return glm::vec2(j[0].get<float>(), j[1].get<float>());
    return glm::vec2(0.0f);
}

// ===== Padding Parsing =====

static void ParsePadding(const nlohmann::json& comp, float pad[4]) {
    pad[0] = pad[1] = pad[2] = pad[3] = 0;
    if (!comp.contains("padding")) return;
    const auto& pv = comp["padding"];
    if (pv.is_number()) {
        float v = pv.get<float>(); pad[0] = pad[1] = pad[2] = pad[3] = v;
    } else if (pv.is_array()) {
        if (pv.size() == 1) { float v = pv[0].get<float>(); pad[0] = pad[1] = pad[2] = pad[3] = v; }
        else if (pv.size() >= 4) { pad[0]=pv[0].get<float>(); pad[1]=pv[1].get<float>(); pad[2]=pv[2].get<float>(); pad[3]=pv[3].get<float>(); }
    }
}

// ===== Measure Text Intrinsic Height =====

static float MeasureTextHeight(const std::string& text, uint32_t fontSize) {
    int lineCount = 1;
    for (size_t i = 0; i < text.size(); i++) { if (text[i] == '\n') lineCount++; }
    return fontSize * 1.3f * static_cast<float>(lineCount);
}

// ===== Forward declarations =====

static void LayoutNode(const std::string& compId,
                       CustomV2DslRender::LayoutBox& box,
                       LayoutContext& ctx,
                       std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes);

// ===== Intrinsic Size Measurement (with cache) =====

static std::pair<float, float> MeasureIntrinsicSize(
    const std::string& compId,
    LayoutContext& ctx,
    float parentAvailW)
{
    if (!ctx.componentMap.count(compId)) return {0, 0};
    const auto& comp = ctx.componentMap.at(compId);
    std::string type = comp.value("type", kEmpty);

    float w = 0, h = 0;
    bool wIsPct = false, hIsPct = false;

    // Text: cache — intrinsic size is deterministic, never depends on parentAvailW
    if (type == "text") {
        auto cacheIt = ctx.measureCache.find(compId);
        if (cacheIt != ctx.measureCache.end()) return cacheIt->second;
        std::string text = CustomV2DslRender::ResolveContent(comp, {});
        uint32_t fontSize = ResolveFontSize(comp);
        float maxLineW = 0;
        size_t lineStart = 0;
        while (lineStart < text.size()) {
            size_t nlPos = text.find('\n', lineStart);
            std::string line = (nlPos != std::string::npos)
                ? text.substr(lineStart, nlPos - lineStart)
                : text.substr(lineStart);
            float lineW = CustomV2DslRender::MeasureTextWidth(line, fontSize);
            maxLineW = std::max(maxLineW, lineW);
            if (nlPos != std::string::npos) lineStart = nlPos + 1;
            else break;
        }
        w = maxLineW;
        h = MeasureTextHeight(text, fontSize);
        ctx.measureCache[compId] = {w, h};
        return {w, h};
    }

    // Parse size from JSON (Rect, Image)
    auto sizeArr = comp.find("size");
    if (sizeArr != comp.end() && sizeArr->is_array() && sizeArr->size() >= 2) {
        auto [wv, wp] = ParseSizeValue((*sizeArr)[0]);
        auto [hv, hp] = ParseSizeValue((*sizeArr)[1]);
        w = wv; wIsPct = wp;
        h = hv; hIsPct = hp;
    }
    if (wIsPct) w = w * parentAvailW;
    if (hIsPct) h = h * parentAvailW;

    float pad[4] = {0};
    ParsePadding(comp, pad);
    float innerW = (w > 0 ? w : parentAvailW) - pad[1] - pad[3];

    if (type == "img") {
        if (w == 0) w = 100;
        if (h == 0) h = 100;
    } else if (type == "circle") {
        float radius = comp.value("radius", 50.0f);
        w = h = radius * 2.0f;
    } else if (type == "rect") {
        std::string dir = comp.value("direction", kEmpty);
        float gap = comp.value("gap", 0.0f);

        const auto& children = ctx.childrenMap[compId];
        if (dir == "vertical") {
            float totalChildH = 0;
            float maxChildW = 0;
            int childCount = 0;
            for (const auto& childId : children) {
                auto [cw, ch] = MeasureIntrinsicSize(childId, ctx, innerW);
                totalChildH += ch;
                maxChildW = std::max(maxChildW, cw);
                childCount++;
            }
            if (childCount > 1) totalChildH += gap * (childCount - 1);
            if (w == 0 || wIsPct) w = maxChildW + pad[1] + pad[3];
            if (h == 0) h = totalChildH + pad[0] + pad[2];
        } else if (dir == "horizontal") {
            float totalChildW = 0, maxChildH = 0;
            int childCount = 0;
            for (const auto& childId : children) {
                auto [cw, ch] = MeasureIntrinsicSize(childId, ctx, innerW);
                totalChildW += cw;
                maxChildH = std::max(maxChildH, ch);
                childCount++;
            }
            if (childCount > 1) totalChildW += gap * (childCount - 1);
            if (w == 0 || wIsPct) w = totalChildW + pad[1] + pad[3];
            if (h == 0) h = maxChildH + pad[0] + pad[2];
        } else {
            float maxCW = 0, maxCH = 0;
            for (const auto& childId : children) {
                auto [cw, ch] = MeasureIntrinsicSize(childId, ctx, innerW);
                maxCW = std::max(maxCW, cw);
                maxCH = std::max(maxCH, ch);
            }
            if (w == 0) w = maxCW + pad[1] + pad[3];
            if (h == 0) h = maxCH + pad[0] + pad[2];
        }
    }

    return {w, h};
}

// ===== Layout Engine =====

static void LayoutFlow(
    const nlohmann::json& comp,
    const std::string& compId,
    CustomV2DslRender::LayoutBox& box,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes,
    bool horizontal)
{
    const auto& children = ctx.childrenMap[compId];
    if (children.empty()) return;

    std::string alignStr = comp.value("align", std::string("start"));
    std::string justifyStr = comp.value("justify", std::string("start"));
    float gap = comp.value("gap", 0.0f);

    float availW = box.width - box.padding[1] - box.padding[3];
    float availH = box.height - box.padding[0] - box.padding[2];

    // Measure all children
    std::vector<float> childW, childH;
    float totalMain = 0;
    for (const auto& childId : children) {
        if (!ctx.componentMap.count(childId)) { childW.push_back(0); childH.push_back(0); continue; }
        auto [w, h] = MeasureIntrinsicSize(childId, ctx, availW);
        const auto& childComp = ctx.componentMap.at(childId);
        std::string childType = childComp.value("type", kEmpty);
        std::string childDir = childComp.value("direction", kEmpty);
        bool childHasExplicitSize = (childComp.find("size") != childComp.end() && childComp["size"].is_array());
        bool isFlowContainer = (childType == "rect" && !childDir.empty());
        if (horizontal) {
            if (childType == "rect" && !childHasExplicitSize && !isFlowContainer) h = availH;
            else if (h == 0) h = availH;
            if (w == 0) w = 60;
        } else {
            if (childType == "rect" && !childHasExplicitSize && !isFlowContainer) w = availW;
            else if (w == 0) w = availW;
        }
        childW.push_back(w); childH.push_back(h);
        totalMain += horizontal ? w : h;
    }

    float availMain = horizontal ? availW : availH;
    float availCross = horizontal ? availH : availW;
    float baseGapTotal = children.size() > 1 ? gap * static_cast<float>(children.size() - 1) : 0.0f;
    float extraSpace = std::max(0.0f, availMain - totalMain - baseGapTotal);
    float childGap = gap;
    float startMain = horizontal ? box.padding[3] : box.padding[0];

    if (justifyStr == "center") startMain += extraSpace / 2.0f;
    else if (justifyStr == "end") startMain += extraSpace;
    else if (justifyStr == "space-between" && children.size() > 1) childGap = gap + extraSpace / static_cast<float>(children.size() - 1);
    else if (justifyStr == "space-around") { childGap = gap + extraSpace / static_cast<float>(children.size()); startMain += extraSpace / static_cast<float>(children.size()) / 2.0f; }

    float posMain = startMain;
    for (size_t idx = 0; idx < children.size(); idx++) {
        if (!ctx.componentMap.count(children[idx])) continue;
        float cw = childW[idx], ch = childH[idx];
        const auto& childComp = ctx.componentMap.at(children[idx]);
        auto posIt = childComp.find("pos");

        CustomV2DslRender::LayoutBox childBox;
        childBox.componentId = children[idx];
        childBox.componentType = childComp.value("type", kEmpty);
        childBox.width = cw;
        childBox.height = ch;
        ParsePadding(childComp, childBox.padding);

        bool useAutoPos = true;
        if (posIt != childComp.end() && posIt->is_array()) {
            glm::vec2 absPos = ParseVec2(*posIt);
            // Guard: if child pos is completely outside parent box, treat as if pos was not set
            bool outsideParent = (absPos.x + cw <= box.x) || (absPos.x >= box.x + availW) ||
                                 (absPos.y + ch <= box.y) || (absPos.y >= box.y + availH);
            if (!outsideParent) {
                childBox.x = absPos.x;
                childBox.y = absPos.y;
                useAutoPos = false;
            } else {
                LOGI("AutoFix: child '%s' pos outside parent '%s' in flow, falling back to auto-center", children[idx].c_str(), compId.c_str());
            }
        }
        if (useAutoPos) {
            float crossPos = 0;
            float childCross = horizontal ? ch : cw;

            if (alignStr == "center") crossPos = (availCross - childCross) / 2.0f;
            else if (alignStr == "end") crossPos = availCross - childCross;
            if (horizontal) {
                childBox.x = box.x + posMain;
                childBox.y = box.y + box.padding[0] + crossPos;
            } else {
                childBox.x = box.x + box.padding[3] + crossPos;
                childBox.y = box.y + posMain;
            }
            posMain += (horizontal ? cw : ch) + childGap;
        }

        boxes[children[idx]] = childBox;
        LayoutNode(children[idx], childBox, ctx, boxes);
    }
}

static void LayoutStack(
    const nlohmann::json& comp,
    const std::string& compId,
    CustomV2DslRender::LayoutBox& box,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    const auto& children = ctx.childrenMap[compId];
    if (children.empty()) return;

    std::string alignStr = comp.value("align", std::string("start"));
    std::string justifyStr = comp.value("justify", std::string("start"));

    float availW = box.width - box.padding[1] - box.padding[3];
    float availH = box.height - box.padding[0] - box.padding[2];

    for (const auto& childId : children) {
        if (!ctx.componentMap.count(childId)) continue;
        const auto& childComp = ctx.componentMap.at(childId);
        auto [w, h] = MeasureIntrinsicSize(childId, ctx, availW);
        if (w == 0) w = availW;

        auto posIt = childComp.find("pos");

        CustomV2DslRender::LayoutBox childBox;
        childBox.componentId = childId;
        childBox.componentType = childComp.value("type", kEmpty);
        childBox.width = w;
        childBox.height = h;
        ParsePadding(childComp, childBox.padding);

        bool useAutoPos = true;
        if (posIt != childComp.end() && posIt->is_array()) {
            glm::vec2 absPos = ParseVec2(*posIt);
            // Guard: if child pos is completely outside parent box, treat as if pos was not set
            // (fallback to align/justify auto-centering). Cost: 4 float comparisons.
            bool outsideParent = (absPos.x + w <= box.x) || (absPos.x >= box.x + availW) ||
                                 (absPos.y + h <= box.y) || (absPos.y >= box.y + availH);
            if (!outsideParent) {
                childBox.x = absPos.x;
                childBox.y = absPos.y;
                useAutoPos = false;
            } else {
                LOGI("AutoFix: child '%s' pos outside parent '%s', falling back to auto-center", childId.c_str(), compId.c_str());
            }
        }
        if (useAutoPos) {
            if (alignStr == "center") childBox.x = box.x + box.padding[3] + (availW - w) / 2.0f;
            else if (alignStr == "end") childBox.x = box.x + box.padding[3] + availW - w;
            else childBox.x = box.x + box.padding[3];

            if (justifyStr == "center") childBox.y = box.y + box.padding[0] + (availH - h) / 2.0f;
            else if (justifyStr == "end") childBox.y = box.y + box.padding[0] + availH - h;
            else childBox.y = box.y + box.padding[0];
        }

        boxes[childId] = childBox;
        LayoutNode(childId, childBox, ctx, boxes);
    }
}

static void LayoutNode(const std::string& compId,
                       CustomV2DslRender::LayoutBox& box,
                       LayoutContext& ctx,
                       std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    if (!ctx.componentMap.count(compId)) return;
    const auto& comp = ctx.componentMap.at(compId);
    std::string type = comp.value("type", kEmpty);

    ParsePadding(comp, box.padding);

    if (type == "rect") {
        std::string dir = comp.value("direction", kEmpty);
        if (dir == "vertical") LayoutFlow(comp, compId, box, ctx, boxes, false);
        else if (dir == "horizontal") LayoutFlow(comp, compId, box, ctx, boxes, true);
        else LayoutStack(comp, compId, box, ctx, boxes);
    } else if (type == "img") {
        if (!ctx.childrenMap[compId].empty()) LayoutStack(comp, compId, box, ctx, boxes);
    } else if (type == "circle") {
        std::string dir = comp.value("direction", kEmpty);
        if (dir == "vertical") LayoutFlow(comp, compId, box, ctx, boxes, false);
        else if (dir == "horizontal") LayoutFlow(comp, compId, box, ctx, boxes, true);
        else if (!ctx.childrenMap[compId].empty()) LayoutStack(comp, compId, box, ctx, boxes);
    }
}

// ===== Auto-Fix Layout =====

static std::string WrapText(const std::string& text, uint32_t fontSize, float maxWidth) {
    if (maxWidth <= 0 || text.empty()) return text;
    std::string result;
    size_t lineStart = 0;
    while (lineStart < text.size()) {
        size_t nlPos = text.find('\n', lineStart);
        std::string line = (nlPos != std::string::npos)
            ? text.substr(lineStart, nlPos - lineStart)
            : text.substr(lineStart);
        if (!result.empty()) result += '\n';

        float lineW = CustomV2DslRender::MeasureTextWidth(line, fontSize);
        if (lineW <= maxWidth) {
            result += line;
        } else {
            float curW = 0;
            std::string curLine;
            for (size_t i = 0; i < line.size(); ) {
                unsigned char c = static_cast<unsigned char>(line[i]);
                size_t byteCount = 1;
                if (c >= 128) {
                    if ((c & 0xE0) == 0xC0) byteCount = 2;
                    else if ((c & 0xF0) == 0xE0) byteCount = 3;
                    else if ((c & 0xF8) == 0xF0) byteCount = 4;
                }
                std::string ch = line.substr(i, byteCount);
                float chW = CustomV2DslRender::MeasureTextWidth(ch, fontSize);
                if (!curLine.empty() && curW + chW > maxWidth) {
                    result += curLine + '\n';
                    curLine = ch;
                    curW = chW;
                } else {
                    curLine += ch;
                    curW += chW;
                }
                i += byteCount;
            }
            if (!curLine.empty()) result += curLine;
        }

        if (nlPos != std::string::npos) lineStart = nlPos + 1;
        else break;
    }
    return result;
}

static float MeasureMaxLineWidth(const std::string& text, uint32_t fontSize) {
    float maxW = 0.0f;
    size_t lineStart = 0;
    while (lineStart < text.size()) {
        size_t nlPos = text.find('\n', lineStart);
        std::string line = (nlPos != std::string::npos)
            ? text.substr(lineStart, nlPos - lineStart)
            : text.substr(lineStart);
        maxW = std::max(maxW, CustomV2DslRender::MeasureTextWidth(line, fontSize));
        if (nlPos != std::string::npos) lineStart = nlPos + 1;
        else break;
    }
    return maxW;
}

static float FindExpansionRightBoundary(
    const std::string& compId,
    const LayoutContext& ctx,
    const std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    auto directParentIt = ctx.childToParent.find(compId);
    if (directParentIt != ctx.childToParent.end()) {
        auto parentBoxIt = boxes.find(directParentIt->second);
        if (parentBoxIt != boxes.end()) {
            const auto& parentBox = parentBoxIt->second;
            return parentBox.x + parentBox.width - parentBox.padding[1];
        }
    }

    std::string rootId = compId;
    auto parentIt = ctx.childToParent.find(rootId);
    while (parentIt != ctx.childToParent.end()) {
        rootId = parentIt->second;
        parentIt = ctx.childToParent.find(rootId);
    }
    auto boxIt = boxes.find(rootId);
    if (boxIt == boxes.end()) return std::numeric_limits<float>::max();
    return boxIt->second.x + boxIt->second.width;
}

static bool TryExpandSingleTextStackParent(
    const std::string& parentId,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    if (!ctx.componentMap.count(parentId) || !boxes.count(parentId)) return false;
    auto& parentComp = ctx.componentMap[parentId];
    if (parentComp.value("type", kEmpty) != "rect") return false;
    if (parentComp.contains("direction")) return false;

    const auto& children = ctx.childrenMap[parentId];
    if (children.size() != 1) return false;

    const std::string& textId = children.front();
    if (!ctx.componentMap.count(textId) || !boxes.count(textId)) return false;
    auto& textComp = ctx.componentMap[textId];
    if (textComp.value("type", kEmpty) != "text") return false;

    auto& parentBox = boxes[parentId];
    auto& textBox = boxes[textId];
    float currentInnerW = parentBox.width - parentBox.padding[1] - parentBox.padding[3];
    if (currentInnerW <= 0.0f) return false;
    if (textBox.width <= currentInnerW) return false;

    std::string text = CustomV2DslRender::ResolveContent(textComp, {});
    if (text.empty() || text.find('\n') != std::string::npos) return false;

    uint32_t fontSize = ResolveFontSize(textComp);
    float textW = textBox.width;

    constexpr float kMinSidePadding = 32.0f;
    float targetLeftPad = std::max(parentBox.padding[3], kMinSidePadding);
    float targetRightPad = std::max(parentBox.padding[1], kMinSidePadding);
    float targetW = std::ceil(textW + targetLeftPad + targetRightPad);
    if (targetW <= parentBox.width) return false;

    float rootRight = FindExpansionRightBoundary(parentId, ctx, boxes);
    constexpr float kScreenMargin = 24.0f;
    float maxW = rootRight - parentBox.x - kScreenMargin;
    if (targetW > maxW) return false;

    parentBox.width = targetW;
    parentBox.padding[1] = targetRightPad;
    parentBox.padding[3] = targetLeftPad;
    if (parentComp.contains("size") && parentComp["size"].is_array() && parentComp["size"].size() >= 2) {
        parentComp["size"][0] = targetW;
    }
    parentComp["padding"] = nlohmann::json::array({ parentBox.padding[0], targetRightPad, parentBox.padding[2], targetLeftPad });

    textBox.width = textW;
    textBox.height = MeasureTextHeight(text, fontSize);
    ctx.measureCache.erase(textId);
    LayoutNode(parentId, parentBox, ctx, boxes);
    LOGI("AutoFix: expanded label '%s' width to %.0fpx for single-line text '%s'",
         parentId.c_str(), targetW, textId.c_str());
    return true;
}

static void AutoFixHorizontalFlowTextOverflow(
    const std::string& compId,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    if (!ctx.componentMap.count(compId) || !boxes.count(compId)) return;
    auto& comp = ctx.componentMap[compId];
    if (comp.value("type", kEmpty) != "rect" || comp.value("direction", kEmpty) != "horizontal") return;

    auto& box = boxes[compId];
    const auto& children = ctx.childrenMap[compId];
    if (children.empty()) return;

    float availW = box.width - box.padding[1] - box.padding[3];
    if (availW <= 0.0f) return;

    float gap = comp.value("gap", 0.0f);
    float gapTotal = children.size() > 1 ? gap * static_cast<float>(children.size() - 1) : 0.0f;
    float fixedW = 0.0f;
    float totalTextW = 0.0f;
    std::vector<std::string> textChildren;

    for (const auto& childId : children) {
        if (!ctx.componentMap.count(childId) || !boxes.count(childId)) continue;
        const auto& childComp = ctx.componentMap.at(childId);
        if (childComp.value("type", kEmpty) == "text") {
            textChildren.push_back(childId);
            totalTextW += boxes[childId].width;
        } else {
            fixedW += boxes[childId].width;
        }
    }
    if (textChildren.empty() || totalTextW <= 0.0f) return;

    constexpr float kMinLabelSidePadding = 32.0f;
    float targetLeftPad = std::max(box.padding[3], kMinLabelSidePadding);
    float targetRightPad = std::max(box.padding[1], kMinLabelSidePadding);
    float singleLineRequiredW = fixedW + totalTextW + gapTotal + targetLeftPad + targetRightPad;
    if (singleLineRequiredW > box.width) {
        float rightBoundary = FindExpansionRightBoundary(compId, ctx, boxes);
        constexpr float kBoundaryMargin = 24.0f;
        float maxW = rightBoundary - box.x - kBoundaryMargin;
        if (singleLineRequiredW <= maxW) {
            float targetW = std::ceil(singleLineRequiredW);
            box.width = targetW;
            box.padding[1] = targetRightPad;
            box.padding[3] = targetLeftPad;
            if (comp.contains("size") && comp["size"].is_array() && comp["size"].size() >= 2) {
                comp["size"][0] = targetW;
            }
            comp["padding"] = nlohmann::json::array({ box.padding[0], targetRightPad, box.padding[2], targetLeftPad });
            LayoutNode(compId, box, ctx, boxes);
            LOGI("AutoFix: expanded horizontal label '%s' width to %.0fpx", compId.c_str(), targetW);
            return;
        }
    }

    float textBudget = availW - fixedW - gapTotal;
    if (textBudget <= 0.0f) return;

    float rightBoundary = box.x + box.width - box.padding[1];
    float maxRight = box.x + box.padding[3];
    for (const auto& childId : children) {
        if (!boxes.count(childId)) continue;
        const auto& childBox = boxes[childId];
        maxRight = std::max(maxRight, childBox.x + childBox.width);
    }
    if (maxRight <= rightBoundary) return;

    bool changed = false;
    constexpr uint32_t kMinAutoFontSize = 32;
    for (const auto& childId : textChildren) {
        auto& childComp = ctx.componentMap[childId];
        std::string text = CustomV2DslRender::ResolveContent(childComp, {});
        if (text.empty()) continue;

        uint32_t fontSize = ResolveFontSize(childComp);
        float currentW = boxes[childId].width;
        float targetW = textBudget * (currentW / totalTextW);
        if (targetW <= 0.0f || currentW <= targetW) continue;

        uint32_t newFontSize = static_cast<uint32_t>(std::floor(static_cast<float>(fontSize) * targetW / currentW));
        newFontSize = std::max(kMinAutoFontSize, std::min(fontSize, newFontSize));
        if (newFontSize < fontSize) {
            childComp["fontSize"] = newFontSize;
            ctx.measureCache.erase(childId);
            boxes[childId].width = MeasureMaxLineWidth(text, newFontSize);
            boxes[childId].height = MeasureTextHeight(text, newFontSize);
            changed = true;
            LOGI("AutoFix: shrunk text '%s' font %u -> %u for horizontal overflow in '%s'",
                 childId.c_str(), fontSize, newFontSize, compId.c_str());
        }

        if (boxes[childId].width > targetW) {
            std::string wrapped = WrapText(text, ResolveFontSize(childComp), targetW);
            if (wrapped != text) {
                childComp["content"] = wrapped;
                ctx.measureCache.erase(childId);
                uint32_t wrappedFont = ResolveFontSize(childComp);
                boxes[childId].width = MeasureMaxLineWidth(wrapped, wrappedFont);
                boxes[childId].height = MeasureTextHeight(wrapped, wrappedFont);
                changed = true;
                LOGI("AutoFix: wrapped text '%s' to %.0fpx for horizontal overflow in '%s'",
                     childId.c_str(), targetW, compId.c_str());
            }
        }
    }

    if (changed) {
        LayoutNode(compId, box, ctx, boxes);
    }
}

static void AutoFixOverflow(
    const std::string& compId,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    if (!ctx.componentMap.count(compId) || !boxes.count(compId)) return;
    const auto& comp = ctx.componentMap.at(compId);
    auto& box = boxes[compId];
    std::string type = comp.value("type", kEmpty);
    const auto& children = ctx.childrenMap[compId];
    for (const auto& childId : children) {
        AutoFixOverflow(childId, ctx, boxes);
    }

    if (type == "rect") {
        AutoFixHorizontalFlowTextOverflow(compId, ctx, boxes);
    }
    if (type != "rect") return;

    float innerBottom = box.y + box.height - box.padding[2];
    float maxChildBottom = 0;
    for (const auto& childId : children) {
        if (!boxes.count(childId)) continue;
        const auto& cb = boxes[childId];
        maxChildBottom = std::max(maxChildBottom, cb.y + cb.height);
    }

    if (maxChildBottom > innerBottom) {
        box.height += (maxChildBottom - innerBottom);
        LOGI("AutoFix: expanded '%s' height by %.0f (child overflow bottom)", compId.c_str(), maxChildBottom - innerBottom);
    }
}

static void AutoFixOverlap(
    const std::string& compId,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    if (!ctx.componentMap.count(compId) || !boxes.count(compId)) return;
    const auto& comp = ctx.componentMap.at(compId);
    auto& parentBox = boxes[compId];
    std::string type = comp.value("type", kEmpty);
    if (type != "rect") return;
    const auto& children = ctx.childrenMap[compId];
    for (const auto& childId : children) {
        AutoFixOverlap(childId, ctx, boxes);
    }

    if (children.size() < 2) return;

    std::string dir = comp.value("direction", kEmpty);
    bool allHavePos = false;
    if (dir.empty()) {
        allHavePos = true;
        for (const auto& childId : children) {
            if (!ctx.componentMap.count(childId)) continue;
            const auto& childComp = ctx.componentMap.at(childId);
            if (!childComp.contains("pos") || !childComp["pos"].is_array()) {
                allHavePos = false;
                break;
            }
        }
    }

    struct ChildInfo {
        std::string id;
        float x, y, w, h;
    };
    std::vector<ChildInfo> childInfos;
    childInfos.reserve(children.size());
    for (const auto& childId : children) {
        if (!boxes.count(childId)) continue;
        const auto& cb = boxes[childId];
        childInfos.push_back({childId, cb.x, cb.y, cb.width, cb.height});
    }
    if (childInfos.size() < 2) return;

    std::sort(childInfos.begin(), childInfos.end(),
        [](const ChildInfo& a, const ChildInfo& b) { return a.y < b.y; });

    float parentInnerW = parentBox.width - parentBox.padding[1] - parentBox.padding[3];
    float parentGap = comp.value("gap", 0.0f);
    auto getMinVerticalGap = [&](const std::string& prevId) -> float {
        if (parentGap > 0.0f) return parentGap;
        if (!ctx.componentMap.count(prevId)) return 0.0f;
        const auto& prevComp = ctx.componentMap.at(prevId);
        if (prevComp.value("type", kEmpty) == "text") return 12.0f;
        return 0.0f;
    };

    std::vector<float> origYPos(childInfos.size());
    std::vector<float> origBottomPos(childInfos.size());
    for (size_t k = 0; k < childInfos.size(); k++) {
        origYPos[k] = childInfos[k].y;
        origBottomPos[k] = childInfos[k].y + childInfos[k].h;
    }

    for (size_t i = 1; i < childInfos.size(); i++) {
        auto& child = childInfos[i];
        float maxBottom = child.y;
        for (size_t j = 0; j < i; j++) {
            const auto& prev = childInfos[j];
            if (prev.x + prev.w > child.x && prev.x < child.x + child.w) {
                float minGap = getMinVerticalGap(prev.id);
                maxBottom = std::max(maxBottom, prev.y + prev.h + minGap);
            }
        }
        if (child.y < maxBottom) {
            float pushDown = maxBottom - child.y;
            child.y += pushDown;
            boxes[child.id].y = child.y;
            LOGI("AutoFix: vertical overlap '%s' pushed down %.0f to Y=%.0f",
                 child.id.c_str(), pushDown, child.y);
        }
    }

    for (size_t i = 0; i < childInfos.size(); i++) {
        for (size_t j = i + 1; j < childInfos.size(); j++) {
            if (origYPos[i] == origYPos[j]) {
                float maxY = std::max(childInfos[i].y, childInfos[j].y);
                if (childInfos[i].y < maxY) {
                    childInfos[i].y = maxY;
                    boxes[childInfos[i].id].y = maxY;
                }
                if (childInfos[j].y < maxY) {
                    childInfos[j].y = maxY;
                    boxes[childInfos[j].id].y = maxY;
                }
            }
        }
    }

    for (size_t i = 1; i < childInfos.size(); i++) {
        const auto& prev = childInfos[i - 1];
        const auto& curr = childInfos[i];
        bool xOverlaps = (prev.x + prev.w > curr.x && prev.x < curr.x + curr.w);
        bool sameRow = (origYPos[i] == origYPos[i - 1]);
        if (!xOverlaps || sameRow) continue;

        float origGap = origYPos[i] - origBottomPos[i - 1];
        float minGap = getMinVerticalGap(childInfos[i - 1].id);
        float resolvedGap = std::max(origGap, minGap);
        if (resolvedGap > 0) {
            float prevBottom = childInfos[i - 1].y + childInfos[i - 1].h;
            float targetY = prevBottom + resolvedGap;
            if (childInfos[i].y < targetY) {
                childInfos[i].y = targetY;
                boxes[childInfos[i].id].y = targetY;
            }
        }
    }

    if (allHavePos) return;
    for (size_t i = 0; i < childInfos.size(); i++) {
        for (size_t j = i + 1; j < childInfos.size(); j++) {
            auto& childA = childInfos[i];
            auto& childB = childInfos[j];
            if (childA.y + childA.h <= childB.y || childB.y + childB.h <= childA.y) continue;
            if (childA.x + childA.w > childB.x && childA.x < childB.x + childB.w) {
                ChildInfo* toShrink = (childA.x <= childB.x) ? &childA : &childB;
                float halfW = parentInnerW / 2.0f;
                if (toShrink->w > halfW) {
                    toShrink->w = halfW;
                    boxes[toShrink->id].width = halfW;
                    if (ctx.componentMap.count(toShrink->id)) {
                        const auto& tComp = ctx.componentMap.at(toShrink->id);
                        std::string tType = tComp.value("type", kEmpty);
                        if (tType == "text") {
                            std::string text = CustomV2DslRender::ResolveContent(tComp, {});
                            uint32_t fontSize = ResolveFontSize(tComp);
                            std::string wrapped = WrapText(text, fontSize, halfW - 20);
                            if (wrapped != text) {
                                ctx.componentMap[toShrink->id]["content"] = wrapped;
                                float newH = MeasureTextHeight(wrapped, fontSize);
                                toShrink->h = newH;
                                boxes[toShrink->id].height = newH;
                                LOGI("AutoFix: wrapped text '%s' to %.0fpx width", toShrink->id.c_str(), halfW);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void AutoWrapText(
    const std::string& compId,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    if (!ctx.componentMap.count(compId) || !boxes.count(compId)) return;
    const auto& comp = ctx.componentMap.at(compId);
    std::string type = comp.value("type", kEmpty);

    if (type == "text") {
        // O(1) parent lookup via pre-built childToParent map
        auto parentIt = ctx.childToParent.find(compId);
        if (parentIt == ctx.childToParent.end()) return;
        const std::string& parentId = parentIt->second;
        if (TryExpandSingleTextStackParent(parentId, ctx, boxes)) return;
        if (!boxes.count(parentId)) return;
        auto& pb = boxes[parentId];
        float rightBoundary = pb.x + pb.width - pb.padding[1];

        auto& box = boxes[compId];
        const auto& siblings = ctx.childrenMap[parentId];
        for (const auto& sibId : siblings) {
            if (sibId == compId) continue;
            if (!boxes.count(sibId)) continue;
            const auto& sibBox = boxes[sibId];
            bool verticallyOverlaps = (sibBox.y < box.y + box.height) &&
                                      (sibBox.y + sibBox.height > box.y);
            if (sibBox.x > box.x && verticallyOverlaps) {
                rightBoundary = std::min(rightBoundary, sibBox.x - 10.0f);
            }
        }

        float availW = rightBoundary - box.x;
        if (availW <= 0) return;

        std::string text = CustomV2DslRender::ResolveContent(comp, {});
        uint32_t fontSize = ResolveFontSize(comp);
        float textW = 0;
        size_t ls = 0;
        while (ls < text.size()) {
            size_t nl = text.find('\n', ls);
            std::string line = (nl != std::string::npos) ? text.substr(ls, nl - ls) : text.substr(ls);
            textW = std::max(textW, CustomV2DslRender::MeasureTextWidth(line, fontSize));
            if (nl != std::string::npos) ls = nl + 1; else break;
        }

        if (textW > availW) {
            std::string wrapped = WrapText(text, fontSize, availW - 10);
            if (wrapped != text) {
                ctx.componentMap[compId]["content"] = wrapped;
                box.height = MeasureTextHeight(wrapped, fontSize);
                float newMaxW = 0;
                size_t wls = 0;
                while (wls < wrapped.size()) {
                    size_t wnl = wrapped.find('\n', wls);
                    std::string wline = (wnl != std::string::npos) ? wrapped.substr(wls, wnl - wls) : wrapped.substr(wls);
                    newMaxW = std::max(newMaxW, CustomV2DslRender::MeasureTextWidth(wline, fontSize));
                    if (wnl != std::string::npos) wls = wnl + 1; else break;
                }
                box.width = newMaxW;
                LOGI("AutoFix: auto-wrapped text '%s' to %.0fpx (was %.0fpx, sibling boundary)", compId.c_str(), availW, textW);
            }
        }
        return;
    }

    if (type != "rect" && type != "img" && type != "circle") return;
    const auto& children = ctx.childrenMap[compId];
    for (const auto& childId : children) {
        AutoWrapText(childId, ctx, boxes);
    }
}

static void AdoptOrphanComponents(
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    // Build set of all child IDs from pre-built childrenMap
    std::set<std::string> allChildren;
    for (const auto& [id, ch] : ctx.childrenMap) {
        for (const auto& c : ch) allChildren.insert(c);
    }

    bool adopted = false;
    for (const auto& [id, comp] : ctx.componentMap) {
        if (allChildren.count(id)) continue;
        if (!ctx.childrenMap[id].empty()) continue;
        if (!comp.contains("pos") || !comp["pos"].is_array()) continue;

        glm::vec2 pos = ParseVec2(comp["pos"]);
        std::string bestParent;
        float bestArea = 1e18f;
        for (const auto& [pid, pcomp] : ctx.componentMap) {
            if (pid == id) continue;
            if (pcomp.value("type", kEmpty) != "rect") continue;
            if (!boxes.count(pid)) continue;
            const auto& pb = boxes[pid];
            if (pos.x >= pb.x && pos.x <= pb.x + pb.width &&
                pos.y >= pb.y && pos.y <= pb.y + pb.height) {
                float area = pb.width * pb.height;
                if (area < bestArea) {
                    bestArea = area;
                    bestParent = pid;
                }
            }
        }

        if (!bestParent.empty()) {
            auto& parentComp = ctx.componentMap[bestParent];
            if (!parentComp.contains("children")) {
                parentComp["children"] = nlohmann::json::array();
            }
            parentComp["children"].push_back(id);
            if (!boxes.count(id)) {
                CustomV2DslRender::LayoutBox orphanBox;
                orphanBox.componentId = id;
                orphanBox.componentType = comp.value("type", kEmpty);
                orphanBox.x = pos.x;
                orphanBox.y = pos.y;
                auto sizeArr = comp.find("size");
                if (sizeArr != comp.end() && sizeArr->is_array() && sizeArr->size() >= 2) {
                    auto [wv, wp] = ParseSizeValue((*sizeArr)[0]);
                    auto [hv, hp] = ParseSizeValue((*sizeArr)[1]);
                    orphanBox.width = wv;
                    orphanBox.height = hv;
                }
                boxes[id] = orphanBox;
            }
            LOGI("AutoFix: adopted orphan '%s' into parent '%s'", id.c_str(), bestParent.c_str());
            adopted = true;
        }
    }
    if (adopted) ctx.RebuildChildMaps();
}

static void AutoFixLayout(
    const std::string& rootId,
    LayoutContext& ctx,
    std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes)
{
    LOGI("AutoFixLayout: running auto-fix for root '%s'", rootId.c_str());
    AdoptOrphanComponents(ctx, boxes);
    AutoFixOverflow(rootId, ctx, boxes);
    AutoWrapText(rootId, ctx, boxes);
    AutoFixOverlap(rootId, ctx, boxes);
    AutoFixOverflow(rootId, ctx, boxes);
}

// ===== Convert to DslRenderCommand =====

static void ConvertComponent(
    const std::string& compId,
    LayoutContext& ctx,
    const std::unordered_map<std::string, CustomV2DslRender::LayoutBox>& boxes,
    const nlohmann::json& dataModel,
    std::vector<DslRenderCommand>& commands)
{
    if (!ctx.componentMap.count(compId) || !boxes.count(compId)) return;
    const auto& comp = ctx.componentMap.at(compId);
    const auto& box = boxes.at(compId);
    std::string type = comp.value("type", kEmpty);

    if (type == "rect") {
        if (comp.contains("color")) {
            std::string colorStr = comp.value("color", std::string("#FFFFFF"));
            float radius = comp.value("radius", 0.0f);

            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Rect;
            cmd.pos = glm::vec2(box.x, box.y);
            cmd.size = glm::vec2(box.width, box.height);
            cmd.color = CustomV2DslRender::ParseColor(colorStr);
            cmd.radius = radius;
            cmd.glass = comp.value("glass", false);
            cmd.componentId = compId;
            if (comp.contains("action") && comp["action"].is_string())
                cmd.action = comp["action"].get<std::string>();
            commands.push_back(cmd);
        }
        const auto& children = ctx.childrenMap[compId];
        for (const auto& childId : children) {
            ConvertComponent(childId, ctx, boxes, dataModel, commands);
        }
    } else if (type == "text") {
        std::string text = CustomV2DslRender::ResolveContent(comp, dataModel);
        if (text.empty()) return;

        uint32_t fontSize = ResolveFontSize(comp);
        std::string colorStr = comp.value("color", std::string("#000000"));
        glm::vec4 color = CustomV2DslRender::ParseColor(colorStr);

        // streamText: keep full text as one command (line wrapping handled by renderer)
        if (comp.contains("streamText")) {
            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Text;
            cmd.text = text;
            cmd.pos = glm::vec2(box.x, box.y);
            cmd.fontSize = fontSize;
            cmd.color = color;
            cmd.glowWidth = comp.value("glowWidth", 0.0f);
            cmd.glowIntensity = comp.value("glowIntensity", 0.0f);
            cmd.componentId = compId;
            cmd.streamText = true;
            if (comp["streamText"].is_object()) {
                cmd.streamTextSpeed = comp["streamText"].value("speed", 0.0f);
                cmd.streamTextMaxWidth = comp["streamText"].value("maxWidth", 0.0f);
                cmd.markdown = comp["streamText"].value("markdown", false);
            } else if (comp["streamText"].is_number()) {
                cmd.streamTextSpeed = comp["streamText"].get<float>();
            }
            commands.push_back(cmd);
        } else {
            // Normal text: split by newlines into separate line commands
            std::string textAlign = comp.value("textAlign", std::string("start"));
            std::string verticalAlign = comp.value("verticalAlign", std::string("start"));
            float textY = box.y;
            float totalH = MeasureTextHeight(text, fontSize);
            if (verticalAlign == "center" || verticalAlign == "middle") textY = box.y + (box.height - totalH) / 2.0f;
            else if (verticalAlign == "end" || verticalAlign == "bottom") textY = box.y + box.height - totalH;
            float lineY = textY;
            float lineHeight = fontSize * 1.3f;
            size_t pos = 0;
            while (true) {
                size_t nlPos = text.find('\n', pos);
                std::string line = (nlPos != std::string::npos)
                    ? text.substr(pos, nlPos - pos)
                    : text.substr(pos);
                if (!line.empty()) {
                    float lineW = CustomV2DslRender::MeasureTextWidth(line, fontSize);
                    float thisTextX = box.x;
                    if (textAlign == "center") {
                        thisTextX = box.x + (box.width - lineW) / 2.0f;
                    } else if (textAlign == "end" || textAlign == "right") {
                        thisTextX = box.x + box.width - lineW;
                    }
                    DslRenderCommand cmd;
                    cmd.type = DslRenderCommand::Type::Text;
                    cmd.text = line;
                    cmd.pos = glm::vec2(thisTextX, lineY);
                    cmd.fontSize = fontSize;
                    cmd.color = color;
                    cmd.glowWidth = comp.value("glowWidth", 0.0f);
                    cmd.glowIntensity = comp.value("glowIntensity", 0.0f);
                    cmd.componentId = compId;
                    if (comp.contains("shadow")) {
                        float autoOffset = std::clamp(static_cast<float>(fontSize) * 0.055f, 3.0f, 8.0f);
                        glm::vec2 shadowOffset(autoOffset, autoOffset);
                        std::string shadowColorStr = "#5A3A2B";
                        bool enableShadow = true;
                        const auto& shadow = comp["shadow"];
                        if (shadow.is_boolean()) {
                            enableShadow = shadow.get<bool>();
                            if (enableShadow) {
                                float luminance = color.r * 0.299f + color.g * 0.587f + color.b * 0.114f;
                                if (luminance < 0.45f) {
                                    // Dark text on a light background is already legible; a dark offset copy reads as ghosting.
                                    enableShadow = false;
                                }
                            }
                        } else if (shadow.is_string()) {
                            shadowColorStr = shadow.get<std::string>();
                        } else if (shadow.is_array() && shadow.size() >= 2) {
                            shadowOffset = glm::vec2(shadow[0].get<float>(), shadow[1].get<float>());
                            if (shadow.size() >= 3 && shadow[2].is_string()) shadowColorStr = shadow[2].get<std::string>();
                        } else if (shadow.is_object()) {
                            shadowOffset = glm::vec2(shadow.value("dx", autoOffset), shadow.value("dy", autoOffset));
                            shadowColorStr = shadow.value("color", shadowColorStr);
                        }
                        if (enableShadow) {
                            DslRenderCommand shadowCmd = cmd;
                            shadowCmd.pos += shadowOffset;
                            shadowCmd.color = CustomV2DslRender::ParseColor(shadowColorStr);
                            shadowCmd.glowWidth = 0.0f;
                            shadowCmd.glowIntensity = 0.0f;
                            commands.push_back(shadowCmd);
                        }
                    }
                    commands.push_back(cmd);
                }
                if (nlPos == std::string::npos) break;
                lineY += lineHeight;
                pos = nlPos + 1;
            }
        }
    } else if (type == "img") {
        std::string src = comp.value("src", std::string(""));
        if (src.empty() && comp.contains("dataPath"))
            src = CustomV2DslRender::ResolveDataPath(dataModel, comp["dataPath"].get<std::string>());
        if (src.empty()) return;

        DslRenderCommand cmd;
        cmd.type = DslRenderCommand::Type::Image;
        cmd.imagePath = src; cmd.pos = glm::vec2(box.x, box.y);
        cmd.size = glm::vec2(box.width, box.height);
        cmd.color = glm::vec4(1, 1, 1, 1);
        commands.push_back(cmd);
        const auto& children = ctx.childrenMap[compId];
        for (const auto& childId : children) {
            ConvertComponent(childId, ctx, boxes, dataModel, commands);
        }
    } else if (type == "circle") {
        if (comp.contains("color")) {
            float radius = comp.value("radius", 50.0f);
            std::string colorStr = comp.value("color", std::string("#FFFFFF"));
            glm::vec4 parsedColor = CustomV2DslRender::ParseColor(colorStr);
            glm::vec2 circleCenter = glm::vec2(box.x + radius, box.y + radius);

            DslRenderCommand cmd;
            cmd.type = DslRenderCommand::Type::Circle;
            cmd.pos = circleCenter;
            cmd.circleRadius = radius;
            cmd.color = parsedColor;
            cmd.componentId = compId;
            if (comp.contains("action") && comp["action"].is_string())
                cmd.action = comp["action"].get<std::string>();
            commands.push_back(cmd);
        }
        const auto& children = ctx.childrenMap[compId];
        for (const auto& childId : children) {
            ConvertComponent(childId, ctx, boxes, dataModel, commands);
        }
    }
}

// ===== CanParse =====

bool CustomV2DslRender::CanParse(const nlohmann::json& root) const {
    if (!root.is_object()) return false;
    if (!root.contains("updateComponents")) return false;
    auto& uc = root["updateComponents"];
    if (!uc.is_object() || !uc.contains("components") || !uc["components"].is_array()) return false;
    if (uc["components"].empty()) return false;

    bool hasStandardType = false;
    for (const auto& comp : uc["components"]) {
        if (!comp.is_object()) continue;
        if (comp.contains("component")) return false; // A2UI
        if (!comp.contains("type")) continue;
        std::string t = comp["type"].get<std::string>();
        if (t == "target" || t == "crosshair") return false;
        if (t == "rect" || t == "text" || t == "img" || t == "circle" || t == "icon") hasStandardType = true;
    }
    return hasStandardType;
}

// ===== Parse =====

std::vector<DslRenderCommand> CustomV2DslRender::Parse(
    const nlohmann::json& root,
    const ParseContext& ctx
) const {
    std::vector<DslRenderCommand> commands;

    nlohmann::json updateComp;
    if (root.contains("updateComponents") && root["updateComponents"].is_object())
        updateComp = root["updateComponents"];
    else { LOGE("CustomV2DslRender: No updateComponents found"); return {}; }

    nlohmann::json dataModel = ctx.dataModel;
    if (root.contains("updateDataModel") && root["updateDataModel"].is_object()) {
        auto& udm = root["updateDataModel"];
        if (udm.contains("value")) {
            if (udm.contains("path") && udm["path"].is_string()) {
                std::string basePath = udm["path"].get<std::string>();
                nlohmann::json nested = udm["value"];
                std::vector<std::string> segments;
                std::istringstream pss(basePath);
                std::string seg;
                while (std::getline(pss, seg, '/')) {
                    if (!seg.empty()) segments.push_back(seg);
                }
                for (int i = static_cast<int>(segments.size()) - 1; i >= 0; i--) {
                    nlohmann::json wrapper;
                    wrapper[segments[i]] = nested;
                    nested = wrapper;
                }
                dataModel = nested;
            } else {
                dataModel = udm["value"];
            }
            LOGI("CustomV2DslRender: Loaded data model");
        }
    }

    std::unordered_map<std::string, nlohmann::json> componentMap;
    std::vector<std::string> compOrder;
    if (updateComp.contains("components") && updateComp["components"].is_array()) {
        for (const auto& comp : updateComp["components"]) {
            if (!comp.is_object() || !comp.contains("id")) continue;
            std::string id = JsonIdToString(comp["id"]);
            if (id.empty()) continue;
            componentMap[id] = comp;
            compOrder.push_back(id);
        }
    }
    if (componentMap.empty()) { LOGE("CustomV2DslRender: No components found"); return {}; }

    // Process children: "total" before LayoutContext construction
    // First pass: collect all IDs that are already declared as children somewhere
    std::set<std::string> alreadyChildren;
    for (const auto& [id, comp] : componentMap) {
        if (comp.contains("children") && comp["children"].is_array()) {
            for (const auto& c : comp["children"]) {
                std::string cid = JsonIdToString(c);
                if (!cid.empty()) alreadyChildren.insert(cid);
            }
        }
    }

    // Second pass: expand "total" to include only orphan components
    for (size_t i = 0; i < compOrder.size(); i++) {
        auto& comp = componentMap[compOrder[i]];
        if (comp.contains("children") &&
            comp["children"].is_string() &&
            comp["children"].get<std::string>() == "total") {
            nlohmann::json newChildren = nlohmann::json::array();
            for (size_t j = i + 1; j < compOrder.size(); j++) {
                // Only add if not already a child of some other component
                if (!alreadyChildren.count(compOrder[j])) {
                    newChildren.push_back(compOrder[j]);
                }
            }
            comp["children"] = newChildren;
            LOGI("CustomV2DslRender: children:'total' expanded '%s' -> %zu children (excluding %zu already-owned)",
                 compOrder[i].c_str(), newChildren.size(), alreadyChildren.size());
        }
    }

    // Pre-resolve all text contents so layout/auto-fix and rendering use the same text.
    for (auto& p : componentMap) {
        auto& compJson = p.second;
        if (compJson.value("type", kEmpty) == "text") {
            std::string resolved = CustomV2DslRender::ResolveContent(compJson, dataModel);
            compJson["content"] = resolved;
            // After resolving dataPath for this parse, content becomes the source of truth.
            // Auto-fix may wrap/shrink content; keeping dataPath would overwrite that during ConvertComponent.
            compJson.erase("dataPath");
        }
    }

    // Pre-convert icon → img: validate src against known healing icons, fallback to sparkles.png
    static const std::set<std::string> kHealingIcons = {
        "healing/lotus.png", "healing/leaf.png", "healing/feather.png",
        "healing/sparkles.png", "healing/sunrise.png", "healing/moon.png",
        "healing/heart.png", "healing/waves.png", "healing/refresh.png",
        "healing/bookmark.png"
    };
    static const std::string kIconFallback = "healing/sparkles.png";
    for (auto& p : componentMap) {
        auto& compJson = p.second;
        if (compJson.value("type", kEmpty) != "icon") continue;
        compJson["type"] = "img";
        std::string src = compJson.value("src", std::string(""));
        if (src.empty() || !kHealingIcons.count(src)) {
            if (!src.empty())
                LOGI("Icon fallback: '%s' not in healing icon list, replacing with '%s'", src.c_str(), kIconFallback.c_str());
            compJson["src"] = kIconFallback;
        }
    }

    // Build layout context (childrenMap + childToParent + measureCache, built once)
    LayoutContext layoutCtx(componentMap);

    // Find top-level IDs using childToParent (O(n) instead of scanning all children)
    std::vector<std::string> topLevelIds;
    for (const auto& comp : updateComp["components"]) {
        if (!comp.is_object() || !comp.contains("id")) continue;
        std::string id = JsonIdToString(comp["id"]);
        if (id.empty()) continue;
        if (!layoutCtx.childToParent.count(id)) {
            topLevelIds.push_back(id);
        }
    }
    if (topLevelIds.empty() && !compOrder.empty()) {
        topLevelIds.push_back(compOrder[0]);
    }

    float designWidth = ctx.designWidth, designHeight = ctx.designHeight;
    std::unordered_map<std::string, LayoutBox> boxes;

    // Phase 1: Layout + Auto-fix
    for (const auto& rootId : topLevelIds) {
        const auto& rootComp = componentMap[rootId];

        glm::vec2 rootPos(0, 0);
        auto posIt = rootComp.find("pos");
        if (posIt != rootComp.end() && posIt->is_array()) rootPos = ParseVec2(*posIt);

        float rootW = 0, rootH = 0;
        std::string rootType = rootComp.value("type", std::string("rect"));
        
        if (rootType == "circle") {
            float radius = rootComp.value("radius", 50.0f);
            rootW = rootH = radius * 2.0f;
        } else {
            auto sizeArr = rootComp.find("size");
            if (sizeArr != rootComp.end() && sizeArr->is_array() && sizeArr->size() >= 2) {
                auto [wv, wp] = ParseSizeValue((*sizeArr)[0]);
                auto [hv, hp] = ParseSizeValue((*sizeArr)[1]);
                if (!wp && wv > 0) rootW = wv;
                if (!hp && hv > 0) rootH = hv;
            }
        }

        LOGI("CustomV2DslRender: Design %.0fx%.0f, top-level '%s' pos=[%.0f,%.0f] size=%.0fx%.0f",
             designWidth, designHeight, rootId.c_str(), rootPos.x, rootPos.y, rootW, rootH);

        LayoutBox rootBox;
        rootBox.componentId = rootId;
        rootBox.componentType = rootComp.value("type", std::string("rect"));
        rootBox.width = rootW; rootBox.height = rootH;
        rootBox.x = rootPos.x; rootBox.y = rootPos.y;
        ParsePadding(rootComp, rootBox.padding);
        boxes[rootId] = rootBox;
        LayoutNode(rootId, rootBox, layoutCtx, boxes);

        if (ctx.autoFix) {
            AutoFixLayout(rootId, layoutCtx, boxes);
        }
    }

    // Phase 2: Recompute top-level IDs after auto-fix (orphans may have been adopted)
    std::vector<std::string> finalTopLevelIds;
    for (const auto& id : topLevelIds) {
        if (!layoutCtx.childToParent.count(id)) {
            finalTopLevelIds.push_back(id);
        }
    }

    // Phase 3: Convert only truly top-level components
    for (const auto& rootId : finalTopLevelIds) {
        ConvertComponent(rootId, layoutCtx, boxes, dataModel, commands);
    }

    // Phase 4: Auto-scale (behind CMake toggle, default OFF)
    // Maps JSON coordinate space to screen pixels when they differ.
    // Enable via cmake: -DENABLE_DSL_AUTO_SCALE=ON
#ifdef DSL_AUTO_SCALE
    float canvasMaxX = 0.0f, canvasMaxY = 0.0f;
    for (const auto& cmd : commands) {
        if (cmd.type == DslRenderCommand::Type::Rect || cmd.type == DslRenderCommand::Type::Image) {
            canvasMaxX = std::max(canvasMaxX, cmd.pos.x + cmd.size.x);
            canvasMaxY = std::max(canvasMaxY, cmd.pos.y + cmd.size.y);
        } else if (cmd.type == DslRenderCommand::Type::Circle) {
            canvasMaxX = std::max(canvasMaxX, cmd.pos.x + cmd.circleRadius);
            canvasMaxY = std::max(canvasMaxY, cmd.pos.y + cmd.circleRadius);
        }
    }
    float scaleX = 1.0f, scaleY = 1.0f;
    if (canvasMaxX > 0 && canvasMaxY > 0) {
        bool hasFullScreenBg = false;
        for (const auto& cmd : commands) {
            if (cmd.type == DslRenderCommand::Type::Rect || cmd.type == DslRenderCommand::Type::Image) {
                if (cmd.pos.x < 10.0f && cmd.pos.y < 10.0f &&
                    cmd.size.x >= canvasMaxX * 0.9f && cmd.size.y >= canvasMaxY * 0.9f) {
                    hasFullScreenBg = true;
                    break;
                }
            } else if (cmd.type == DslRenderCommand::Type::Circle) {
                if (cmd.pos.x - cmd.circleRadius < 10.0f && cmd.pos.y - cmd.circleRadius < 10.0f &&
                    cmd.circleRadius * 2.0f >= canvasMaxX * 0.9f && cmd.circleRadius * 2.0f >= canvasMaxY * 0.9f) {
                    hasFullScreenBg = true;
                    break;
                }
            }
        }
        if (hasFullScreenBg) {
            scaleX = designWidth / canvasMaxX;
            scaleY = designHeight / canvasMaxY;
        }
    }
    if (std::fabs(scaleX - 1.0f) > 0.001f || std::fabs(scaleY - 1.0f) > 0.001f) {
        float sMin = std::min(scaleX, scaleY);
        for (auto& cmd : commands) {
            cmd.pos.x *= scaleX; cmd.pos.y *= scaleY;
            cmd.size.x *= scaleX; cmd.size.y *= scaleY;
            cmd.fontSize = static_cast<uint32_t>(cmd.fontSize * sMin);
            cmd.radius *= sMin;
            cmd.circleRadius *= sMin;
        }
        LOGI("CustomV2DslRender: SCALED scaleX=%.4f scaleY=%.4f", scaleX, scaleY);
    }
#endif

    return commands;
}

// Self-registration (before A2UI so CanParse order is correct)
static bool g_registered = [](){
    application::dsl::DslRenderRegistry::Register(std::make_unique<application::dsl::CustomV2DslRender>());
    return true;
}();

} // namespace application::dsl
