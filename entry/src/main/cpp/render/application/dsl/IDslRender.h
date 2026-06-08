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

#ifndef IDSL_RENDER_H
#define IDSL_RENDER_H

#include "DslRenderCommand.h"
#include <string>
#include <vector>
#include "agenui_engine/thirdparty/json.hpp"

namespace application::dsl {

/**
 * Context passed to parser's Parse() method.
 * Contains rendering parameters that parsers need for layout calculation.
 */
struct ParseContext {
    nlohmann::json dataModel;     // Data model for path resolution (A2UI style)
    float designWidth = 1276.0f;  // Design space width in pixels
    float designHeight = 2400.0f; // Design space height in pixels
    float density = 0.0f;         // Device pixel density (densityPixels), 0 = not set
    bool autoFix = false;         // Enable auto-fix for overflow and overlap
};

/**
 * Abstract interface for all DSL format parsers.
 * Each DSL format (Custom, A2UI, etc.) implements this interface.
 *
 * To add a new DSL format:
 * 1. Create XxxParser.h/.cpp implementing this interface
 * 2. Call DslRenderRegistry::Register() in your .cpp static initializer
 * 3. Add the .cpp to CMakeLists.txt
 * 4. Add prompt and debug_cases as needed
 */
class IDslRender {
public:
    virtual ~IDslRender() = default;

    /** Human-readable name for logging/debugging */
    virtual std::string GetFormatName() const = 0;

    /**
     * Quick check if this parser can handle the given pre-parsed JSON.
     * Registry parses JSON once and passes the result to avoid redundant parsing.
     */
    virtual bool CanParse(const nlohmann::json& root) const = 0;

    /**
     * Parse the pre-parsed JSON into a list of DslRenderCommand.
     * @param root Pre-parsed JSON object from the DSL string
     * @param ctx Context with data model and design dimensions
     * @return List of render commands, empty on failure
     */
    virtual std::vector<DslRenderCommand> Parse(
        const nlohmann::json& root,
        const ParseContext& ctx = {}
    ) const = 0;
};

} // namespace application::dsl

#endif // IDSL_RENDER_H
