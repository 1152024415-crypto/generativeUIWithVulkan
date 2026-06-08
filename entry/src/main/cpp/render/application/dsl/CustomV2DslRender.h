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

#ifndef CUSTOM_V2_DSL_RENDER_H
#define CUSTOM_V2_DSL_RENDER_H

#include "dsl/IDslRender.h"
#include <unordered_map>
#include <string>

namespace application::dsl {

/**
 * Renderer for the Custom DSL v2 format.
 *
 * Only three component types: Rect, Text, Image.
 * - Rect with "direction" field acts as flow container (vertical/horizontal)
 * - Rect without "direction" acts as stack (z-order by children array index)
 * - Components with "pos" are absolutely positioned and exit the flow
 * - Text has NO size field — auto-measures from content
 * - Size supports: absolute pixels [w,h] or "100%"
 */
class CustomV2DslRender : public IDslRender {
public:
    std::string GetFormatName() const override { return "CustomV2"; }
    bool CanParse(const nlohmann::json& root) const override;
    std::vector<DslRenderCommand> Parse(
        const nlohmann::json& root,
        const ParseContext& ctx = {}
    ) const override;

    struct LayoutBox {
        std::string componentId;
        std::string componentType;
        float x = 0, y = 0, width = 0, height = 0;
        float padding[4] = {0}; // top, right, bottom, left
    };

    static glm::vec4 ParseColor(const std::string& hexColor);
    static float MeasureTextWidth(const std::string& text, uint32_t fontSize);
    static std::string ResolveDataPath(const nlohmann::json& dataModel, const std::string& path);
    static std::string ResolveContent(const nlohmann::json& comp, const nlohmann::json& dataModel);
};

} // namespace application::dsl

#endif // CUSTOM_V2_DSL_RENDER_H
