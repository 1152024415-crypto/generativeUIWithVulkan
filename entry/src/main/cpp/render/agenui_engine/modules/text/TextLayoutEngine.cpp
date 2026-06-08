/*
 * TextLayoutEngine — Pure CPU text layout implementation.
 * Split from TextRenderer: all GPU operations moved to TextPipeline.
 */

#include "TextLayoutEngine.h"
#include "FontManager.h"
#include "TextAtlas.h"
#include "MsdfAtlasData.h"
#include "logger_common.h"
#include "text_layout/TextLayout.h"
#include "core/IGraphicsAPI.h"
#include <hitrace/trace.h>

#ifdef AGENUI_PLATFORM_HARMONYOS
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include <hilog/log.h>
#endif

#include "thirdparty/glm/glm.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <memory>
#include <chrono>
#include <fstream>
#include <optional>
#include <functional>

namespace AgenUIEngine {

// ============================================================
// Constructor & Destructor
// ============================================================

TextLayoutEngine::TextLayoutEngine() {
}

TextLayoutEngine::~TextLayoutEngine() {
    cleanup();
}

// ============================================================
// Lifecycle
// ============================================================

#if AGENUI_PLATFORM_HARMONYOS
bool TextLayoutEngine::initialize(void* resourceManager, const std::string& fontName) {
    LOGI("TextLayoutEngine: Initializing (CPU only, HarmonyOS)");

    m_fontManager = std::make_unique<FontManager>();
    if (!m_fontManager->initialize()) {
        LOGE("TextLayoutEngine: Failed to initialize font manager");
        return false;
    }

    m_textAtlas = std::make_unique<TextAtlas>();
    if (!m_textAtlas->initialize(2048, 2048)) {
        LOGE("TextLayoutEngine: Failed to initialize text atlas");
        return false;
    }

    m_initialized = true;
    LOGI("TextLayoutEngine: Initialized successfully (HarmonyOS)");
    return true;
}
#endif

#if AGENUI_PLATFORM_WINDOWS
bool TextLayoutEngine::initializeFromFile(const char* fontPath) {
    LOGI("TextLayoutEngine: Initializing from file '%s' (CPU only)", fontPath);

    m_fontManager = std::make_unique<FontManager>();
    if (!m_fontManager->initialize()) {
        LOGE("TextLayoutEngine: Failed to initialize font manager");
        return false;
    }

    if (!m_fontManager->loadFont(fontPath)) {
        LOGE("TextLayoutEngine: Failed to load font from '%s'", fontPath);
        return false;
    }

    // Test if font supports Chinese
    const Glyph* testGlyph = m_fontManager->getGlyph(0x4F60, 64);  // "你"
    if (testGlyph != nullptr && testGlyph->width > 0) {
        LOGI("TextLayoutEngine: Font '%s' supports Chinese", fontPath);
    } else {
        LOGW("TextLayoutEngine: Font '%s' doesn't support Chinese", fontPath);
    }

    m_textAtlas = std::make_unique<TextAtlas>();
    if (!m_textAtlas->initialize(2048, 2048)) {
        LOGE("TextLayoutEngine: Failed to initialize text atlas");
        return false;
    }

    m_initialized = true;
    LOGI("TextLayoutEngine: Initialized successfully from file");
    return true;
}
#endif

void TextLayoutEngine::cleanup() {
    LOGI("TextLayoutEngine: Cleaning up CPU resources");
    m_fontManager.reset();
    m_textAtlas.reset();
    m_sdfAtlas.reset();
    m_msdfAtlasData.reset();
    m_glyphsInAtlas.clear();
    m_sdfGlyphsInAtlas.clear();
    m_initialized = false;
}

// ============================================================
// Atlas management (CPU side)
// ============================================================

const uint8_t* TextLayoutEngine::getAtlasData() const {
    return (m_textAtlas && m_textAtlas->getData()) ? m_textAtlas->getData() : nullptr;
}

uint32_t TextLayoutEngine::getAtlasWidth() const {
    return m_textAtlas ? m_textAtlas->getWidth() : 0;
}

uint32_t TextLayoutEngine::getAtlasHeight() const {
    return m_textAtlas ? m_textAtlas->getHeight() : 0;
}

const uint8_t* TextLayoutEngine::getSDFAtlasData() const {
    return (m_sdfAtlas && m_sdfAtlas->getData()) ? m_sdfAtlas->getData() : nullptr;
}

bool TextLayoutEngine::loadMsdfDataFromJson(const std::string& jsonPath) {
    std::ifstream jf(jsonPath, std::ios::binary);
    if (!jf) return false;
    std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(jf)),
                                   std::istreambuf_iterator<char>());
    if (jsonBytes.empty()) return false;

    MsdfAtlasData data;
    if (!data.loadFromJson(jsonBytes)) return false;

    m_msdfAtlasData = std::move(data);
    LOGI("TextLayoutEngine: MSDF data loaded from %s", jsonPath.c_str());
    return true;
}

bool TextLayoutEngine::ensureMsdfDataReady() {
    if (m_msdfAtlasData && m_msdfAtlasData->isLoaded()) return true;
    if (m_msdfDataInitAttempted) return false;

    m_msdfDataInitAttempted = true;

    static const std::vector<std::string> searchPaths = {
        "/data/storage/el2/base/haps/entry/files/rawfile/",
        "/data/storage/el2/base/files/rawfile/"
    };
    for (const auto& dir : searchPaths) {
        if (loadMsdfDataFromJson(dir + "msdf_atlas.json")) {
            return true;
        }
    }

    LOGW("TextLayoutEngine: MSDF data not found");
    return false;
}

void TextLayoutEngine::ensureGlyphsInAtlas(const std::vector<Glyph>& glyphs, uint32_t fontSize) {
    if (!m_textAtlas) return;

    auto t0 = std::chrono::high_resolution_clock::now();
    bool atlasModified = false;

    // Include font ID in atlas key so glyphs from different fonts
    // (e.g. "default" vs "bold") don't collide for the same codepoint+size.
    const std::string& activeFont = m_fontManager->getActiveFontId();
    uint32_t fontHash = static_cast<uint32_t>(std::hash<std::string>{}(activeFont));

    for (size_t i = 0; i < glyphs.size(); ++i) {
        const auto& glyph = glyphs[i];
        if (glyph.bitmapData && glyph.width > 0 && glyph.height > 0) {
            uint64_t key = (static_cast<uint64_t>(glyph.codepoint) << 32) |
                           (static_cast<uint64_t>(fontHash) << 16) | fontSize;
            bool needsAdd = false;
            if (m_glyphsInAtlas.count(key) == 0) {
                needsAdd = true;
            } else {
                uint32_t cachedX = 0, cachedY = 0;
                if (!m_fontManager->getGlyphAtlasPosition(glyph.codepoint, fontSize, cachedX, cachedY) ||
                    (cachedX == 0 && cachedY == 0)) {
                    m_glyphsInAtlas.erase(key);
                    needsAdd = true;
                }
            }
            if (needsAdd) {
                if (m_textAtlas->addGlyph(glyph)) {
                    m_glyphsInAtlas.insert(key);
                    atlasModified = true;
                    m_fontManager->updateGlyphAtlasPosition(glyph.codepoint, fontSize, glyph.atlasX, glyph.atlasY);
                }
            }
        }
    }

    if (atlasModified || m_atlasNeedsUpdate) {
        m_atlasNeedsUpdate = true;  // Signal to TextPipeline that upload is needed
    }

    if (g_perfEnabled) {
        auto dt = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
        if (dt > 1.0f) {
            PERF_LOG("[PERF] ensureGlyphsInAtlas %.2f ms (atlasModified=%d, glyphCount=%zu)", dt, atlasModified, glyphs.size());
        }
    }
}

void TextLayoutEngine::ensureSDFGlyphsInAtlas(const std::vector<const Glyph*>& sdfGlyphs, uint32_t fontSize, uint32_t spread) {
    // Lazily create SDF atlas on first use (glow/stroke path)
    if (!m_sdfAtlas) {
        m_sdfAtlas = std::make_unique<TextAtlas>();
        if (!m_sdfAtlas->initialize(2048, 2048)) {
            LOGE("TextLayoutEngine: Failed to initialize SDF text atlas");
            m_sdfAtlas.reset();
            return;
        }
        m_sdfAtlasNeedsUpdate = true;
    }

    bool atlasModified = false;

    // Include font ID in atlas key so glyphs from different fonts don't collide.
    const std::string& sdfActiveFont = m_fontManager->getActiveFontId();
    uint32_t sdfFontHash = static_cast<uint32_t>(std::hash<std::string>{}(sdfActiveFont));

    for (const Glyph* glyphPtr : sdfGlyphs) {
        if (!glyphPtr) continue;
        const auto& glyph = *glyphPtr;
        if (glyph.bitmapData && glyph.width > 0 && glyph.height > 0) {
            uint64_t key = (static_cast<uint64_t>(glyph.codepoint) << 40) |
                           (static_cast<uint64_t>(sdfFontHash) << 24) |
                           (static_cast<uint64_t>(fontSize) << 16) | spread;
            bool needsAdd = false;
            if (m_sdfGlyphsInAtlas.count(key) == 0) {
                needsAdd = true;
            } else {
                uint32_t cachedX = 0, cachedY = 0;
                if (!m_fontManager->getSDFGlyphAtlasPosition(glyph.codepoint, fontSize, spread, cachedX, cachedY) ||
                    (cachedX == 0 && cachedY == 0)) {
                    m_sdfGlyphsInAtlas.erase(key);
                    needsAdd = true;
                }
            }
            if (needsAdd) {
                if (m_sdfAtlas->addGlyph(glyph)) {
                    m_sdfGlyphsInAtlas.insert(key);
                    atlasModified = true;
                    m_fontManager->updateSDFGlyphAtlasPosition(glyph.codepoint, fontSize, spread, glyph.atlasX, glyph.atlasY);
                }
            }
        }
    }

    if (atlasModified || m_sdfAtlasNeedsUpdate) {
        m_sdfAtlasNeedsUpdate = true;
    }
}

void TextLayoutEngine::precacheText(const std::string& text, uint32_t fontSize) {
    if (!m_fontManager || !m_textAtlas) return;
    const std::vector<Glyph>& glyphs = m_fontManager->rasterizeText(text, fontSize);
    if (glyphs.empty()) return;
    ensureGlyphsInAtlas(glyphs, fontSize);
    LOGI("TextLayoutEngine: precacheText cached %zu glyphs for '%s' (size=%u)",
         glyphs.size(), text.c_str(), fontSize);
}

// ============================================================
// Prepare → Layout → buildDrawData pipeline
// ============================================================

// ---- UTF-8 decoder (shared helper) ----
static std::vector<uint32_t> decodeUtf8(const std::string& text) {
    std::vector<uint32_t> cps;
    cps.reserve(text.size());
    size_t ci = 0;
    while (ci < text.size()) {
        uint32_t cp = 0;
        uint8_t b0 = static_cast<uint8_t>(text[ci]);
        if (b0 < 0x80) { cp = b0; ci += 1; }
        else if ((b0 & 0xE0) == 0xC0) {
            if (ci + 1 >= text.size()) break;
            cp = ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(text[ci+1]) & 0x3F); ci += 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            if (ci + 2 >= text.size()) break;
            cp = ((b0 & 0x0F) << 12) | ((static_cast<uint8_t>(text[ci+1]) & 0x3F) << 6) | (static_cast<uint8_t>(text[ci+2]) & 0x3F); ci += 3;
        } else if ((b0 & 0xF8) == 0xF0) {
            if (ci + 3 >= text.size()) break;
            cp = ((b0 & 0x07) << 18) | ((static_cast<uint8_t>(text[ci+1]) & 0x3F) << 12) | ((static_cast<uint8_t>(text[ci+2]) & 0x3F) << 6) | (static_cast<uint8_t>(text[ci+3]) & 0x3F); ci += 4;
        } else { ci += 1; continue; }
        cps.push_back(cp);
    }
    return cps;
}

std::unique_ptr<PreparedGlyphRun> TextLayoutEngine::prepareText(const std::string& text, uint32_t fontSize) {
    if (!m_fontManager) {
        LOGE("TextLayoutEngine::prepareText: Font manager not initialized");
        return nullptr;
    }

    const bool hasGlow = (m_glowWidth > 0.0f && m_glowIntensity > 0.0f);
    const bool needsSdf = hasGlow || m_strokeWidth > 0.0f;
    std::vector<uint32_t> codepoints = decodeUtf8(text);

    auto run = std::make_unique<PreparedGlyphRun>();
    run->fontSize = fontSize;
    run->glyphs.reserve(codepoints.size());

    // --- Try MSDF path first if glow/stroke is needed ---
    if (needsSdf) {
        ensureMsdfDataReady();
        const MsdfAtlasData* msdfData = getMsdfAtlasData();
        if (msdfData) {
            bool allCovered = !codepoints.empty();
            for (uint32_t cp : codepoints) {
                if (!msdfData->getGlyph(cp)) { allCovered = false; break; }
            }
            if (allCovered) {
                run->renderPath = TextRenderPath::MSDF;
                const float atlasW = static_cast<float>(msdfData->getAtlasWidth());
                const float atlasH = static_cast<float>(msdfData->getAtlasHeight());
                const float ascender = msdfData->getAscender();
                const float fontPx = static_cast<float>(fontSize);

                run->maxBitmapTop = ascender * fontPx;

                for (uint32_t cp : codepoints) {
                    const MsdfGlyph* mg = msdfData->getGlyph(cp);
                    if (!mg) continue;

                    GlyphInfo gi;
                    gi.codepoint = cp;

                    if (!mg->hasBounds) {
                        gi.advance = mg->advance * fontPx;
                        run->glyphs.push_back(gi);
                        continue;
                    }

                    gi.hasBitmap = true;
                    gi.advance = mg->advance * fontPx;
                    gi.left = mg->planeLeft * fontPx;
                    gi.top = mg->planeTop * fontPx;
                    gi.width = (mg->planeRight - mg->planeLeft) * fontPx;
                    gi.height = (mg->planeTop - mg->planeBottom) * fontPx;

                    gi.u0 = mg->atlasLeft / atlasW;
                    gi.u1 = mg->atlasRight / atlasW;
                    gi.v0 = (atlasH - mg->atlasTop) / atlasH;
                    gi.v1 = (atlasH - mg->atlasBottom) / atlasH;

                    run->glyphs.push_back(gi);
                }
                return run;
            }
        }

        // --- Fall back to SDF path ---
        constexpr uint32_t sdfFontSize = SDF_RENDER_FONT_SIZE;
        constexpr uint32_t spread = SDF_SPREAD;
        const float sdfScale = static_cast<float>(fontSize) / static_cast<float>(sdfFontSize);

        run->renderPath = TextRenderPath::SDF;

        std::vector<const Glyph*> sdfGlyphs;
        sdfGlyphs.reserve(codepoints.size());
        for (uint32_t cp : codepoints) {
            sdfGlyphs.push_back(m_fontManager->getSDFGlyph(cp, sdfFontSize, spread));
        }

        ensureSDFGlyphsInAtlas(sdfGlyphs, sdfFontSize, spread);

        const float atlasW = 2048.0f;
        const float atlasH = 2048.0f;
        float maxTop = 0.0f;

        for (size_t i = 0; i < sdfGlyphs.size(); ++i) {
            const Glyph* g = sdfGlyphs[i];
            GlyphInfo gi;
            gi.codepoint = codepoints[i];

            if (!g || !g->bitmapData || g->width == 0 || g->height == 0) {
                if (g) gi.advance = static_cast<float>(g->advance) * sdfScale;
                run->glyphs.push_back(gi);
                continue;
            }

            gi.hasBitmap = true;
            gi.advance = static_cast<float>(g->advance) * sdfScale;
            gi.left = static_cast<float>(g->bitmapLeft) * sdfScale;
            gi.top = static_cast<float>(g->bitmapTop) * sdfScale;
            gi.width = static_cast<float>(g->width) * sdfScale;
            gi.height = static_cast<float>(g->height) * sdfScale;

            float scaledTop = static_cast<float>(g->bitmapTop) * sdfScale;
            if (scaledTop > maxTop) maxTop = scaledTop;

            uint32_t atlasX = 0, atlasY = 0;
            if (!m_fontManager->getSDFGlyphAtlasPosition(g->codepoint, sdfFontSize, spread, atlasX, atlasY)) {
                atlasX = g->atlasX;
                atlasY = g->atlasY;
            }

            gi.u0 = static_cast<float>(atlasX) / atlasW;
            gi.v0 = static_cast<float>(atlasY) / atlasH;
            gi.u1 = static_cast<float>(atlasX + g->width) / atlasW;
            gi.v1 = static_cast<float>(atlasY + g->height) / atlasH;

            run->glyphs.push_back(gi);
        }
        run->maxBitmapTop = maxTop;
        return run;
    }

    // --- Bitmap path (default) ---
    run->renderPath = TextRenderPath::Bitmap;

    const std::vector<Glyph>& glyphs = m_fontManager->rasterizeText(text, fontSize);
    if (glyphs.empty()) {
        LOGW("TextLayoutEngine::prepareText: No glyphs for text='%s'", text.c_str());
        return run;
    }

    ensureGlyphsInAtlas(glyphs, fontSize);

    const float atlasW = 2048.0f;
    const float atlasH = 2048.0f;
    float maxTop = 0.0f;

    for (const auto& glyph : glyphs) {
        GlyphInfo gi;
        gi.codepoint = glyph.codepoint;

        float bitmapTopF = static_cast<float>(glyph.bitmapTop);
        if (bitmapTopF > maxTop) maxTop = bitmapTopF;

        if (!glyph.bitmapData || glyph.width == 0 || glyph.height == 0) {
            gi.advance = static_cast<float>(glyph.advance);
            run->glyphs.push_back(gi);
            continue;
        }

        gi.hasBitmap = true;
        gi.advance = static_cast<float>(glyph.advance);
        gi.left = static_cast<float>(glyph.bitmapLeft);
        gi.top = bitmapTopF;
        gi.width = static_cast<float>(glyph.width);
        gi.height = static_cast<float>(glyph.height);

        uint32_t atlasX = 0, atlasY = 0;
        if (!m_fontManager->getGlyphAtlasPosition(glyph.codepoint, fontSize, atlasX, atlasY)) {
            atlasX = glyph.atlasX;
            atlasY = glyph.atlasY;
        }

        gi.u0 = static_cast<float>(atlasX) / atlasW;
        gi.v0 = static_cast<float>(atlasY) / atlasH;
        gi.u1 = static_cast<float>(atlasX + glyph.width) / atlasW;
        gi.v1 = static_cast<float>(atlasY + glyph.height) / atlasH;

        run->glyphs.push_back(gi);
    }
    run->maxBitmapTop = maxTop;
    return run;
}

TextLayoutResult TextLayoutEngine::layoutSingleLine(const PreparedGlyphRun& run) {
    TextLayoutResult result;
    if (run.glyphs.empty()) return result;

    result.glyphs.reserve(run.glyphs.size());

    float baselineY = run.maxBitmapTop;
    float penX = 0.0f;
    uint32_t prevCp = 0;
    bool prevWasSpace = false;

    for (const auto& gi : run.glyphs) {
        if (prevCp != 0) {
            penX += m_fontManager->getKerning(prevCp, gi.codepoint, run.fontSize);
        }

        if (!gi.hasBitmap) {
            penX += gi.advance;
            prevCp = gi.codepoint;
            // Mark that the next visible glyph follows a space-like character
            if (gi.codepoint == ' ' || gi.codepoint == '\t') {
                prevWasSpace = true;
            }
            continue;
        }

        float x = roundf(penX + gi.left);
        float y = roundf(baselineY - gi.top);

        PositionedGlyph pg;
        pg.x0 = x;
        pg.y0 = y;
        pg.x1 = x + gi.width;
        pg.y1 = y + gi.height;
        pg.u0 = gi.u0;
        pg.v0 = gi.v0;
        pg.u1 = gi.u1;
        pg.v1 = gi.v1;
        pg.afterSpace = prevWasSpace;
        prevWasSpace = false;

        result.glyphs.push_back(pg);
        penX += gi.advance;
        prevCp = gi.codepoint;
    }

    result.totalWidth = penX;
    result.totalHeight = baselineY * 2.0f;
    return result;
}

TextLayoutResult TextLayoutEngine::layoutMultiLine(
    const PreparedTextWithSegments& prepared,
    const LayoutLinesResult& layoutResult,
    uint32_t fontSize,
    float lineHeight) {
    TextLayoutResult result;
    if (layoutResult.lines.empty()) return result;

    size_t estimatedGlyphs = 0;
    for (const auto& line : layoutResult.lines) {
        estimatedGlyphs += (line.end.segmentIndex - line.start.segmentIndex) * 5;
    }
    result.glyphs.reserve(estimatedGlyphs);

    for (size_t lineIdx = 0; lineIdx < layoutResult.lines.size(); lineIdx++) {
        const LayoutLine& line = layoutResult.lines[lineIdx];
        float lineOffsetY = static_cast<float>(lineIdx) * lineHeight;
        float currentX = 0.0f;

        for (int32_t segIdx = line.start.segmentIndex; segIdx < line.end.segmentIndex; segIdx++) {
            if (segIdx < 0 || segIdx >= static_cast<int32_t>(prepared.segments.size())) continue;

            SegmentBreakKind kind = prepared.kinds[segIdx];

            if (kind == SegmentBreakKind::SoftHyphen || kind == SegmentBreakKind::HardBreak) {
                continue;
            }
            if (kind == SegmentBreakKind::Space || kind == SegmentBreakKind::PreservedSpace ||
                kind == SegmentBreakKind::Tab || kind == SegmentBreakKind::Glue) {
                if (segIdx < static_cast<int32_t>(prepared.widths.size())) {
                    currentX += prepared.widths[segIdx];
                }
                continue;
            }

            const std::u32string& segText = prepared.segments[segIdx];
            int32_t startG = (segIdx == line.start.segmentIndex) ? line.start.graphemeIndex : 0;
            int32_t endG = (segIdx == line.end.segmentIndex)
                ? line.end.graphemeIndex
                : static_cast<int32_t>(segText.size());
            if (startG >= endG) continue;

            std::u32string visibleText = segText.substr(startG, endG - startG);

            char32_t prevCp = 0;
            for (char32_t cp : visibleText) {
                const Glyph* glyph = m_fontManager->getGlyph(static_cast<uint32_t>(cp), fontSize);
                if (!glyph || !glyph->bitmapData || glyph->width == 0 || glyph->height == 0) {
                    if (glyph) currentX += static_cast<float>(glyph->advance);
                    prevCp = cp;
                    continue;
                }

                if (prevCp != 0) {
                    currentX += m_fontManager->getKerning(static_cast<uint32_t>(prevCp),
                                                          static_cast<uint32_t>(cp), fontSize);
                }

                ensureGlyphsInAtlas({*glyph}, fontSize);

                uint32_t atlasX = 0, atlasY = 0;
                if (!m_fontManager->getGlyphAtlasPosition(glyph->codepoint, fontSize, atlasX, atlasY)) {
                    atlasX = glyph->atlasX;
                    atlasY = glyph->atlasY;
                }

                float glyphX = roundf(currentX + static_cast<float>(glyph->bitmapLeft));
                float glyphY = roundf(lineOffsetY + static_cast<float>(fontSize) - static_cast<float>(glyph->bitmapTop));

                const float atlasW = 2048.0f;
                const float atlasH = 2048.0f;

                PositionedGlyph pg;
                pg.x0 = glyphX;
                pg.y0 = glyphY;
                pg.x1 = glyphX + static_cast<float>(glyph->width);
                pg.y1 = glyphY + static_cast<float>(glyph->height);
                pg.u0 = static_cast<float>(atlasX) / atlasW;
                pg.v0 = static_cast<float>(atlasY) / atlasH;
                pg.u1 = static_cast<float>(atlasX + glyph->width) / atlasW;
                pg.v1 = static_cast<float>(atlasY + glyph->height) / atlasH;

                result.glyphs.push_back(pg);
                currentX += static_cast<float>(glyph->advance);
                prevCp = cp;
            }
        }
    }

    result.totalHeight = static_cast<float>(layoutResult.lines.size()) * lineHeight;
    result.totalWidth = 0.0f;
    return result;
}

// ============================================================
// buildDrawData — Pure CPU generation of GPU-ready data
// ============================================================

TextDrawData TextLayoutEngine::buildDrawData(
    const PreparedGlyphRun& run,
    const TextLayoutResult& layout,
    const glm::vec2& ndcPosition,
    const glm::vec3& color) {
    TextDrawData drawData;
    drawData.renderPath = run.renderPath;

    if (layout.glyphs.empty()) return drawData;

    // Normalization factors: pixel space → NDC
    const float normWidth = m_normWidth;
    const float normHeight = m_normHeight;

    // Build projection matrix from NDC position
    float projection[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    projection[12] = ndcPosition.x;
    projection[13] = ndcPosition.y;

    // Push constants: 128 bytes = 32 floats
    float* pc = drawData.pushConstants;
    memcpy(pc, projection, sizeof(projection));
    pc[16] = color.b;   // BGR
    pc[17] = color.g;
    pc[18] = color.r;
    pc[19] = m_hasGradient ? 1.0f : 0.0f;
    pc[20] = m_glowIntensity;
    pc[21] = m_glowWidth;
    pc[22] = m_gradientEndColor.b;
    pc[23] = m_gradientEndColor.g;
    pc[24] = m_gradientEndColor.r;
    pc[25] = m_strokeWidth;
    pc[26] = m_strokeColor.b;
    pc[27] = m_strokeColor.g;
    pc[28] = m_strokeColor.r;
    pc[29] = static_cast<float>(m_gradientDirection);

    // SDF path: adjust glow/stroke width to SDF pixel space
    if (run.renderPath == TextRenderPath::SDF) {
        constexpr uint32_t sdfFontSize = SDF_RENDER_FONT_SIZE;
        const float sdfScale = static_cast<float>(run.fontSize) / static_cast<float>(sdfFontSize);
        pc[21] = m_glowWidth / sdfScale;
        pc[25] = m_strokeWidth / sdfScale;
    }

    // Build vertex data from PositionedGlyphs (pixel → NDC)
    // 4 vertices per glyph × 8 floats per vertex = 32 floats per glyph
    auto& vertices = drawData.vertices;
    auto& indices = drawData.indices;
    vertices.reserve(layout.glyphs.size() * 32);
    indices.reserve(layout.glyphs.size() * 6);

    for (const auto& pg : layout.glyphs) {
        float x0 = pg.x0 * normWidth;
        float y0 = pg.y0 * normHeight;
        float x1 = pg.x1 * normWidth;
        float y1 = pg.y1 * normHeight;

        // TL
        vertices.push_back(x0); vertices.push_back(y0);
        vertices.push_back(pg.u0); vertices.push_back(pg.v0);
        vertices.push_back(0.0f); vertices.push_back(0.0f);
        vertices.push_back(pg.u0); vertices.push_back(pg.v0);
        // TR
        vertices.push_back(x1); vertices.push_back(y0);
        vertices.push_back(pg.u1); vertices.push_back(pg.v0);
        vertices.push_back(1.0f); vertices.push_back(0.0f);
        vertices.push_back(pg.u0); vertices.push_back(pg.v0);
        // BR
        vertices.push_back(x1); vertices.push_back(y1);
        vertices.push_back(pg.u1); vertices.push_back(pg.v1);
        vertices.push_back(1.0f); vertices.push_back(1.0f);
        vertices.push_back(pg.u0); vertices.push_back(pg.v0);
        // BL
        vertices.push_back(x0); vertices.push_back(y1);
        vertices.push_back(pg.u0); vertices.push_back(pg.v1);
        vertices.push_back(0.0f); vertices.push_back(1.0f);
        vertices.push_back(pg.u0); vertices.push_back(pg.v0);

        uint16_t base = static_cast<uint16_t>(vertices.size() / 8 - 4);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 1);
        indices.push_back(base + 0);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }

    return drawData;
}

// ============================================================
// layoutStyledBlocks — Pure CPU layout for multi-styled text
// ============================================================

// --- Internal context struct (file-scoped, not in header) ---
struct StyledLayoutData {
    // Output
    std::unique_ptr<StyledLayoutResult> result;

    // Layout state (mutable)
    float yCursor = 0.0f;
    uint32_t charCursor = 0;
    uint32_t totalGlyphs = 0;
    float scaledLineHeight = 0.0f;

    // Coordinate parameters (immutable after init)
    float designX, designY, designMaxWidth, refW;
    float maxLineWidthPx;
    float scale, normX, normY;
    glm::vec2 ndcPos;
    uint32_t baseFontSize;

    // Font
    FontManager* fm;

    // Deferred collections
    struct QuoteBlockInfo {
        size_t blockIndex;
        size_t glyphStart;
        size_t glyphEnd;
        uint32_t charStart;
        uint32_t charCount;
        int quoteLevel;
        float borderWidth;
        glm::vec3 borderColor;
        float borderOffset;
    };
    std::vector<QuoteBlockInfo> quoteBlockInfos;

    struct InlineCodeSegInfo {
        size_t glyphStart;
        size_t glyphEnd;
        uint32_t startCharIndex;
        uint32_t charCount;
        uint32_t scaledBlockFontSize;
    };
    std::vector<InlineCodeSegInfo> inlineCodeSegs;
};

// --- Shared utility: compute bounding box of a glyph range ---
void TextLayoutEngine::computeGlyphBoundingBox(
    const std::vector<PositionedGlyph>& glyphs,
    size_t start, size_t end,
    float& minX, float& maxX,
    float& minY, float& maxY) {
    minX = glyphs[start].x0;
    maxX = glyphs[start].x1;
    minY = glyphs[start].y0;
    maxY = glyphs[start].y1;
    for (size_t gi = start; gi < end; gi++) {
        const auto& g = glyphs[gi];
        if (g.x0 < minX) minX = g.x0;
        if (g.x1 > maxX) maxX = g.x1;
        if (g.y0 < minY) minY = g.y0;
        if (g.y1 > maxY) maxY = g.y1;
    }
}

// --- Shared utility: build push constants for a segment ---
void TextLayoutEngine::buildSegmentPushConstants(
    StyledSegmentInfo& si,
    const glm::vec2& ndcPos, float normY,
    bool italic, const std::vector<PositionedGlyph>& glyphs) {
    float projection[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, ndcPos.x, ndcPos.y, 0, 1
    };

    if (italic && !glyphs.empty()) {
        constexpr float italicShear = 0.25f;
        float baselineY_ndc = glyphs[0].y1 * normY;
        projection[4]  = -italicShear;
        projection[12] = ndcPos.x + italicShear * baselineY_ndc;
    }

    memcpy(si.pushConstants, projection, sizeof(projection));
    si.pushConstants[16] = si.color.b;
    si.pushConstants[17] = si.color.g;
    si.pushConstants[18] = si.color.r;
}

// --- Horizontal Rule ---
void TextLayoutEngine::layoutHorizontalRule(StyledLayoutData& ctx,
                                            const Core::TextBlock& block) {
    ctx.scaledLineHeight = ctx.baseFontSize * ctx.scale * 1.3f;
    float halfLine = ctx.scaledLineHeight * 0.5f;
    ctx.yCursor += halfLine;
    DecorativeRect hr;
    hr.pos = glm::vec2(ctx.designX, ctx.designY + ctx.yCursor / ctx.scale);
    float hrWidth = ctx.designMaxWidth > 0 ? ctx.designMaxWidth : (ctx.refW / ctx.scale);
    hr.size = glm::vec2(hrWidth, 2.0f);
    hr.color = block.hrColor;
    hr.radius = 0.0f;
    ctx.result->decorations.push_back(hr);
    ctx.yCursor += 2.0f + halfLine;
}

// --- Table: measure column widths ---
void TextLayoutEngine::measureTableColumns(StyledLayoutData& ctx,
                                           const std::vector<Core::TextBlock>& blocks,
                                           size_t startIdx, size_t endIdx,
                                           int colCount, float tableFontSize,
                                           std::vector<float>& colMaxWidth) {
    colMaxWidth.assign(colCount, 0.0f);
    for (size_t ri = startIdx; ri <= endIdx; ri++) {
        const auto& row = blocks[ri];
        for (int ci = 0; ci < colCount && ci < static_cast<int>(row.segments.size()); ci++) {
            const auto& seg = row.segments[ci];
            if (seg.text.empty()) continue;
            ctx.fm->setActiveFont(seg.fontId);
            auto prep = prepareText(seg.text, static_cast<uint32_t>(tableFontSize));
            if (!prep) continue;
            auto lay = layoutSingleLine(*prep);
            if (lay.totalWidth > colMaxWidth[ci])
                colMaxWidth[ci] = lay.totalWidth;
        }
    }
}

// --- Table: distribute column widths ---
void TextLayoutEngine::distributeColumnWidths(float tableMaxWidth, int colCount,
                                              const std::vector<float>& colMaxWidth,
                                              float cellPadding,
                                              std::vector<float>& colStartX,
                                              std::vector<float>& colWidths) {
    float totalContentWidth = 0.0f;
    for (float w : colMaxWidth) totalContentWidth += w;
    float totalPadding = cellPadding * 2.0f * static_cast<float>(colCount);

    colStartX.resize(colCount);
    colWidths.resize(colCount);
    if (totalContentWidth > 0.0f) {
        float availWidth = tableMaxWidth - totalPadding;
        float x = 0.0f;
        for (int ci = 0; ci < colCount; ci++) {
            colStartX[ci] = x;
            colWidths[ci] = (colMaxWidth[ci] / totalContentWidth) * availWidth;
            x += colWidths[ci] + totalPadding / static_cast<float>(colCount);
        }
    } else {
        float colWidth = tableMaxWidth / static_cast<float>(colCount);
        for (int ci = 0; ci < colCount; ci++) {
            colStartX[ci] = static_cast<float>(ci) * colWidth;
            colWidths[ci] = colWidth;
        }
    }
}

// --- Table: layout cells ---
void TextLayoutEngine::layoutTableCells(StyledLayoutData& ctx,
                                        const std::vector<Core::TextBlock>& blocks,
                                        size_t startIdx, size_t endIdx,
                                        int colCount, float tableFontSize, float tableLineHeight,
                                        const std::vector<float>& colStartX,
                                        const std::vector<float>& colWidths,
                                        const std::vector<int>& columnAligns,
                                        float cellPadding,
                                        float& tableWidth,
                                        uint32_t& firstRowGlyphCount) {
    tableWidth = 0.0f;
    firstRowGlyphCount = 0;
    uint32_t tableCharStart = ctx.charCursor;

    for (size_t ri = startIdx; ri <= endIdx; ri++) {
        const auto& row = blocks[ri];

        for (int ci = 0; ci < colCount && ci < static_cast<int>(row.segments.size()); ci++) {
            const auto& seg = row.segments[ci];
            if (seg.text.empty()) continue;

            ctx.fm->setActiveFont(seg.fontId);
            auto prep = prepareText(seg.text, static_cast<uint32_t>(tableFontSize));
            if (!prep) continue;

            if (!ctx.result->firstPrepared) {
                ctx.result->firstPrepared = std::make_unique<PreparedGlyphRun>(*prep);
            }

            auto lay = layoutSingleLine(*prep);

            // Compute horizontal alignment offset
            float textW = lay.totalWidth;
            float colW = colWidths[ci];
            int align = (ci < static_cast<int>(columnAligns.size()))
                        ? columnAligns[ci] : 1;
            float colX;
            switch (align) {
            case 2: colX = colStartX[ci] + colW - textW - cellPadding; break;
            case 1: colX = colStartX[ci] + (colW - textW) / 2.0f; break;
            case 0:
            default: colX = colStartX[ci] + cellPadding; break;
            }

            // Vertical centering
            float textH = 0.0f;
            if (!lay.glyphs.empty()) {
                float minGY = lay.glyphs[0].y0;
                float maxGY = lay.glyphs[0].y1;
                for (const auto& g : lay.glyphs) {
                    if (g.y0 < minGY) minGY = g.y0;
                    if (g.y1 > maxGY) maxGY = g.y1;
                }
                textH = maxGY - minGY;
            }
            float vOffset = (tableLineHeight - textH) / 2.0f;

            for (auto& g : lay.glyphs) {
                g.x0 += colX;
                g.x1 += colX;
                g.y0 += ctx.yCursor + vOffset;
                g.y1 += ctx.yCursor + vOffset;
            }

            // Record segment info
            StyledSegmentInfo si;
            si.startCharIndex = ctx.charCursor;
            si.charCount = static_cast<uint32_t>(lay.glyphs.size());
            si.indexOffset = static_cast<uint32_t>(ctx.result->combinedLayout->glyphs.size()) * 6;
            si.indexCount = static_cast<uint32_t>(lay.glyphs.size()) * 6;
            si.color = seg.color;
            si.renderPath = prep->renderPath;
            si.fontId = seg.fontId;

            buildSegmentPushConstants(si, ctx.ndcPos, ctx.normY, false, lay.glyphs);
            ctx.result->segments.push_back(si);

            for (auto& g : lay.glyphs)
                ctx.result->combinedLayout->glyphs.push_back(g);

            ctx.totalGlyphs += static_cast<uint32_t>(lay.glyphs.size());
            ctx.charCursor += static_cast<uint32_t>(lay.glyphs.size());

            if (!lay.glyphs.empty()) {
                float rightEdge = lay.glyphs.back().x1 + cellPadding;
                if (rightEdge > tableWidth) tableWidth = rightEdge;
            }
        }

        ctx.yCursor += tableLineHeight;

        if (ri == startIdx) {
            firstRowGlyphCount = ctx.charCursor - tableCharStart;
        }
    }
}

// --- Table: generate decorative grid lines, header bg ---
void TextLayoutEngine::generateTableDecorations(StyledLayoutData& ctx,
                                                int colCount, float tableLineHeight,
                                                const std::vector<float>& colStartX,
                                                const std::vector<float>& colWidths,
                                                float cellPadding,
                                                float actualTableWidth,
                                                float tableYStart, uint32_t tableCharStart,
                                                uint32_t firstRowGlyphCount, uint32_t tableTotalGlyphs) {
    float gridLineThickness = 1.0f;
    glm::vec3 gridColor{0.4f, 0.4f, 0.4f};
    glm::vec3 headerBgColor{0.76f, 0.76f, 0.78f};

    // Compute column right edges for vertical lines
    float totalPadding = cellPadding * 2.0f * static_cast<float>(colCount);
    std::vector<float> colRightEdge(colCount);
    for (int ci = 0; ci < colCount; ci++) {
        colRightEdge[ci] = colStartX[ci] + colWidths[ci]
            + totalPadding / static_cast<float>(colCount);
    }

    // Table top border
    {
        DecorativeRect r;
        r.pos = glm::vec2(ctx.designX, ctx.designY + tableYStart / ctx.scale);
        r.size = glm::vec2(actualTableWidth / ctx.scale, gridLineThickness);
        r.color = gridColor;
        r.radius = 0.0f;
        r.startCharIndex = tableCharStart;
        r.charCount = firstRowGlyphCount;
        ctx.result->decorations.push_back(r);
    }

    // Header background
    {
        DecorativeRect bg;
        bg.pos = glm::vec2(ctx.designX, ctx.designY + tableYStart / ctx.scale);
        bg.size = glm::vec2(actualTableWidth / ctx.scale, tableLineHeight / ctx.scale);
        bg.color = headerBgColor;
        bg.radius = 0.0f;
        bg.startCharIndex = tableCharStart;
        bg.charCount = firstRowGlyphCount;
        ctx.result->decorations.push_back(bg);
    }

    // Header/data separator
    {
        float sepY = tableYStart + tableLineHeight;
        DecorativeRect r;
        r.pos = glm::vec2(ctx.designX, ctx.designY + sepY / ctx.scale);
        r.size = glm::vec2(actualTableWidth / ctx.scale, gridLineThickness);
        r.color = gridColor;
        r.radius = 0.0f;
        r.startCharIndex = tableCharStart;
        r.charCount = firstRowGlyphCount;
        ctx.result->decorations.push_back(r);
    }

    // Table bottom border
    {
        DecorativeRect r;
        r.pos = glm::vec2(ctx.designX, ctx.designY + ctx.yCursor / ctx.scale);
        r.size = glm::vec2(actualTableWidth / ctx.scale, gridLineThickness);
        r.color = gridColor;
        r.radius = 0.0f;
        r.startCharIndex = tableCharStart;
        r.charCount = tableTotalGlyphs;
        ctx.result->decorations.push_back(r);
    }

    // Vertical column separators
    for (int ci = 0; ci + 1 < colCount; ci++) {
        float vx = colRightEdge[ci];
        DecorativeRect r;
        r.pos = glm::vec2(ctx.designX + vx / ctx.scale, ctx.designY + tableYStart / ctx.scale);
        r.size = glm::vec2(gridLineThickness, (ctx.yCursor - tableYStart) / ctx.scale);
        r.color = gridColor;
        r.radius = 0.0f;
        r.startCharIndex = tableCharStart;
        r.charCount = tableTotalGlyphs;
        ctx.result->decorations.push_back(r);
    }
}

// --- Table block: orchestrator ---
void TextLayoutEngine::layoutTableBlock(StyledLayoutData& ctx,
                                        const std::vector<Core::TextBlock>& blocks,
                                        size_t startIdx, size_t endIdx) {
    int colCount = blocks[startIdx].tableColumnCount;
    float tableFontSize = static_cast<float>(ctx.baseFontSize) * ctx.scale;
    float tableLineHeight = tableFontSize * 1.3f;
    float tableMaxWidth = ctx.maxLineWidthPx > 0.0f ? ctx.maxLineWidthPx : 1000.0f * ctx.scale;
    float cellPadding = 8.0f * ctx.scale;

    // Pass 1: measure columns
    std::vector<float> colMaxWidth;
    measureTableColumns(ctx, blocks, startIdx, endIdx, colCount, tableFontSize, colMaxWidth);

    // Pass 2: distribute widths
    std::vector<float> colStartX, colWidths;
    distributeColumnWidths(tableMaxWidth, colCount, colMaxWidth, cellPadding, colStartX, colWidths);

    // Column alignment from first row
    const auto& columnAligns = blocks[startIdx].columnAligns;

    // Pass 3: layout cells
    float tableYStart = ctx.yCursor;
    uint32_t tableCharStart = ctx.charCursor;
    float tableWidth = 0.0f;
    uint32_t firstRowGlyphCount = 0;

    layoutTableCells(ctx, blocks, startIdx, endIdx,
                     colCount, tableFontSize, tableLineHeight,
                     colStartX, colWidths, columnAligns, cellPadding,
                     tableWidth, firstRowGlyphCount);

    // Pass 4: decorations
    float totalPadding = cellPadding * 2.0f * static_cast<float>(colCount);
    std::vector<float> colRightEdge(colCount);
    for (int ci = 0; ci < colCount; ci++) {
        colRightEdge[ci] = colStartX[ci] + colWidths[ci]
            + totalPadding / static_cast<float>(colCount);
    }
    float actualTableWidth = colRightEdge.empty() ? tableWidth : colRightEdge.back();
    uint32_t tableTotalGlyphs = ctx.charCursor - tableCharStart;

    generateTableDecorations(ctx, colCount, tableLineHeight,
                             colStartX, colWidths, cellPadding,
                             actualTableWidth, tableYStart, tableCharStart,
                             firstRowGlyphCount, tableTotalGlyphs);
}

// --- Generic block: auto line wrapping ---
void TextLayoutEngine::wrapBlockGlyphs(StyledLayoutData& ctx,
                                       size_t blockGlyphStart,
                                       uint32_t scaledBlockFontSize) {
    auto& glyphs = ctx.result->combinedLayout->glyphs;
    if (blockGlyphStart >= glyphs.size()) {
        ctx.yCursor += ctx.scaledLineHeight;
        return;
    }

    std::vector<PositionedGlyph> blockGlyphs(
        glyphs.begin() + blockGlyphStart, glyphs.end());

    bool needsWrap = false;
    if (ctx.maxLineWidthPx > 0.0f && !blockGlyphs.empty()) {
        float minX = blockGlyphs.front().x0;
        float maxX = blockGlyphs.back().x1;
        for (const auto& g : blockGlyphs) {
            if (g.x0 < minX) minX = g.x0;
            if (g.x1 > maxX) maxX = g.x1;
        }
        needsWrap = (maxX - minX) > ctx.maxLineWidthPx;
    }

    if (needsWrap) {
        float yBlockStart = blockGlyphs.front().y0;
        std::vector<PositionedGlyph> wrapped;
        float curY = yBlockStart;
        size_t i = 0;
        while (i < blockGlyphs.size()) {
            float lineX0 = blockGlyphs[i].x0;
            size_t splitEnd = blockGlyphs.size();
            for (size_t j = i; j < blockGlyphs.size(); j++) {
                float relX1 = blockGlyphs[j].x1 - lineX0;
                if (relX1 > ctx.maxLineWidthPx && j > i) {
                    splitEnd = j;
                    break;
                }
            }

            // Word-boundary wrapping
            if (splitEnd < blockGlyphs.size() && splitEnd > i) {
                for (size_t k = splitEnd - 1; k > i; k--) {
                    if (blockGlyphs[k].afterSpace) {
                        splitEnd = k;
                        break;
                    }
                }
            }

            float xOffset = blockGlyphs[i].x0;
            for (size_t j = i; j < splitEnd; j++) {
                PositionedGlyph g = blockGlyphs[j];
                g.x0 -= xOffset;
                g.x1 -= xOffset;
                g.y0 = curY + (blockGlyphs[j].y0 - yBlockStart);
                g.y1 = curY + (blockGlyphs[j].y1 - yBlockStart);
                wrapped.push_back(g);
            }
            curY += ctx.scaledLineHeight;
            i = splitEnd;
        }
        glyphs.erase(glyphs.begin() + blockGlyphStart, glyphs.end());
        for (auto& g : wrapped) glyphs.push_back(g);
        ctx.yCursor = curY;
    } else {
        ctx.yCursor += ctx.scaledLineHeight;
    }
}

// --- Generic block: left indent ---
void TextLayoutEngine::applyBlockIndent(StyledLayoutData& ctx,
                                        const Core::TextBlock& block,
                                        size_t blockGlyphStart) {
    if (block.leftIndent > 0.0f) {
        float indentPx = block.leftIndent * ctx.scale;
        auto& glyphs = ctx.result->combinedLayout->glyphs;
        for (size_t gi = blockGlyphStart; gi < glyphs.size(); gi++) {
            glyphs[gi].x0 += indentPx;
            glyphs[gi].x1 += indentPx;
        }
    }
}

// --- Generic block: inline code backgrounds ---
void TextLayoutEngine::generateInlineCodeBackgrounds(StyledLayoutData& ctx) {
    const auto& glyphs = ctx.result->combinedLayout->glyphs;
    for (const auto& ci : ctx.inlineCodeSegs) {
        if (ci.glyphStart >= ci.glyphEnd) continue;
        if (ci.glyphStart >= glyphs.size()) continue;
        size_t endIdx = std::min(ci.glyphEnd, glyphs.size());

        float minX, maxX, minY, maxY;
        computeGlyphBoundingBox(glyphs, ci.glyphStart, endIdx, minX, maxX, minY, maxY);

        float padX = 2.0f * ctx.scale;
        float padY = 2.0f * ctx.scale;
        DecorativeRect codeBg;
        codeBg.pos = glm::vec2(
            ctx.designX + (minX - padX) / ctx.scale,
            ctx.designY + (minY - padY) / ctx.scale);
        codeBg.size = glm::vec2(
            (maxX - minX + padX * 2) / ctx.scale,
            (maxY - minY + padY * 2) / ctx.scale);
        codeBg.color = glm::vec3(0.76f, 0.76f, 0.78f);
        codeBg.radius = 3.0f;
        codeBg.startCharIndex = ci.startCharIndex;
        codeBg.charCount = ci.charCount;
        ctx.result->decorations.push_back(codeBg);
    }
    ctx.inlineCodeSegs.clear();
}

// --- Generic block: code block background ---
void TextLayoutEngine::generateCodeBlockBackground(StyledLayoutData& ctx,
                                                   const Core::TextBlock& block,
                                                   size_t blockGlyphStart,
                                                   uint32_t blockCharStart) {
    const auto& glyphs = ctx.result->combinedLayout->glyphs;
    if (!block.hasBackground || blockGlyphStart >= glyphs.size()) return;

    float minX, maxX, minY, maxY;
    computeGlyphBoundingBox(glyphs, blockGlyphStart, glyphs.size(), minX, maxX, minY, maxY);

    float pad = block.bgPadding * ctx.scale;
    DecorativeRect bg;
    bg.pos = glm::vec2(
        ctx.designX + (minX - pad) / ctx.scale,
        ctx.designY + (minY - pad) / ctx.scale);
    bg.size = glm::vec2(
        (maxX - minX + pad * 2) / ctx.scale,
        (maxY - minY + pad * 2) / ctx.scale);
    bg.color = block.bgColor;
    bg.radius = block.bgRadius;
    bg.startCharIndex = blockCharStart;
    bg.charCount = static_cast<uint32_t>(glyphs.size()) - blockGlyphStart;
    ctx.result->decorations.push_back(bg);
}

// --- Generic block: underline decoration ---
void TextLayoutEngine::generateUnderline(StyledLayoutData& ctx,
                                         const Core::TextBlock& block,
                                         size_t blockGlyphStart,
                                         uint32_t blockCharStart,
                                         uint32_t scaledBlockFontSize) {
    const auto& glyphs = ctx.result->combinedLayout->glyphs;
    if (!block.hasUnderline || blockGlyphStart >= glyphs.size()) return;

    float minX = glyphs[blockGlyphStart].x0;
    float maxX = glyphs[blockGlyphStart].x1;
    float maxY = glyphs[blockGlyphStart].y1;
    for (size_t gi = blockGlyphStart; gi < glyphs.size(); gi++) {
        const auto& g = glyphs[gi];
        if (g.x0 < minX) minX = g.x0;
        if (g.x1 > maxX) maxX = g.x1;
        if (g.y1 > maxY) maxY = g.y1;
    }
    float gapAbove = scaledBlockFontSize * 0.12f;
    float gapBelow = scaledBlockFontSize * 0.20f;
    float ulY = maxY + gapAbove;
    uint32_t blockGlyphCount = static_cast<uint32_t>(glyphs.size()) - blockGlyphStart;
    DecorativeRect ul;
    ul.pos = glm::vec2(ctx.designX + minX / ctx.scale, ctx.designY + ulY / ctx.scale);
    ul.size = glm::vec2((maxX - minX) / ctx.scale, block.underlineHeight);
    ul.color = block.underlineColor;
    ul.radius = 0.0f;
    ul.startCharIndex = blockCharStart;
    ul.charCount = blockGlyphCount;
    ctx.result->decorations.push_back(ul);
    ctx.yCursor += gapAbove + block.underlineHeight * ctx.scale + gapBelow;
}

// --- Generic block: full pipeline ---
void TextLayoutEngine::layoutGenericBlock(StyledLayoutData& ctx,
                                          const Core::TextBlock& block,
                                          size_t blockIdx) {
    uint32_t scaledBlockFontSize = static_cast<uint32_t>(ctx.baseFontSize * block.fontSizeScale * ctx.scale);
    ctx.scaledLineHeight = scaledBlockFontSize * 1.3f;

    float xCursor = 0.0f;
    uint32_t blockGlyphStart = static_cast<uint32_t>(ctx.result->combinedLayout->glyphs.size());
    uint32_t blockCharStart = ctx.charCursor;

    // Pass 1: prepare all segments, collect maxBitmapTop for common baseline
    struct SegPrep {
        const Core::TextSegment* seg;
        std::unique_ptr<PreparedGlyphRun> prepared;
        float maxBitmapTop;
    };
    std::vector<SegPrep> segPreps;
    float commonBaseline = 0.0f;

    for (const auto& segment : block.segments) {
        if (segment.text.empty()) continue;
        ctx.fm->setActiveFont(segment.fontId);
        auto prepared = prepareText(segment.text, scaledBlockFontSize);
        if (!prepared) continue;

        if (!ctx.result->firstPrepared) {
            ctx.result->firstPrepared = std::move(prepared);
            prepared = prepareText(segment.text, scaledBlockFontSize);
            if (!prepared) continue;
        }

        float mbt = prepared->maxBitmapTop;
        if (mbt > commonBaseline) commonBaseline = mbt;
        segPreps.push_back({&segment, std::move(prepared), mbt});
    }

    // Pass 2: layout with common baseline
    for (auto& sp : segPreps) {
        const auto& segment = *sp.seg;
        auto layout = layoutSingleLine(*sp.prepared);

        float yAdjust = commonBaseline - sp.maxBitmapTop;

        for (auto& g : layout.glyphs) {
            g.x0 += xCursor;
            g.x1 += xCursor;
            g.y0 += ctx.yCursor + yAdjust;
            g.y1 += ctx.yCursor + yAdjust;
        }

        // Record segment info
        StyledSegmentInfo si;
        si.startCharIndex = ctx.charCursor;
        si.charCount = static_cast<uint32_t>(layout.glyphs.size());
        si.indexOffset = static_cast<uint32_t>(ctx.result->combinedLayout->glyphs.size()) * 6;
        si.indexCount = static_cast<uint32_t>(layout.glyphs.size()) * 6;
        si.color = segment.color;
        si.renderPath = sp.prepared->renderPath;
        si.fontId = segment.fontId;

        buildSegmentPushConstants(si, ctx.ndcPos, ctx.normY, segment.italic, layout.glyphs);
        ctx.result->segments.push_back(si);

        size_t segGlyphStart = ctx.result->combinedLayout->glyphs.size();
        for (auto& g : layout.glyphs) ctx.result->combinedLayout->glyphs.push_back(g);
        size_t segGlyphEnd = ctx.result->combinedLayout->glyphs.size();

        // Track inline code segments for deferred background generation
        if (segment.fontId == "mono" && segGlyphStart < segGlyphEnd && !block.hasBackground) {
            StyledLayoutData::InlineCodeSegInfo ci;
            ci.glyphStart = segGlyphStart;
            ci.glyphEnd = segGlyphEnd;
            ci.startCharIndex = ctx.charCursor;
            ci.charCount = static_cast<uint32_t>(segGlyphEnd - segGlyphStart);
            ci.scaledBlockFontSize = scaledBlockFontSize;
            ctx.inlineCodeSegs.push_back(ci);
        }

        xCursor += layout.glyphs.empty() ? 0 : layout.totalWidth;
        ctx.totalGlyphs += static_cast<uint32_t>(layout.glyphs.size());
        ctx.charCursor += static_cast<uint32_t>(layout.glyphs.size());
    }

    // Sub-steps: wrap → indent → inline code bg → code block bg → underline
    wrapBlockGlyphs(ctx, blockGlyphStart, scaledBlockFontSize);
    applyBlockIndent(ctx, block, blockGlyphStart);
    generateInlineCodeBackgrounds(ctx);
    generateCodeBlockBackground(ctx, block, blockGlyphStart, blockCharStart);
    generateUnderline(ctx, block, blockGlyphStart, blockCharStart, scaledBlockFontSize);

    // Collect blockquote border info for deferred rendering
    if (block.hasLeftBorder) {
        size_t glyphEnd = ctx.result->combinedLayout->glyphs.size();
        if (blockGlyphStart < glyphEnd) {
            StyledLayoutData::QuoteBlockInfo qi;
            qi.blockIndex = blockIdx;
            qi.glyphStart = blockGlyphStart;
            qi.glyphEnd = glyphEnd;
            qi.charStart = blockCharStart;
            qi.charCount = ctx.charCursor - blockCharStart;
            qi.quoteLevel = std::max(1, block.quoteLevel);
            qi.borderWidth = block.leftBorderWidth;
            qi.borderColor = block.leftBorderColor;
            qi.borderOffset = block.leftBorderOffset;
            ctx.quoteBlockInfos.push_back(qi);
        }
    }
}

// --- Deferred blockquote border rendering ---
void TextLayoutEngine::flushBlockquoteBorders(StyledLayoutData& ctx,
                                              const std::vector<Core::TextBlock>& blocks) {
    const auto& quoteBlockInfos = ctx.quoteBlockInfos;
    const auto& glyphs = ctx.result->combinedLayout->glyphs;

    for (size_t qi = 0; qi < quoteBlockInfos.size(); ) {
        // Find group
        size_t groupEnd = qi + 1;
        while (groupEnd < quoteBlockInfos.size()) {
            size_t prevIdx = quoteBlockInfos[groupEnd - 1].blockIndex;
            size_t nextIdx = quoteBlockInfos[groupEnd].blockIndex;
            bool allBetweenAreQuotes = true;
            for (size_t bi = prevIdx + 1; bi < nextIdx; bi++) {
                if (!blocks[bi].hasLeftBorder) {
                    allBetweenAreQuotes = false;
                    break;
                }
            }
            if (allBetweenAreQuotes) {
                groupEnd++;
            } else {
                break;
            }
        }

        // Determine max nesting level in this group
        int maxLevel = 0;
        for (size_t j = qi; j < groupEnd; j++)
            maxLevel = std::max(maxLevel, quoteBlockInfos[j].quoteLevel);

        // For each level, find contiguous runs and draw lines
        for (int lvl = 1; lvl <= maxLevel; lvl++) {
            size_t j = qi;
            while (j < groupEnd) {
                while (j < groupEnd && quoteBlockInfos[j].quoteLevel < lvl) j++;
                if (j >= groupEnd) break;

                size_t runStart = j;
                while (j < groupEnd && quoteBlockInfos[j].quoteLevel >= lvl) j++;
                size_t runEnd = j;

                // Compute Y range from glyphs in this run
                float minY = 1e9f, maxY = -1e9f;
                for (size_t r = runStart; r < runEnd; r++) {
                    for (size_t gi = quoteBlockInfos[r].glyphStart;
                         gi < quoteBlockInfos[r].glyphEnd; gi++) {
                        const auto& g = glyphs[gi];
                        if (g.y0 < minY) minY = g.y0;
                        if (g.y1 > maxY) maxY = g.y1;
                    }
                }

                if (minY < maxY) {
                    float lineX = (quoteBlockInfos[runStart].borderOffset +
                                   (lvl - 1) * 72.0f) * ctx.scale;
                    uint32_t runCharStart = quoteBlockInfos[runStart].charStart;
                    uint32_t runCharEnd = quoteBlockInfos[runEnd - 1].charStart
                                         + quoteBlockInfos[runEnd - 1].charCount;
                    DecorativeRect vl;
                    vl.pos = glm::vec2(ctx.designX + lineX / ctx.scale,
                                        ctx.designY + minY / ctx.scale);
                    vl.size = glm::vec2(quoteBlockInfos[runStart].borderWidth,
                                        (maxY - minY) / ctx.scale);
                    vl.color = quoteBlockInfos[runStart].borderColor;
                    vl.radius = 1.0f;
                    vl.startCharIndex = runCharStart;
                    vl.charCount = runCharEnd - runCharStart;
                    ctx.result->decorations.push_back(vl);
                }
            }
        }

        qi = groupEnd;
    }
}

// --- layoutStyledBlocks: orchestrator ---
std::unique_ptr<StyledLayoutResult> TextLayoutEngine::layoutStyledBlocks(
    const std::vector<Core::TextBlock>& blocks,
    uint32_t baseFontSize,
    float maxWidth,
    const glm::vec2& ndcPos,
    float normX, float normY,
    float scale) {
    OH_HiTrace_CountTraceEx(HITRACE_LEVEL_INFO, "AgenUI:LayoutBlockCount",
                            static_cast<int64_t>(blocks.size()));

    StyledLayoutData ctx;
    ctx.result = std::make_unique<StyledLayoutResult>();
    ctx.result->combinedLayout = std::make_unique<TextLayoutResult>();

    ctx.fm = m_fontManager.get();
    if (!ctx.fm) return std::move(ctx.result);

    ctx.maxLineWidthPx = maxWidth > 0.0f ? maxWidth * scale : 0.0f;
    ctx.scaledLineHeight = baseFontSize * scale * 1.3f;
    ctx.baseFontSize = baseFontSize;
    ctx.scale = scale;
    ctx.normX = normX;
    ctx.normY = normY;
    ctx.ndcPos = ndcPos;
    ctx.designX = (ndcPos.x + 1.0f) / normX;
    ctx.designY = (ndcPos.y + 1.0f) / normY;
    ctx.designMaxWidth = maxWidth > 0.0f ? maxWidth : 0.0f;
    ctx.refW = ctx.designMaxWidth > 0.0f ? ctx.designMaxWidth : 1080.0f;

    for (size_t i = 0; i < blocks.size(); i++) {
        const auto& block = blocks[i];

        if (block.isHorizontalRule) {
            layoutHorizontalRule(ctx, block);
            continue;
        }

        if (block.isTableRow) {
            size_t tableEnd = i;
            while (tableEnd + 1 < blocks.size() && blocks[tableEnd + 1].isTableRow)
                tableEnd++;
            layoutTableBlock(ctx, blocks, i, tableEnd);
            i = tableEnd;
            continue;
        }

        if (block.skipRender) {
            ctx.yCursor += ctx.scaledLineHeight;
            continue;
        }

        layoutGenericBlock(ctx, block, i);
    }

    flushBlockquoteBorders(ctx, blocks);

    ctx.fm->setActiveFont("default");
    ctx.result->combinedLayout->totalHeight = ctx.yCursor;
    ctx.result->totalGlyphs = ctx.totalGlyphs;

    OH_HiTrace_CountTraceEx(HITRACE_LEVEL_INFO, "AgenUI:LayoutTotalGlyphs",
                            static_cast<int64_t>(ctx.totalGlyphs));

    return std::move(ctx.result);
}

// ============================================================
// layoutPlainText — Pure CPU layout for single-style text
// ============================================================

std::unique_ptr<TextLayoutEngine::PlainLayoutResult> TextLayoutEngine::layoutPlainText(
    const std::string& text,
    uint32_t fontSize,
    float maxWidth,
    float scale,
    const std::string& fontId) {
    auto result = std::make_unique<PlainLayoutResult>();

    FontManager* fm = m_fontManager.get();
    if (!fm) return result;

    // Set active font
    if (fontId != "default") {
        fm->setActiveFont(fontId);
    }

    float maxLineWidthPx = maxWidth > 0.0f ? maxWidth * scale : 0.0f;
    float lineHeight = fontSize * 1.3f;

    // Helper: auto-wrap for a single logical line's glyphs.
    auto wrapLine = [&](std::vector<PositionedGlyph>& glyphs,
                        float yCursorStart,
                        size_t* visualLineCount = nullptr) -> std::vector<PositionedGlyph> {
        if (visualLineCount) {
            *visualLineCount = glyphs.empty() ? 0 : 1;
        }
        if (maxLineWidthPx <= 0.0f || glyphs.empty()) {
            for (auto& g : glyphs) {
                g.y0 += yCursorStart;
                g.y1 += yCursorStart;
            }
            return glyphs;
        }

        std::vector<PositionedGlyph> wrapped;
        float curY = yCursorStart;
        size_t lineCount = 0;
        size_t i = 0;

        while (i < glyphs.size()) {
            lineCount++;
            size_t lineStart = i;
            size_t splitEnd = glyphs.size();
            float lineX0 = glyphs[lineStart].x0;

            for (size_t j = i; j < glyphs.size(); j++) {
                float relX1 = glyphs[j].x1 - lineX0;
                if (relX1 > maxLineWidthPx && j > lineStart) {
                    splitEnd = j;
                    break;
                }
            }

            // Word-boundary wrapping: find a glyph marked afterSpace,
            // but only if the break point leaves a reasonable amount of
            // content on the current line (at least 20% of maxWidth).
            // This avoids breaking right after a bullet "• " which would
            // leave the bullet alone on a line.
            if (splitEnd < glyphs.size() && splitEnd > lineStart) {
                for (size_t k = splitEnd - 1; k > lineStart; k--) {
                    if (glyphs[k].afterSpace) {
                        float widthToHere = glyphs[k].x0 - lineX0;
                        if (widthToHere >= maxLineWidthPx * 0.2f) {
                            splitEnd = k;
                        }
                        break;
                    }
                }
            }

            float xOffset = glyphs[lineStart].x0;
            for (size_t j = lineStart; j < splitEnd; j++) {
                PositionedGlyph g = glyphs[j];
                g.x0 -= xOffset;
                g.x1 -= xOffset;
                g.y0 += curY;
                g.y1 += curY;
                wrapped.push_back(g);
            }

            curY += lineHeight;
            i = splitEnd;
        }

        if (visualLineCount) {
            *visualLineCount = lineCount;
        }
        return wrapped;
    };

    bool hasNewlines = text.find('\n') != std::string::npos;

    if (!hasNewlines) {
        // Single line path
        auto prepared = prepareText(text, fontSize);
        if (!prepared) {
            if (fontId != "default") fm->setActiveFont("default");
            return result;
        }
        auto layout = layoutSingleLine(*prepared);

        if (maxLineWidthPx > 0.0f && !layout.glyphs.empty() &&
            layout.glyphs.back().x1 - layout.glyphs.front().x0 > maxLineWidthPx) {
            auto wrappedGlyphs = wrapLine(layout.glyphs, 0.0f);
            layout.glyphs = std::move(wrappedGlyphs);
        }

        result->totalGlyphs = static_cast<uint32_t>(layout.glyphs.size());
        result->prepared = std::move(prepared);
        result->layout = std::make_unique<TextLayoutResult>(std::move(layout));
    } else {
        // Multi-line path: split by '\n', wrap each paragraph independently
        std::unique_ptr<PreparedGlyphRun> firstPrepared;
        auto combinedLayout = std::make_unique<TextLayoutResult>();
        float yCursor = 0.0f;

        size_t lineStart = 0;
        while (lineStart <= text.size()) {
            size_t nlPos = text.find('\n', lineStart);
            std::string line = (nlPos != std::string::npos)
                ? text.substr(lineStart, nlPos - lineStart)
                : text.substr(lineStart);

            if (!line.empty()) {
                auto prepared = prepareText(line, fontSize);
                if (prepared) {
                    auto lineLayout = layoutSingleLine(*prepared);
                    if (!firstPrepared) {
                        firstPrepared = std::move(prepared);
                    }

                    size_t visualLineCount = 0;
                    auto wrappedGlyphs = wrapLine(lineLayout.glyphs, yCursor, &visualLineCount);
                    for (auto& g : wrappedGlyphs) {
                        combinedLayout->glyphs.push_back(g);
                    }
                    combinedLayout->totalWidth = std::max(combinedLayout->totalWidth, lineLayout.totalWidth);

                    // Advance by the number of visual lines produced.
                    // Do not use glyph y1 — it includes font ascender/bitmap height
                    // and would add extra blank space between lines.
                    yCursor += static_cast<float>(std::max<size_t>(visualLineCount, 1)) * lineHeight;
                }
            } else {
                yCursor += lineHeight;
            }

            if (nlPos == std::string::npos) break;
            lineStart = nlPos + 1;
        }

        combinedLayout->totalHeight = yCursor;
        result->totalGlyphs = static_cast<uint32_t>(combinedLayout->glyphs.size());
        result->prepared = std::move(firstPrepared);
        result->layout = std::move(combinedLayout);
    }

    // Reset font
    if (fontId != "default") fm->setActiveFont("default");

    return result;
}

} // namespace AgenUIEngine
