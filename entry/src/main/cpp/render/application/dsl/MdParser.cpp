/*
 * MdParser — Markdown parser implementation using md4c.
 *
 * Uses the md4c SAX-style callback API to parse CommonMark (with tables)
 * into the project's MdBlock/MdSegment data structures.
 *
 * md4c provides full CommonMark 0.31 compliance plus GitHub-style tables,
 * so we no longer need to handle inline style matching, blockquote nesting,
 * paragraph continuation, or table syntax ourselves.
 */

#include "MdParser.h"
#include "md4c/md4c.h"
#include <cstring>
#include <sstream>
#include <algorithm>

namespace application::dsl {

// ============================================================================
// Parser state — accumulates blocks during md4c SAX traversal
// ============================================================================

struct ParserState {
    std::vector<MdBlock> blocks;

    // --- Inline style tracking (nested spans) ---
    int boldDepth = 0;
    int italicDepth = 0;
    int codeDepth = 0;

    MdStyle currentStyle() const {
        if (codeDepth > 0) return MdStyle::CodeInline;
        bool b = boldDepth > 0;
        bool i = italicDepth > 0;
        if (b && i) return MdStyle::BoldItalic;
        if (b) return MdStyle::Bold;
        if (i) return MdStyle::Italic;
        return MdStyle::Normal;
    }

    // --- Text accumulator ---
    std::string currentText;
    std::vector<MdSegment> pendingSegments;

    void flushText() {
        if (!currentText.empty()) {
            pendingSegments.push_back({currentText, currentStyle()});
            currentText.clear();
        }
    }

    // --- Block nesting ---
    int quoteLevel = 0;
    int headingLevel = 0;
    bool inCodeBlock = false;
    std::string codeBlockText;

    // --- Table state ---
    bool inTable = false;
    std::vector<MdColumnAlign> tableAligns;
    std::vector<std::vector<MdTableCell>> tableRows;
    std::vector<MdTableCell> currentRowCells;
    std::string cellText;

    void flushCellText() {
        size_t s = cellText.find_first_not_of(" \t\n\r");
        size_t e = cellText.find_last_not_of(" \t\n\r");
        if (s == std::string::npos)
            currentRowCells.push_back({""});
        else
            currentRowCells.push_back({cellText.substr(s, e - s + 1)});
        cellText.clear();
    }

    // Finalize a text-containing block (P, H, LI) from pendingSegments
    void finalizeBlock(MdBlockType type) {
        flushText();
        MdBlock block;
        block.type = type;
        block.quoteLevel = quoteLevel;

        if (type == MdBlockType::ListItem) {
            block.segments.push_back({"\xE2\x97\x8F ", MdStyle::Normal}); // ● bullet
        }

        for (auto& seg : pendingSegments)
            block.segments.push_back(std::move(seg));
        pendingSegments.clear();

        // Headings: force bold style and merge into single segment
        if (type >= MdBlockType::Heading1 && type <= MdBlockType::Heading6) {
            for (auto& seg : block.segments) {
                if (seg.style == MdStyle::Normal)
                    seg.style = MdStyle::Bold;
            }
            if (block.segments.size() > 1) {
                std::string merged;
                for (auto& seg : block.segments) merged += seg.text;
                block.segments.clear();
                block.segments.push_back({merged, MdStyle::Bold});
            } else if (!block.segments.empty()) {
                block.segments[0].style = MdStyle::Bold;
            }
        }

        blocks.push_back(std::move(block));
    }
};

// ============================================================================
// md4c callbacks
// ============================================================================

static int enterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto* st = static_cast<ParserState*>(userdata);

    switch (type) {
    case MD_BLOCK_QUOTE:
        st->quoteLevel++;
        break;
    case MD_BLOCK_H: {
        auto* h = static_cast<MD_BLOCK_H_DETAIL*>(detail);
        st->headingLevel = h->level;
        break;
    }
    case MD_BLOCK_CODE:
        st->inCodeBlock = true;
        st->codeBlockText.clear();
        break;
    case MD_BLOCK_TABLE:
        st->inTable = true;
        st->tableRows.clear();
        st->tableAligns.clear();
        break;
    case MD_BLOCK_TR:
        st->currentRowCells.clear();
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        st->cellText.clear();
        break;
    default:
        break;
    }
    return 0;
}

static int leaveBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto* st = static_cast<ParserState*>(userdata);

    switch (type) {
    case MD_BLOCK_QUOTE:
        st->quoteLevel = std::max(0, st->quoteLevel - 1);
        break;

    case MD_BLOCK_P:
        st->finalizeBlock(MdBlockType::Paragraph);
        break;

    case MD_BLOCK_LI:
        st->finalizeBlock(MdBlockType::ListItem);
        break;

    case MD_BLOCK_H: {
        MdBlockType bt;
        switch (st->headingLevel) {
        case 1: bt = MdBlockType::Heading1; break;
        case 2: bt = MdBlockType::Heading2; break;
        case 3: bt = MdBlockType::Heading3; break;
        case 4: bt = MdBlockType::Heading4; break;
        case 5: bt = MdBlockType::Heading5; break;
        default: bt = MdBlockType::Heading6; break;
        }
        st->finalizeBlock(bt);
        break;
    }

    case MD_BLOCK_HR:
        st->blocks.push_back({MdBlockType::HorizontalRule, {}, st->quoteLevel, {}});
        break;

    case MD_BLOCK_CODE: {
        MdBlock block;
        block.type = MdBlockType::CodeBlock;
        block.quoteLevel = st->quoteLevel;
        // Split code block text into lines as CodeInline segments
        std::istringstream cs(st->codeBlockText);
        std::string cline;
        bool first = true;
        while (std::getline(cs, cline)) {
            if (!first) block.segments.push_back({"\n", MdStyle::CodeInline});
            block.segments.push_back({cline, MdStyle::CodeInline});
            first = false;
        }
        if (block.segments.empty())
            block.segments.push_back({"", MdStyle::CodeInline});
        st->blocks.push_back(std::move(block));
        st->codeBlockText.clear();
        st->inCodeBlock = false;
        break;
    }

    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        st->flushCellText();
        // Collect alignment from header cells only (first row)
        if (st->tableRows.empty()) {
            auto* td = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
            MdColumnAlign align = MdColumnAlign::Left;
            if (td->align == MD_ALIGN_CENTER) align = MdColumnAlign::Center;
            else if (td->align == MD_ALIGN_RIGHT) align = MdColumnAlign::Right;
            st->tableAligns.push_back(align);
        }
        break;

    case MD_BLOCK_TR:
        st->tableRows.push_back(st->currentRowCells);
        st->currentRowCells.clear();
        break;

    case MD_BLOCK_TABLE: {
        MdBlock block;
        block.type = MdBlockType::Table;
        block.quoteLevel = st->quoteLevel;
        block.tableData.columnCount = st->tableRows.empty() ? 0
            : static_cast<int>(st->tableRows[0].size());
        block.tableData.columnAligns = st->tableAligns;
        block.tableData.rows = std::move(st->tableRows);
        st->blocks.push_back(std::move(block));
        st->inTable = false;
        break;
    }

    default:
        break;
    }
    return 0;
}

static int enterSpan(MD_SPANTYPE type, void* /*detail*/, void* userdata) {
    auto* st = static_cast<ParserState*>(userdata);
    st->flushText();

    switch (type) {
    case MD_SPAN_STRONG:  st->boldDepth++;  break;
    case MD_SPAN_EM:      st->italicDepth++; break;
    case MD_SPAN_CODE:    st->codeDepth++;  break;
    default: break;
    }
    return 0;
}

static int leaveSpan(MD_SPANTYPE type, void* /*detail*/, void* userdata) {
    auto* st = static_cast<ParserState*>(userdata);
    st->flushText();

    switch (type) {
    case MD_SPAN_STRONG:  st->boldDepth  = std::max(0, st->boldDepth  - 1); break;
    case MD_SPAN_EM:      st->italicDepth = std::max(0, st->italicDepth - 1); break;
    case MD_SPAN_CODE:    st->codeDepth  = std::max(0, st->codeDepth  - 1); break;
    default: break;
    }
    return 0;
}

static int textCallback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size,
                         void* userdata) {
    auto* st = static_cast<ParserState*>(userdata);
    std::string str(text, size);

    // --- Table cell text ---
    if (st->inTable) {
        if (type == MD_TEXT_NORMAL || type == MD_TEXT_CODE) {
            st->cellText += str;
        } else if (type == MD_TEXT_BR) {
            st->cellText += "\n";
        }
        return 0;
    }

    // --- Code block text ---
    if (st->inCodeBlock) {
        if (type == MD_TEXT_CODE) {
            st->codeBlockText += str;
        } else if (type == MD_TEXT_BR) {
            st->codeBlockText += "\n";
        }
        return 0;
    }

    // --- Normal inline text ---
    switch (type) {
    case MD_TEXT_NORMAL:
    case MD_TEXT_CODE:
    case MD_TEXT_HTML:
    case MD_TEXT_ENTITY:
        st->currentText += str;
        break;
    case MD_TEXT_SOFTBR:
        st->currentText += " ";
        break;
    case MD_TEXT_BR:
        st->currentText += " ";
        break;
    default:
        break;
    }
    return 0;
}

// ============================================================================
// Public API
// ============================================================================

std::vector<MdBlock> parseMarkdown(const std::string& mdText) {
    if (mdText.empty()) return {};

    ParserState state;

    MD_PARSER parser;
    std::memset(&parser, 0, sizeof(MD_PARSER));
    parser.flags = MD_FLAG_TABLES;
    parser.enter_block = enterBlock;
    parser.leave_block = leaveBlock;
    parser.enter_span  = enterSpan;
    parser.leave_span  = leaveSpan;
    parser.text        = textCallback;

    int ret = md_parse(mdText.c_str(), static_cast<MD_SIZE>(mdText.size()),
                       &parser, &state);
    if (ret != 0) return {};

    return std::move(state.blocks);
}

} // namespace application::dsl
