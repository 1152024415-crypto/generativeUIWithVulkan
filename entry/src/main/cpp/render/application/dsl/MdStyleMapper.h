/*
 * MdStyleMapper — Converts MdBlock (markdown AST) into engine TextBlock.
 *
 * This is the bridge between the markdown parser (application layer)
 * and the rendering engine (which only knows TextBlock/TextSegment).
 *
 * All style decisions (colors, font IDs, font size scaling) live here,
 * keeping the engine markdown-agnostic and the parser style-agnostic.
 */

#ifndef APP_DSL_MDSTYLEMAPPER_H
#define APP_DSL_MDSTYLEMAPPER_H

#include "MdParser.h"
#include "agenui_engine/core/IGraphicsAPI.h"
#include <vector>

namespace application::dsl {

/**
 * Map parsed markdown blocks into engine-friendly TextBlocks.
 *
 * Style policy:
 *   Headings: bold font, white color, scaled fontSize (H1=2x, H2=1.5x, H3=1.17x)
 *   CodeInline/CodeBlock: "mono" font, orange color (1.0, 0.6, 0.2)
 *   Bold/BoldItalic: "bold" font, white color
 *   Italic: "default" font, white color, italic=true
 *   BlockQuote: gray color (0.5, 0.5, 0.5)
 *   HorizontalRule: skipRender=true (reserves vertical space)
 *   Paragraph/ListItem: default style, white color
 */
std::vector<AgenUIEngine::Core::TextBlock> mapMdToTextBlocks(const std::vector<MdBlock>& mdBlocks);

} // namespace application::dsl

#endif // APP_DSL_MDSTYLEMAPPER_H
