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

#ifndef DSL_RENDER_REGISTRY_H
#define DSL_RENDER_REGISTRY_H

#include "IDslRender.h"
#include <memory>
#include <vector>

namespace application::dsl {

/**
 * Central registry for DSL format renderers.
 * Renderers self-register via static initializers in their own .cpp files.
 * This file contains ONLY framework code — it never needs modification
 * when adding new DSL formats.
 *
 * Usage:
 *   auto commands = DslRenderRegistry::Parse(jsonString);
 */
class DslRenderRegistry {
public:
    /**
     * Register a renderer instance. Called by each renderer's static initializer.
     */
    static void Register(std::unique_ptr<IDslRender> renderer);

    /**
     * Detect the DSL format and parse it using the matching renderer.
     * Iterates through registered renderers, calling CanParse() on each.
     * The first one that matches handles the input.
     *
     * @param input The raw DSL string (JSON)
     * @param ctx Context with data model and design dimensions
     * @return List of render commands, empty on failure
     */
    static std::vector<DslRenderCommand> Parse(
        const std::string& input,
        const ParseContext& ctx = {}
    );

private:
    static std::vector<std::unique_ptr<IDslRender>>& GetRenderers();
};

} // namespace application::dsl

#endif // DSL_RENDER_REGISTRY_H
