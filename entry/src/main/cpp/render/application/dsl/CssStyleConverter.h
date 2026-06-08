#pragma once

#include <string>
#include <yoga/Yoga.h>
#include "agenui_engine/thirdparty/json.hpp"

namespace application {
namespace dsl {

/**
 * Converts CSS style properties (from JSON) to Yoga layout API calls.
 * Adapted from AGenUI_ho's CSSStyleConverter, simplified to work with nlohmann::json
 * instead of SerializableData/ComponentSnapshot.
 */
class CssStyleConverter {
public:
    /**
     * Apply all recognized CSS properties from a JSON styles object to a Yoga node.
     * @param styles  JSON object like {"padding": "20px", "flex-grow": "1", ...}
     * @param node    Target Yoga node
     */
    static void applyStyles(const nlohmann::json& styles, YGNodeRef node);

private:
    // --- Dimension ---
    static void applyWidth(YGNodeRef node, const nlohmann::json& value);
    static void applyHeight(YGNodeRef node, const nlohmann::json& value);
    static void applyMinWidth(YGNodeRef node, const nlohmann::json& value);
    static void applyMaxWidth(YGNodeRef node, const nlohmann::json& value);
    static void applyMinHeight(YGNodeRef node, const nlohmann::json& value);
    static void applyMaxHeight(YGNodeRef node, const nlohmann::json& value);

    // --- Flexbox ---
    static void applyFlexDirection(YGNodeRef node, const nlohmann::json& value);
    static void applyFlexWrap(YGNodeRef node, const nlohmann::json& value);
    static void applyJustifyContent(YGNodeRef node, const nlohmann::json& value);
    static void applyAlignItems(YGNodeRef node, const nlohmann::json& value);
    static void applyAlignSelf(YGNodeRef node, const nlohmann::json& value);
    static void applyAlignContent(YGNodeRef node, const nlohmann::json& value);
    static void applyFlex(YGNodeRef node, const nlohmann::json& value);
    static void applyFlexGrow(YGNodeRef node, const nlohmann::json& value);
    static void applyFlexShrink(YGNodeRef node, const nlohmann::json& value);
    static void applyFlexBasis(YGNodeRef node, const nlohmann::json& value);

    // --- Spacing ---
    static void applyPadding(YGNodeRef node, const nlohmann::json& value);
    static void applyPaddingSide(YGNodeRef node, YGEdge edge, const nlohmann::json& value);
    static void applyMargin(YGNodeRef node, const nlohmann::json& value);
    static void applyMarginSide(YGNodeRef node, YGEdge edge, const nlohmann::json& value);
    static void applyBorder(YGNodeRef node, const nlohmann::json& value);
    static void applyBorderWidth(YGNodeRef node, const nlohmann::json& value);
    static void applyGap(YGNodeRef node, const nlohmann::json& value);

    // --- Positioning ---
    static void applyPosition(YGNodeRef node, const nlohmann::json& value);
    static void applyTop(YGNodeRef node, const nlohmann::json& value);
    static void applyRight(YGNodeRef node, const nlohmann::json& value);
    static void applyBottom(YGNodeRef node, const nlohmann::json& value);
    static void applyLeft(YGNodeRef node, const nlohmann::json& value);

    // --- Display ---
    static void applyDisplay(YGNodeRef node, const nlohmann::json& value);
    static void applyOverflow(YGNodeRef node, const nlohmann::json& value);
    static void applyDirection(YGNodeRef node, const nlohmann::json& value);
    static void applyAspectRatio(YGNodeRef node, const nlohmann::json& value);

    // --- Helpers ---
    static float parseFloat(const nlohmann::json& value, float fallback = 0.0f);
    static float parseLength(const std::string& s);
    static std::string asString(const nlohmann::json& value);
};

} // namespace dsl
} // namespace application
