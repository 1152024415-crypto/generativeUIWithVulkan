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

#include "dsl/DslRenderRegistry.h"
#include "logger_common.h"

namespace application::dsl {

std::vector<std::unique_ptr<IDslRender>>& DslRenderRegistry::GetRenderers() {
    static std::vector<std::unique_ptr<IDslRender>> renderers;
    return renderers;
}

void DslRenderRegistry::Register(std::unique_ptr<IDslRender> renderer) {
    LOGI("DslRenderRegistry: Registering renderer '%s'", renderer->GetFormatName().c_str());
    GetRenderers().push_back(std::move(renderer));
}

std::vector<DslRenderCommand> DslRenderRegistry::Parse(
    const std::string& input,
    const ParseContext& ctx
) {
    LOGI("DslRenderRegistry::Parse called, renderers=%zu input_len=%zu", GetRenderers().size(), input.length());

    // Parse JSON once — all renderers share this parsed result
    nlohmann::json root = nlohmann::json::parse(input, nullptr, false);

    for (const auto& renderer : GetRenderers()) {
        if (renderer->CanParse(root)) {
            LOGI("DslRenderRegistry: Delegating to '%s' renderer", renderer->GetFormatName().c_str());
            return renderer->Parse(root, ctx);
        }
    }
    LOGE("DslRenderRegistry: No matching renderer found for input (length=%zu)", input.length());
    return {};
}

} // namespace application::dsl
