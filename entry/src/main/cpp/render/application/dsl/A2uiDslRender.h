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

#ifndef A2UI_DSL_RENDER_H
#define A2UI_DSL_RENDER_H

#include "dsl/IDslRender.h"
#include <unordered_map>
#include <string>
#include <sstream>

namespace application {
namespace dsl {

/**
 * Renderer for A2UI v0.9 format.
 * Uses Facebook Yoga for Flexbox layout calculation.
 */
class A2uiDslRender : public IDslRender {
public:
    std::string GetFormatName() const override { return "A2UI"; }
    bool CanParse(const nlohmann::json& root) const override;
    std::vector<DslRenderCommand> Parse(
        const nlohmann::json& root,
        const ParseContext& ctx = {}
    ) const override;

    struct LayoutBox {
        std::string componentId;
        std::string componentType;
        float x = 0, y = 0, width = 0, height = 0;
        float padding[4] = {0};  // top, right, bottom, left
    };

    // Public helpers used by measure function and ConvertComponent
    static std::string GetStyleString(const nlohmann::json& comp, const std::string& key, const std::string& defaultVal = "");
    static float GetStylePx(const nlohmann::json& comp, const std::string& key, float defaultVal);
    static uint32_t GetFontSizeFromVariant(const std::string& variant);
    static float MeasureTextWidth(const std::string& text, uint32_t fontSize);
    static std::string ResolveDynamicString(const nlohmann::json& value, const nlohmann::json& dataModel);
    static nlohmann::json ResolvePath(const nlohmann::json& root, const std::string& path);

private:
    // Build Yoga node tree, run layout, collect results into boxes
    void BuildYogaTree(
        const std::string& rootId,
        const std::unordered_map<std::string, nlohmann::json>& componentMap,
        float designWidth, float designHeight,
        const nlohmann::json& dataModel,
        std::unordered_map<std::string, LayoutBox>& boxes) const;

    // Traverse component tree → DslRenderCommand list
    bool ConvertComponent(const nlohmann::json& comp, const LayoutBox& box,
                          const std::unordered_map<std::string, nlohmann::json>& componentMap,
                          const std::unordered_map<std::string, LayoutBox>& boxes,
                          const nlohmann::json& dataModel,
                          std::vector<DslRenderCommand>& commands,
                          float containerWidth, float containerHeight,
                          float layoutScale = 1.0f) const;

    static glm::vec4 ParseColor(const std::string& hexColor);
    static glm::vec3 ParseColorRGB(const std::string& hexColor);
};

} // namespace dsl
} // namespace application

#endif // A2UI_DSL_RENDER_H
