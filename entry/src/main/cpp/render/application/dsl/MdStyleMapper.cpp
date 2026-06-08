/*
 * MdStyleMapper — Convert MdBlock → TextBlock for the rendering engine.
 */

#include "MdStyleMapper.h"

namespace application::dsl {

using namespace AgenUIEngine::Core;

// Style constants
static constexpr float COLOR_CODE_R = 1.0f, COLOR_CODE_G = 0.6f, COLOR_CODE_B = 0.2f;
static constexpr float COLOR_QUOTE_R = 0.5f, COLOR_QUOTE_G = 0.5f, COLOR_QUOTE_B = 0.5f;
static constexpr float COLOR_DEFAULT_R = 1.0f, COLOR_DEFAULT_G = 1.0f, COLOR_DEFAULT_B = 1.0f;

static glm::vec3 defaultColor() { return {COLOR_DEFAULT_R, COLOR_DEFAULT_G, COLOR_DEFAULT_B}; }
static glm::vec3 codeColor()    { return {COLOR_CODE_R, COLOR_CODE_G, COLOR_CODE_B}; }
static glm::vec3 quoteColor()   { return {COLOR_QUOTE_R, COLOR_QUOTE_G, COLOR_QUOTE_B}; }

static TextSegment mapSegment(const MdSegment& seg, MdBlockType blockType) {
    TextSegment ts;
    ts.text = seg.text;

    switch (seg.style) {
    case MdStyle::Bold:
        ts.fontId = "bold";
        ts.italic = false;
        ts.color = defaultColor();
        break;
    case MdStyle::Italic:
        ts.fontId = "default";
        ts.italic = true;
        ts.color = defaultColor();
        break;
    case MdStyle::BoldItalic:
        ts.fontId = "bold";
        ts.italic = true;
        ts.color = defaultColor();
        break;
    case MdStyle::CodeInline:
        ts.fontId = "mono";
        ts.italic = false;
        ts.color = codeColor();
        break;
    case MdStyle::Normal:
    default:
        ts.fontId = "default";
        ts.italic = false;
        // Block-level color override
        if (blockType == MdBlockType::BlockQuote) {
            ts.color = quoteColor();
        } else {
            ts.color = defaultColor();
        }
        break;
    }
    return ts;
}

static float blockFontSizeScale(MdBlockType type) {
    switch (type) {
    case MdBlockType::Heading1: return 2.0f;
    case MdBlockType::Heading2: return 1.5f;
    case MdBlockType::Heading3: return 1.17f;
    case MdBlockType::Heading4: return 1.0f;
    case MdBlockType::Heading5: return 1.0f;
    case MdBlockType::Heading6: return 1.0f;
    default: return 1.0f;
    }
}

std::vector<TextBlock> mapMdToTextBlocks(const std::vector<MdBlock>& mdBlocks) {
    std::vector<TextBlock> result;
    result.reserve(mdBlocks.size());

    for (const auto& mdBlock : mdBlocks) {
        // --- Table: expand into one TextBlock per row ---
        if (mdBlock.type == MdBlockType::Table) {
            int rowCount = static_cast<int>(mdBlock.tableData.rows.size());
            for (int ri = 0; ri < rowCount; ri++) {
                const auto& row = mdBlock.tableData.rows[ri];
                TextBlock tb;
                tb.isTableRow = true;
                tb.tableColumnCount = mdBlock.tableData.columnCount;
                tb.isTableHeaderRow = (ri == 0);
                tb.isTableLastRow = (ri == rowCount - 1);

                for (int ci = 0; ci < mdBlock.tableData.columnCount; ci++) {
                    std::string cellText = (ci < static_cast<int>(row.size()))
                        ? row[ci].text : "";

                    TextSegment ts;
                    ts.text = cellText;
                    ts.color = defaultColor();

                    // Header row uses bold font
                    if (ri == 0) {
                        ts.fontId = "bold";
                    }

                    tb.segments.push_back(std::move(ts));
                    tb.segmentToColumnMap.push_back(static_cast<uint32_t>(ci));
                }

                // Per-column alignment (0=left, 1=center, 2=right)
                tb.columnAligns.resize(mdBlock.tableData.columnCount, 1); // default center
                for (int ci = 0; ci < static_cast<int>(mdBlock.tableData.columnAligns.size()); ci++) {
                    switch (mdBlock.tableData.columnAligns[ci]) {
                    case MdColumnAlign::Left:   tb.columnAligns[ci] = 0; break;
                    case MdColumnAlign::Center:  tb.columnAligns[ci] = 1; break;
                    case MdColumnAlign::Right:   tb.columnAligns[ci] = 2; break;
                    }
                }

                result.push_back(std::move(tb));
            }
            continue;
        }

        TextBlock tb;
        tb.skipRender = (mdBlock.type == MdBlockType::HorizontalRule);
        tb.fontSizeScale = blockFontSizeScale(mdBlock.type);

        // Code block background
        bool isCodeBlock = (mdBlock.type == MdBlockType::CodeBlock);
        if (isCodeBlock) {
            tb.hasBackground = true;
            tb.bgColor = glm::vec3(0.76f, 0.76f, 0.78f);
        }

        // Horizontal rule rendering
        if (mdBlock.type == MdBlockType::HorizontalRule) {
            tb.isHorizontalRule = true;
        }

        // Blockquote: left border decoration + text indent
        // Any block with quoteLevel > 0 is inside a blockquote (heading, list, paragraph)
        if (mdBlock.quoteLevel > 0) {
            tb.hasLeftBorder = true;
            tb.quoteLevel = mdBlock.quoteLevel;
            tb.leftIndent = 72.0f * mdBlock.quoteLevel;
        }

        // H1/H2 underline decoration
        if (mdBlock.type == MdBlockType::Heading1) {
            tb.hasUnderline = true;
            tb.underlineHeight = 2.0f;
        } else if (mdBlock.type == MdBlockType::Heading2) {
            tb.hasUnderline = true;
            tb.underlineHeight = 1.0f;
        }

        // Headings: force all segments to bold font
        bool isHeading = (mdBlock.type == MdBlockType::Heading1 ||
                          mdBlock.type == MdBlockType::Heading2 ||
                          mdBlock.type == MdBlockType::Heading3 ||
                          mdBlock.type == MdBlockType::Heading4 ||
                          mdBlock.type == MdBlockType::Heading5 ||
                          mdBlock.type == MdBlockType::Heading6);

        for (const auto& seg : mdBlock.segments) {
            auto ts = mapSegment(seg, mdBlock.type);
            if (isHeading && ts.fontId == "default") {
                ts.fontId = "bold";
            }
            // Code block text uses default color (not orange like inline code)
            if (isCodeBlock) {
                ts.color = defaultColor();
            }
            tb.segments.push_back(std::move(ts));
        }

        result.push_back(std::move(tb));
    }

    return result;
}

} // namespace application::dsl
