/*
 * MdParser — Lightweight Markdown parser for styled text rendering.
 * Parses a subset of CommonMark into blocks with inline style segments.
 *
 * Supported syntax:
 *   # H1  ## H2  ### H3  #### H4  ##### H5  ###### H6
 *   **bold**  *italic*  ***bold italic***
 *   `inline code`
 *   - list item
 *   > blockquote
 *   --- horizontal rule
 *   Paragraphs (separated by blank lines)
 */

#ifndef APP_DSL_MDPARSER_H
#define APP_DSL_MDPARSER_H

#include <string>
#include <vector>

namespace application::dsl {

// Inline text style
enum class MdStyle {
    Normal,
    Bold,
    Italic,
    BoldItalic,
    CodeInline,
};

// An inline segment with uniform style
struct MdSegment {
    std::string text;       // Plain text (MD markup stripped)
    MdStyle style = MdStyle::Normal;
};

// Column alignment for table cells
enum class MdColumnAlign { Left, Center, Right };

// A single table cell (plain text, no inline styles in MVP)
struct MdTableCell {
    std::string text;
};

// Table data: rows[0] = header row
struct MdTableData {
    int columnCount = 0;
    std::vector<MdColumnAlign> columnAligns;
    std::vector<std::vector<MdTableCell>> rows;  // rows[0] = header
};

// Block-level element types
enum class MdBlockType {
    Paragraph,
    Heading1,
    Heading2,
    Heading3,
    Heading4,
    Heading5,
    Heading6,
    ListItem,
    BlockQuote,
    CodeBlock,
    HorizontalRule,
    Table,
};

// A block containing inline segments
struct MdBlock {
    MdBlockType type = MdBlockType::Paragraph;
    std::vector<MdSegment> segments;
    int quoteLevel = 0;  ///< Blockquote nesting depth (1 = >, 2 = >>, etc.)
    MdTableData tableData;  ///< Only valid when type == Table
};

/**
 * Parse Markdown text into a list of styled blocks.
 * @param mdText  Raw Markdown string (may contain \n line breaks)
 * @return Ordered list of blocks with their inline segments
 */
std::vector<MdBlock> parseMarkdown(const std::string& mdText);

} // namespace application::dsl

#endif // APP_DSL_MDPARSER_H
