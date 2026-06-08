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

#ifndef CUSTOM_DSL_RENDER_H
#define CUSTOM_DSL_RENDER_H

#include "dsl/IDslRender.h"

namespace application::dsl {

/**
 * Renderer for the custom DSL format.
 * Format: JSON array of objects with "type" field.
 * Example: [{"type":"rect","pos":[60,918],"size":[1156,500],"Color":"#90CAF9","radius":30}]
 */
class CustomDslRender : public IDslRender {
public:
    std::string GetFormatName() const override { return "CustomDSL"; }
    bool CanParse(const nlohmann::json& root) const override;
    std::vector<DslRenderCommand> Parse(
        const nlohmann::json& root,
        const ParseContext& ctx = {}
    ) const override;
};

} // namespace application::dsl

#endif // CUSTOM_DSL_RENDER_H
