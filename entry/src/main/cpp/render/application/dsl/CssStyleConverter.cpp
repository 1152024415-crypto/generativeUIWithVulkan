#include "dsl/CssStyleConverter.h"
#include <cctype>
#include <sstream>
#include <vector>

namespace application {
namespace dsl {

// ===== Helpers =====

std::string CssStyleConverter::asString(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number()) return std::to_string(value.get<float>());
    return "";
}

float CssStyleConverter::parseFloat(const nlohmann::json& value, float fallback) {
    if (value.is_number()) return value.get<float>();
    if (value.is_string()) {
        try { return std::stof(value.get<std::string>()); } catch (...) {}
    }
    return fallback;
}

float CssStyleConverter::parseLength(const std::string& s) {
    if (s.empty()) return 0.0f;
    std::string num = s;
    size_t pos = s.find_first_not_of("0123456789.-");
    if (pos != std::string::npos) num = s.substr(0, pos);
    try { return std::stof(num); } catch (...) { return 0.0f; }
}

// ===== applyStyles — main entry point =====

void CssStyleConverter::applyStyles(const nlohmann::json& styles, YGNodeRef node) {
    if (!styles.is_object() || !node) return;

    // clang-format off
    // Dimension
    if (styles.contains("width"))          applyWidth(node, styles["width"]);
    if (styles.contains("height"))         applyHeight(node, styles["height"]);
    if (styles.contains("min-width"))      applyMinWidth(node, styles["min-width"]);
    if (styles.contains("max-width"))      applyMaxWidth(node, styles["max-width"]);
    if (styles.contains("min-height"))     applyMinHeight(node, styles["min-height"]);
    if (styles.contains("max-height"))     applyMaxHeight(node, styles["max-height"]);
    // Flexbox
    if (styles.contains("flex-direction")) applyFlexDirection(node, styles["flex-direction"]);
    if (styles.contains("flex-wrap"))      applyFlexWrap(node, styles["flex-wrap"]);
    if (styles.contains("justify-content"))applyJustifyContent(node, styles["justify-content"]);
    if (styles.contains("align-items"))    applyAlignItems(node, styles["align-items"]);
    if (styles.contains("align-self"))     applyAlignSelf(node, styles["align-self"]);
    if (styles.contains("align-content"))  applyAlignContent(node, styles["align-content"]);
    if (styles.contains("flex"))           applyFlex(node, styles["flex"]);
    if (styles.contains("flex-grow"))      applyFlexGrow(node, styles["flex-grow"]);
    if (styles.contains("flex-shrink"))    applyFlexShrink(node, styles["flex-shrink"]);
    if (styles.contains("flex-basis"))     applyFlexBasis(node, styles["flex-basis"]);
    // Spacing
    if (styles.contains("padding"))        applyPadding(node, styles["padding"]);
    if (styles.contains("padding-top"))    applyPaddingSide(node, YGEdgeTop, styles["padding-top"]);
    if (styles.contains("padding-right"))  applyPaddingSide(node, YGEdgeRight, styles["padding-right"]);
    if (styles.contains("padding-bottom")) applyPaddingSide(node, YGEdgeBottom, styles["padding-bottom"]);
    if (styles.contains("padding-left"))   applyPaddingSide(node, YGEdgeLeft, styles["padding-left"]);
    if (styles.contains("margin"))         applyMargin(node, styles["margin"]);
    if (styles.contains("margin-top"))     applyMarginSide(node, YGEdgeTop, styles["margin-top"]);
    if (styles.contains("margin-right"))   applyMarginSide(node, YGEdgeRight, styles["margin-right"]);
    if (styles.contains("margin-bottom"))  applyMarginSide(node, YGEdgeBottom, styles["margin-bottom"]);
    if (styles.contains("margin-left"))    applyMarginSide(node, YGEdgeLeft, styles["margin-left"]);
    if (styles.contains("border-width"))   applyBorderWidth(node, styles["border-width"]);
    if (styles.contains("border"))         applyBorder(node, styles["border"]);
    if (styles.contains("gap"))            applyGap(node, styles["gap"]);
    // Positioning
    if (styles.contains("position"))       applyPosition(node, styles["position"]);
    if (styles.contains("top"))            applyTop(node, styles["top"]);
    if (styles.contains("right"))          applyRight(node, styles["right"]);
    if (styles.contains("bottom"))         applyBottom(node, styles["bottom"]);
    if (styles.contains("left"))           applyLeft(node, styles["left"]);
    // Display
    if (styles.contains("display"))        applyDisplay(node, styles["display"]);
    if (styles.contains("overflow"))       applyOverflow(node, styles["overflow"]);
    if (styles.contains("direction"))      applyDirection(node, styles["direction"]);
    if (styles.contains("aspect-ratio"))   applyAspectRatio(node, styles["aspect-ratio"]);
    // camelCase aliases
    if (styles.contains("justifyContent")) applyJustifyContent(node, styles["justifyContent"]);
    if (styles.contains("alignItems"))     applyAlignItems(node, styles["alignItems"]);
    if (styles.contains("flexWrap"))       applyFlexWrap(node, styles["flexWrap"]);
    if (styles.contains("alignContent"))   applyAlignContent(node, styles["alignContent"]);
    if (styles.contains("alignSelf"))      applyAlignSelf(node, styles["alignSelf"]);
    if (styles.contains("flexGrow"))       applyFlexGrow(node, styles["flexGrow"]);
    if (styles.contains("flexShrink"))     applyFlexShrink(node, styles["flexShrink"]);
    if (styles.contains("flexBasis"))      applyFlexBasis(node, styles["flexBasis"]);
    if (styles.contains("borderWidth"))    applyBorderWidth(node, styles["borderWidth"]);
    // clang-format on
}

// ===== Dimension =====

void CssStyleConverter::applyWidth(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetWidth(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (s == "auto") { YGNodeStyleSetWidthAuto(node); }
    else if (!s.empty() && s.back() == '%') { YGNodeStyleSetWidthPercent(node, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetWidth(node, parseLength(s)); }
}

void CssStyleConverter::applyHeight(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetHeight(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (s == "auto") { YGNodeStyleSetHeightAuto(node); }
    else if (!s.empty() && s.back() == '%') { YGNodeStyleSetHeightPercent(node, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetHeight(node, parseLength(s)); }
}

void CssStyleConverter::applyMinWidth(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetMinWidth(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetMinWidthPercent(node, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetMinWidth(node, parseLength(s)); }
}

void CssStyleConverter::applyMaxWidth(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetMaxWidth(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetMaxWidthPercent(node, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetMaxWidth(node, parseLength(s)); }
}

void CssStyleConverter::applyMinHeight(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetMinHeight(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetMinHeightPercent(node, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetMinHeight(node, parseLength(s)); }
}

void CssStyleConverter::applyMaxHeight(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetMaxHeight(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetMaxHeightPercent(node, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetMaxHeight(node, parseLength(s)); }
}

// ===== Flexbox =====

void CssStyleConverter::applyFlexDirection(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "row")              YGNodeStyleSetFlexDirection(node, YGFlexDirectionRow);
    else if (s == "row-reverse") YGNodeStyleSetFlexDirection(node, YGFlexDirectionRowReverse);
    else if (s == "column")      YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumn);
    else if (s == "column-reverse") YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumnReverse);
}

void CssStyleConverter::applyFlexWrap(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "nowrap")       YGNodeStyleSetFlexWrap(node, YGWrapNoWrap);
    else if (s == "wrap")    YGNodeStyleSetFlexWrap(node, YGWrapWrap);
    else if (s == "wrap-reverse") YGNodeStyleSetFlexWrap(node, YGWrapWrapReverse);
}

void CssStyleConverter::applyJustifyContent(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "flex-start")     YGNodeStyleSetJustifyContent(node, YGJustifyFlexStart);
    else if (s == "center")    YGNodeStyleSetJustifyContent(node, YGJustifyCenter);
    else if (s == "flex-end")  YGNodeStyleSetJustifyContent(node, YGJustifyFlexEnd);
    else if (s == "space-between") YGNodeStyleSetJustifyContent(node, YGJustifySpaceBetween);
    else if (s == "space-around")  YGNodeStyleSetJustifyContent(node, YGJustifySpaceAround);
    else if (s == "space-evenly")  YGNodeStyleSetJustifyContent(node, YGJustifySpaceEvenly);
}

void CssStyleConverter::applyAlignItems(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "flex-start") YGNodeStyleSetAlignItems(node, YGAlignFlexStart);
    else if (s == "center")    YGNodeStyleSetAlignItems(node, YGAlignCenter);
    else if (s == "flex-end")  YGNodeStyleSetAlignItems(node, YGAlignFlexEnd);
    else if (s == "stretch")   YGNodeStyleSetAlignItems(node, YGAlignStretch);
    else if (s == "baseline")  YGNodeStyleSetAlignItems(node, YGAlignBaseline);
}

void CssStyleConverter::applyAlignSelf(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "auto")           YGNodeStyleSetAlignSelf(node, YGAlignAuto);
    else if (s == "flex-start") YGNodeStyleSetAlignSelf(node, YGAlignFlexStart);
    else if (s == "center")    YGNodeStyleSetAlignSelf(node, YGAlignCenter);
    else if (s == "flex-end")  YGNodeStyleSetAlignSelf(node, YGAlignFlexEnd);
    else if (s == "stretch")   YGNodeStyleSetAlignSelf(node, YGAlignStretch);
    else if (s == "baseline")  YGNodeStyleSetAlignSelf(node, YGAlignBaseline);
}

void CssStyleConverter::applyAlignContent(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "flex-start")     YGNodeStyleSetAlignContent(node, YGAlignFlexStart);
    else if (s == "center")    YGNodeStyleSetAlignContent(node, YGAlignCenter);
    else if (s == "flex-end")  YGNodeStyleSetAlignContent(node, YGAlignFlexEnd);
    else if (s == "stretch")   YGNodeStyleSetAlignContent(node, YGAlignStretch);
    else if (s == "space-between") YGNodeStyleSetAlignContent(node, YGAlignSpaceBetween);
    else if (s == "space-around")  YGNodeStyleSetAlignContent(node, YGAlignSpaceAround);
}

void CssStyleConverter::applyFlex(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    YGNodeStyleSetFlex(node, parseFloat(value));
}

void CssStyleConverter::applyFlexGrow(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    YGNodeStyleSetFlexGrow(node, parseFloat(value));
}

void CssStyleConverter::applyFlexShrink(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    YGNodeStyleSetFlexShrink(node, parseFloat(value));
}

void CssStyleConverter::applyFlexBasis(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetFlexBasis(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (s == "auto") { YGNodeStyleSetFlexBasisAuto(node); }
    else if (!s.empty() && s.back() == '%') { YGNodeStyleSetFlexBasisPercent(node, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetFlexBasis(node, parseLength(s)); }
}

// ===== Spacing =====

void CssStyleConverter::applyPadding(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetPadding(node, YGEdgeAll, value.get<float>()); return; }
    std::string s = asString(value);
    if (s.empty()) return;
    std::vector<float> vals;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        size_t px = tok.find("px");
        if (px != std::string::npos) tok = tok.substr(0, px);
        try { vals.push_back(std::stof(tok)); } catch (...) {}
    }
    if (vals.size() == 1)      YGNodeStyleSetPadding(node, YGEdgeAll, vals[0]);
    else if (vals.size() == 2) { YGNodeStyleSetPadding(node, YGEdgeVertical, vals[0]); YGNodeStyleSetPadding(node, YGEdgeHorizontal, vals[1]); }
    else if (vals.size() >= 4) { YGNodeStyleSetPadding(node, YGEdgeTop, vals[0]); YGNodeStyleSetPadding(node, YGEdgeRight, vals[1]); YGNodeStyleSetPadding(node, YGEdgeBottom, vals[2]); YGNodeStyleSetPadding(node, YGEdgeLeft, vals[3]); }
}

void CssStyleConverter::applyPaddingSide(YGNodeRef node, YGEdge edge, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetPadding(node, edge, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetPaddingPercent(node, edge, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetPadding(node, edge, parseLength(s)); }
}

void CssStyleConverter::applyMargin(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetMargin(node, YGEdgeAll, value.get<float>()); return; }
    std::string s = asString(value);
    if (s.empty()) return;
    std::vector<float> vals;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        try { vals.push_back(std::stof(tok)); } catch (...) {}
    }
    if (vals.size() == 1)      YGNodeStyleSetMargin(node, YGEdgeAll, vals[0]);
    else if (vals.size() == 2) { YGNodeStyleSetMargin(node, YGEdgeVertical, vals[0]); YGNodeStyleSetMargin(node, YGEdgeHorizontal, vals[1]); }
    else if (vals.size() >= 4) { YGNodeStyleSetMargin(node, YGEdgeTop, vals[0]); YGNodeStyleSetMargin(node, YGEdgeRight, vals[1]); YGNodeStyleSetMargin(node, YGEdgeBottom, vals[2]); YGNodeStyleSetMargin(node, YGEdgeLeft, vals[3]); }
}

void CssStyleConverter::applyMarginSide(YGNodeRef node, YGEdge edge, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetMargin(node, edge, value.get<float>()); return; }
    std::string s = asString(value);
    if (s == "auto") { YGNodeStyleSetMarginAuto(node, edge); }
    else if (!s.empty() && s.back() == '%') { YGNodeStyleSetMarginPercent(node, edge, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetMargin(node, edge, parseLength(s)); }
}

void CssStyleConverter::applyBorder(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetBorder(node, YGEdgeAll, value.get<float>()); return; }
    std::string s = asString(value);
    if (s.empty()) return;
    // Parse numeric tokens only (skip keywords like "solid", "#fff")
    std::vector<float> vals;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        if (!tok.empty() && (std::isdigit(static_cast<unsigned char>(tok[0])) || tok[0] == '.')) {
            try { vals.push_back(std::stof(tok)); } catch (...) {}
        }
    }
    if (vals.size() == 1)      YGNodeStyleSetBorder(node, YGEdgeAll, vals[0]);
    else if (vals.size() >= 4) { YGNodeStyleSetBorder(node, YGEdgeTop, vals[0]); YGNodeStyleSetBorder(node, YGEdgeRight, vals[1]); YGNodeStyleSetBorder(node, YGEdgeBottom, vals[2]); YGNodeStyleSetBorder(node, YGEdgeLeft, vals[3]); }
}

void CssStyleConverter::applyBorderWidth(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetBorder(node, YGEdgeAll, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty()) YGNodeStyleSetBorder(node, YGEdgeAll, parseLength(s));
}

void CssStyleConverter::applyGap(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    YGNodeStyleSetGap(node, YGGutterAll, parseFloat(value));
}

// ===== Positioning =====

void CssStyleConverter::applyPosition(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "relative") YGNodeStyleSetPositionType(node, YGPositionTypeRelative);
    else if (s == "absolute") YGNodeStyleSetPositionType(node, YGPositionTypeAbsolute);
}

void CssStyleConverter::applyTop(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetPosition(node, YGEdgeTop, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetPositionPercent(node, YGEdgeTop, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetPosition(node, YGEdgeTop, parseLength(s)); }
}

void CssStyleConverter::applyRight(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetPosition(node, YGEdgeRight, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetPositionPercent(node, YGEdgeRight, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetPosition(node, YGEdgeRight, parseLength(s)); }
}

void CssStyleConverter::applyBottom(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetPosition(node, YGEdgeBottom, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetPositionPercent(node, YGEdgeBottom, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetPosition(node, YGEdgeBottom, parseLength(s)); }
}

void CssStyleConverter::applyLeft(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetPosition(node, YGEdgeLeft, value.get<float>()); return; }
    std::string s = asString(value);
    if (!s.empty() && s.back() == '%') { YGNodeStyleSetPositionPercent(node, YGEdgeLeft, std::stof(s.substr(0, s.size()-1))); }
    else if (!s.empty()) { YGNodeStyleSetPosition(node, YGEdgeLeft, parseLength(s)); }
}

// ===== Display =====

void CssStyleConverter::applyDisplay(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "flex")      YGNodeStyleSetDisplay(node, YGDisplayFlex);
    else if (s == "none") YGNodeStyleSetDisplay(node, YGDisplayNone);
}

void CssStyleConverter::applyOverflow(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "visible") YGNodeStyleSetOverflow(node, YGOverflowVisible);
    else if (s == "hidden") YGNodeStyleSetOverflow(node, YGOverflowHidden);
    else if (s == "scroll") YGNodeStyleSetOverflow(node, YGOverflowScroll);
}

void CssStyleConverter::applyDirection(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    std::string s = asString(value);
    if (s == "ltr")     YGNodeStyleSetDirection(node, YGDirectionLTR);
    else if (s == "rtl") YGNodeStyleSetDirection(node, YGDirectionRTL);
    else if (s == "inherit") YGNodeStyleSetDirection(node, YGDirectionInherit);
}

void CssStyleConverter::applyAspectRatio(YGNodeRef node, const nlohmann::json& value) {
    if (!node) return;
    if (value.is_number()) { YGNodeStyleSetAspectRatio(node, value.get<float>()); return; }
    std::string s = asString(value);
    if (s.empty()) return;
    size_t slash = s.find('/');
    if (slash != std::string::npos) {
        float num = std::stof(s.substr(0, slash));
        float den = std::stof(s.substr(slash + 1));
        if (num > 0 && den > 0) YGNodeStyleSetAspectRatio(node, num / den);
    } else {
        float ar = std::stof(s);
        if (ar > 0) YGNodeStyleSetAspectRatio(node, ar);
    }
}

} // namespace dsl
} // namespace application
