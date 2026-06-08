#include "dsl/GameDslRender.h"
#include "dsl/DslRenderRegistry.h"
#include "logger_common.h"
#include "agenui_engine/thirdparty/glm/glm.hpp"
#include <algorithm>
#include <cmath>

namespace application::dsl {

static glm::vec4 hexToVec4(const std::string& hex) {
    glm::vec4 result(1.0f, 1.0f, 1.0f, 1.0f);
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.length() >= 6) {
        result.r = std::stoul(h.substr(0, 2), nullptr, 16) / 255.0f;
        result.g = std::stoul(h.substr(2, 2), nullptr, 16) / 255.0f;
        result.b = std::stoul(h.substr(4, 2), nullptr, 16) / 255.0f;
    }
    return result;
}

static bool isYellowish(const glm::vec4& color) {
    return color.r > 0.65f && color.g > 0.45f && color.b < 0.25f;
}

static bool isDarkRed(const glm::vec4& color) {
    return color.r > 0.25f && color.g < 0.12f && color.b < 0.12f;
}

static glm::vec4 targetInnerRingColor(const glm::vec4& bullseyeColor) {
    return glm::vec4(
        std::min(1.0f, bullseyeColor.r + 0.08f),
        std::min(1.0f, bullseyeColor.g + 0.05f),
        std::min(1.0f, bullseyeColor.b + 0.05f),
        bullseyeColor.a);
}

static void appendRingCommands(std::vector<DslRenderCommand>& commands,
                               float cx, float cy,
                               float outerRadius,
                               float thickness,
                               const glm::vec4& color) {
    const int segments = 32;
    const float innerRadius = std::max(1.0f, outerRadius - thickness);
    std::vector<glm::vec2> outer;
    std::vector<glm::vec2> inner;
    outer.reserve(segments);
    inner.reserve(segments);

    for (int i = 0; i < segments; i++) {
        float angle = (static_cast<float>(i) / segments) * 3.14159265f * 2.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        outer.push_back(glm::vec2(cx + c * outerRadius, cy + s * outerRadius));
        inner.push_back(glm::vec2(cx + c * innerRadius, cy + s * innerRadius));
    }

    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        DslRenderCommand triA;
        triA.type = DslRenderCommand::Type::Polygon;
        triA.polygonCenter = outer[i];
        triA.polygonVertices = { outer[i], outer[next], inner[i] };
        triA.color = color;
        commands.push_back(triA);

        DslRenderCommand triB;
        triB.type = DslRenderCommand::Type::Polygon;
        triB.polygonCenter = inner[i];
        triB.polygonVertices = { inner[i], outer[next], inner[next] };
        triB.color = color;
        commands.push_back(triB);
    }
}

// ===== CanParse =====

bool GameDslRender::CanParse(const nlohmann::json& root) const {
    if (!root.is_object()) return false;
    if (!root.contains("updateComponents")) return false;
    auto& uc = root["updateComponents"];
    if (!uc.is_object() || !uc.contains("components") || !uc["components"].is_array()) return false;
    for (const auto& comp : uc["components"]) {
        if (!comp.is_object()) continue;
        std::string t = comp.value("type", std::string(""));
        if (t == "target" || t == "crosshair") return true;
    }
    return false;
}

// ===== Parse =====

std::vector<DslRenderCommand> GameDslRender::Parse(
    const nlohmann::json& root,
    const ParseContext& ctx
) const {
    std::vector<DslRenderCommand> commands;

    nlohmann::json standardComponents = nlohmann::json::array();
    nlohmann::json targetComp, crosshairComp;
    bool hasTarget = false, hasCrosshair = false;

    if (root.contains("updateComponents") && root["updateComponents"].contains("components")) {
        for (const auto& comp : root["updateComponents"]["components"]) {
            std::string type = comp.value("type", std::string(""));
            if (type == "target") {
                targetComp = comp;
                hasTarget = true;
            } else if (type == "crosshair") {
                crosshairComp = comp;
                hasCrosshair = true;
            } else {
                standardComponents.push_back(comp);
            }
        }
    }

    // Delegate standard components to parent
    if (!standardComponents.empty()) {
        nlohmann::json standardJson;
        standardJson["updateComponents"]["components"] = standardComponents;
        if (root.contains("updateDataModel")) {
            standardJson["updateDataModel"] = root["updateDataModel"];
        }
        auto standardCmds = CustomV2DslRender::Parse(standardJson, ctx);
        commands.insert(commands.end(), standardCmds.begin(), standardCmds.end());
    }

    // For game mode (mode:"game"), only return standard commands (background).
    // Dynamic game elements are handled by GameModule::RenderFrame.
    bool isGameMode = root.contains("mode") && root["mode"] == "game";
    if (isGameMode) {
        return commands;
    }

    // Preview mode: generate static target visualization
    if (hasTarget) {
        int targetCount = targetComp.value("targetCount", 3);
        int rings = targetComp.value("rings", 4);
        float bullseyeSize = targetComp.value("bullseyeSize", 0.14f);
        // Proportional to design height — matches in-game target scaling
        float radius = ctx.designHeight > 0.0f ? ctx.designHeight * 0.12f : 80.0f;

        // Default colors
        static const float defaultColors[5][3] = {
            {0.9f, 0.9f, 0.9f}, {0.2f, 0.4f, 0.8f},
            {0.9f, 0.9f, 0.9f}, {0.2f, 0.4f, 0.8f}, {0.9f, 0.9f, 0.9f}
        };
        std::vector<glm::vec4> ringColors;
        if (targetComp.contains("colors") && targetComp["colors"].is_array()) {
            for (auto& c : targetComp["colors"]) {
                ringColors.push_back(hexToVec4(c.get<std::string>()));
            }
        }
        if (ringColors.empty()) {
            for (int j = 0; j < 5; j++) {
                ringColors.push_back(glm::vec4(defaultColors[j][0], defaultColors[j][1], defaultColors[j][2], 1.0f));
            }
        }

        glm::vec4 bullseyeColor(0.8f, 0.0f, 0.0f, 1.0f);
        if (targetComp.contains("bullseyeColor")) {
            bullseyeColor = hexToVec4(targetComp["bullseyeColor"].get<std::string>());
        }
        int visibleRingCount = std::max(0, rings - 1);
        int innerRingIndex = std::min(visibleRingCount, static_cast<int>(ringColors.size())) - 1;
        if (innerRingIndex >= 0 &&
            (isYellowish(ringColors[innerRingIndex]) || isDarkRed(ringColors[innerRingIndex]))) {
            ringColors[innerRingIndex] = targetInnerRingColor(bullseyeColor);
        }

        // Evenly distribute targets across design space
        float dw = ctx.designWidth;
        float dh = ctx.designHeight;
        int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(targetCount))));
        int rows = static_cast<int>(std::ceil(static_cast<float>(targetCount) / cols));
        float spacingX = dw / (cols + 1);
        float spacingY = dh / (rows + 1);

        int idx = 0;
        for (int r = 0; r < rows && idx < targetCount; r++) {
            for (int c = 0; c < cols && idx < targetCount; c++) {
                float cx = spacingX * (c + 1);
                float cy = spacingY * (r + 1);

                // Draw outer circles only; the smallest visible circle is the bullseye.
                float bullR = radius * bullseyeSize;
                float ringBand = visibleRingCount > 0
                    ? (radius - bullR) / static_cast<float>(visibleRingCount)
                    : 0.0f;
                for (int i = 0; i < visibleRingCount && !ringColors.empty(); i++) {
                    float ringRadius = bullR + ringBand * static_cast<float>(visibleRingCount - i);
                    auto& rc = ringColors[i % std::min(2, static_cast<int>(ringColors.size()))];

                    DslRenderCommand cmd;
                    cmd.type = DslRenderCommand::Type::Circle;
                    cmd.pos = glm::vec2(cx, cy);
                    cmd.color = glm::vec4(rc.r, rc.g, rc.b, 1.0f);
                    cmd.circleRadius = ringRadius;
                    commands.push_back(cmd);
                }

                // Bullseye center
                DslRenderCommand bull;
                bull.type = DslRenderCommand::Type::Circle;
                bull.pos = glm::vec2(cx, cy);
                bull.color = bullseyeColor;
                bull.circleRadius = bullR;
                commands.push_back(bull);

                idx++;
            }
        }
    }

    // Generate static crosshair visualization
    if (hasCrosshair) {
        float dw = ctx.designWidth;
        float dh = ctx.designHeight;
        float cx = dw / 2.0f;
        float cy = dh / 2.0f;
        float cs = std::max(crosshairComp.value("size", 48.0f), dh * 0.038f);
        float thick = std::max(6.0f, cs * 0.13f);

        glm::vec4 chColor(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 centerColor = chColor;

        float ringOuterRadius = cs * 0.82f;
        float ringThickness = std::max(4.0f, thick * 0.65f);
        appendRingCommands(commands, cx, cy, ringOuterRadius, ringThickness, chColor);

        // Horizontal bar
        DslRenderCommand hBar;
        hBar.type = DslRenderCommand::Type::Rect;
        hBar.pos = glm::vec2(cx - cs, cy - thick * 0.5f);
        hBar.size = glm::vec2(cs * 2, thick);
        hBar.color = chColor;
        commands.push_back(hBar);

        // Vertical bar
        DslRenderCommand vBar;
        vBar.type = DslRenderCommand::Type::Rect;
        vBar.pos = glm::vec2(cx - thick * 0.5f, cy - cs);
        vBar.size = glm::vec2(thick, cs * 2);
        vBar.color = chColor;
        commands.push_back(vBar);

        // Center dot
        float dotR = std::max(6.0f, thick * 0.95f);
        DslRenderCommand dot;
        dot.type = DslRenderCommand::Type::Rect;
        dot.pos = glm::vec2(cx - dotR, cy - dotR);
        dot.size = glm::vec2(dotR * 2, dotR * 2);
        dot.color = centerColor;
        dot.radius = dotR;
        commands.push_back(dot);
    }

    return commands;
}

// ===== ExtractGameConfig =====

GameDslRender::GameConfig GameDslRender::ExtractGameConfig(const nlohmann::json& root) {
    GameConfig config;
    try {
        if (!root.is_object()) return config;

        if (root.contains("updateComponents") && root["updateComponents"].contains("components")) {
            for (const auto& comp : root["updateComponents"]["components"]) {
                std::string type = comp.value("type", std::string(""));
                if (type == "target") config.targetConfig = comp;
                else if (type == "crosshair") config.crosshairConfig = comp;
            }
        }
        if (root.contains("updateDataModel") && root["updateDataModel"].contains("duration")) {
            config.duration = root["updateDataModel"]["duration"].get<float>();
        }
    } catch (...) {}
    return config;
}

// Self-registration
static bool g_registered = [](){
    DslRenderRegistry::Register(std::make_unique<GameDslRender>());
    return true;
}();

} // namespace application::dsl
