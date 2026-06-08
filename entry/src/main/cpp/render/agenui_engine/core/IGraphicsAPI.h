/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Core Graphics API Abstraction Layer
 * ================================
 * This file defines the abstract interfaces for multi-backend graphics support.
 * Supported backends: Vulkan, OpenGL ES, DirectX 12, Metal
 *
 * Architecture: Application -> RenderContext -> IRenderer (high-level draw primitives)
 *               IRenderer only exposes lifecycle + high-level draw methods.
 *               All low-level GPU operations are handled internally by backends.
 */

#ifndef AGENUI_IGRAPHICS_API_H
#define AGENUI_IGRAPHICS_API_H

#include <memory>
#include <vector>
#include <cstdint>
#include <string>
#include "RenderCommand.h"
#include "logger_common.h"

// Include GLM types (needed for interface methods)
#include "thirdparty/glm/glm.hpp"

namespace AgenUIEngine { class FontManager; }

namespace AgenUIEngine::Core {

/**
 * @brief Supported graphics APIs
 */
enum class GraphicsAPI {
    Vulkan,
    OpenGL_ES,
    DirectX12,
    Metal
};

/**
 * @brief Initialization parameters for renderer
 */
struct RendererInitParams {
    void* nativeWindow = nullptr;  // Platform-specific window handle
    int width = 1024;
    int height = 1024;
    bool enableValidation = false;  // Enable validation/debug layers
    bool enableVSync = true;
};

// Forward declarations
class RenderContext;

// -----------------------------------------------------------------------------
// Styled Text Structures (engine-agnostic, not markdown-specific)
// -----------------------------------------------------------------------------

/**
 * @brief A single text fragment with uniform style
 *
 * Used by prepareTextLayout to describe multi-styled text.
 * The engine does not know where these came from (markdown, HTML, etc.).
 */
struct TextSegment {
    std::string text;
    std::string fontId = "default";   ///< "default" / "bold" / "mono" etc.
    glm::vec3 color{1.0f};           ///< Per-segment color (white = use caller color)
    bool italic = false;             ///< Apply italic shear transform
};

/**
 * @brief A layout block containing one or more styled segments
 *
 * Multiple TextBlocks are laid out vertically with individual line heights.
 */
struct TextBlock {
    std::vector<TextSegment> segments;
    float fontSizeScale = 1.0f;       ///< Relative to baseFontSize (e.g. 2.0 for H1)
    bool skipRender = false;          ///< Skip rendering but reserve vertical space

    // Background rectangle (used for code blocks)
    bool hasBackground = false;
    glm::vec3 bgColor{0.15f, 0.15f, 0.17f};
    float bgPadding = 8.0f;
    float bgRadius = 6.0f;

    // Horizontal rule
    bool isHorizontalRule = false;
    glm::vec3 hrColor{0.6f, 0.6f, 0.6f};

    // Left border decoration (generic — set by MdStyleMapper for blockquote)
    bool hasLeftBorder = false;
    glm::vec3 leftBorderColor{0.5f, 0.5f, 0.5f};
    float leftBorderWidth = 20.0f;      ///< Thickness in design pixels
    float leftBorderOffset = 0.0f;      ///< X offset: 0 = line at original text position
    float leftIndent = 72.0f;           ///< Left indent for text content (design pixels)
    int quoteLevel = 0;                 ///< Blockquote nesting level (0 = not a quote)

    // Underline decoration (generic — MdStyleMapper sets it for H1/H2)
    bool hasUnderline = false;
    float underlineHeight = 1.0f;     ///< Thickness in design pixels
    glm::vec3 underlineColor{0.6f, 0.6f, 0.6f};

    // Table row metadata (set by MdStyleMapper for table rows)
    bool isTableRow = false;
    int tableColumnCount = 0;
    bool isTableHeaderRow = false;
    bool isTableLastRow = false;
    std::vector<uint32_t> segmentToColumnMap;  ///< segment[i] → column index
    std::vector<int> columnAligns;             ///< per-column: 0=left, 1=center, 2=right
};

// -----------------------------------------------------------------------------
// Abstract Resource Interfaces (kept for backend-internal use)
// -----------------------------------------------------------------------------

/**
 * @brief Buffer usage flags
 */
enum class BufferUsage {
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    StorageBuffer,
    StagingBuffer
};

/**
 * @brief Texture formats
 */
enum class TextureFormat {
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,
    R8G8B8_UNORM,
    R8_UNORM,
    R32G32B32A32_SFLOAT,
    D24_UNORM_S8_UINT,  // Depth stencil
    UNKNOWN
};

/**
 * @brief Shader stage
 */
enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEvaluation
};

/**
 * @brief Abstract buffer interface
 */
class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual void* map() = 0;
    virtual void unmap() = 0;
    virtual void update(const void* data, size_t size, size_t offset = 0) = 0;
    virtual size_t getSize() const = 0;
    virtual BufferUsage getUsage() const = 0;
    virtual bool isPersistentlyMapped() const = 0;
    virtual uint64_t getNativeHandle() const = 0;
};

/**
 * @brief Abstract texture interface
 */
class ITexture {
public:
    virtual ~ITexture() = default;
    virtual void update(const void* data, uint32_t x, uint32_t y,
                      uint32_t width, uint32_t height) = 0;
    virtual void generateMipmaps() = 0;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual TextureFormat getFormat() const = 0;
    virtual uint64_t getNativeHandle() const = 0;
};

/**
 * @brief Abstract shader interface
 */
class IShader {
public:
    virtual ~IShader() = default;
    virtual const std::vector<uint32_t>& getByteCode() const = 0;
    virtual ShaderStage getStage() const = 0;
    virtual const char* getEntryPoint() const = 0;
};

// -----------------------------------------------------------------------------
// Abstract Renderer Interface (High-Level Draw Primitives)
// -----------------------------------------------------------------------------

/**
 * @brief Abstract graphics renderer interface
 *
 * This is the backend abstraction layer. It only exposes:
 * - Lifecycle management (initialize, beginFrame, endFrame, cleanup)
 * - High-level draw primitives (drawRect, drawText, drawImage, etc.)
 * - Setup methods (pipelines, fonts, shaders)
 *
 * All NDC conversion, pipeline binding, and GPU operations are handled
 * internally by each backend implementation.
 *
 * Application code should use RenderContext (the facade) rather than
 * IRenderer directly.
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    virtual bool initialize(const RendererInitParams& params) = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void cleanup() = 0;
    virtual bool isInitialized() const = 0;

    // -------------------------------------------------------------------------
    // High-Level Draw Primitives (pixel coordinates in, backend handles rest)
    // -------------------------------------------------------------------------

    virtual void drawRect(const glm::vec2& position, const glm::vec2& size,
                          const glm::vec3& color) = 0;

    virtual void drawRoundedRect(const glm::vec2& position, const glm::vec2& size,
                                 float cornerRadius, const glm::vec3& color, float alpha = 1.0f) = 0;

    virtual void drawGlassRoundedRect(const glm::vec2& position, const glm::vec2& size,
                                       float cornerRadius, const glm::vec3& color) = 0;

    virtual void drawPolygon(const glm::vec2& center, const std::vector<glm::vec2>& vertices,
                             const glm::vec3& color, float alpha = 1.0f) = 0;

    virtual void drawCircle(const glm::vec2& center, float radius,
                            const glm::vec3& color, float alpha = 1.0f) = 0;

    virtual void drawText(const std::string& text, const glm::vec2& position,
                          uint32_t fontSize, const glm::vec3& color,
                          const std::string& fontFamily = "default") = 0;

    virtual void drawMultiLineText(const std::string& text, const glm::vec2& position,
                                   uint32_t fontSize, const glm::vec3& color,
                                   float maxWidth, float lineHeight) = 0;

    /**
     * @brief Set text glow parameters for subsequent drawText calls
     * @param glowWidth Glow radius in pixels (0 = no glow)
     * @param glowIntensity Glow brightness (0-1)
     */
    virtual void setTextGlow(float glowWidth, float glowIntensity) = 0;

    /**
     * @brief Set text gradient parameters for subsequent drawText calls
     * @param hasGradient Whether gradient is enabled
     * @param endColor Gradient end color (RGB)
     * @param direction 0=vertical, 1=horizontal
     */
    virtual void setTextGradient(bool hasGradient, const glm::vec3& endColor, int direction = 0) = 0;

    /**
     * @brief Set text stroke parameters for subsequent drawText calls
     * @param width Stroke width in pixels (0 = no stroke)
     * @param color Stroke color (RGB)
     */
    virtual void setTextStroke(float width, const glm::vec3& color) = 0;

    virtual void drawImage(const std::string& imagePath, const glm::vec2& position,
                           const glm::vec2& size, float rotation = 0.0f,
                           const std::vector<glm::vec2>& clipVertices = {},
                           const glm::vec2& clipCenter = {}) = 0;

    // -------------------------------------------------------------------------
    // Setup
    // -------------------------------------------------------------------------

    virtual void setClearColor(float r, float g, float b, float a) = 0;
    virtual void setResourceManager(void* resourceManager) = 0;
    virtual bool initializeFonts(void* resourceManager, const std::string& fontName) = 0;

    virtual bool createPipelines() = 0;

    // -------------------------------------------------------------------------
    // Batch Execution (Template Method — not overridable)
    // -------------------------------------------------------------------------

    /**
     * @brief Execute a batch of sorted render commands
     *
     * Iterates commands, dispatches via switch to individual drawXxx() virtuals,
     * and calls hooks for lifecycle events (type change, begin, end).
     * Subclasses customize behavior by overriding hooks, NOT this method.
     *
     * @param commands  Pre-sorted render commands (sorted by SortKey)
     * @param stats     Frame statistics to update
     */
    void executeCommandBatch(const std::vector<RenderCommand>& commands, FrameStats& stats) {
        DrawType currentType = DrawType::Custom;
        onBeginBatch();
        for (const auto& cmd : commands) {
            if (cmd.type != currentType) {
                currentType = cmd.type;
                onDrawTypeChange(cmd.type);
            }
            switch (cmd.type) {
            case DrawType::Rectangle:
                drawRect(cmd.position, cmd.size, cmd.color);
                break;
            case DrawType::RoundedRectangle:
                drawRoundedRect(cmd.position, cmd.size, cmd.cornerRadius, cmd.color, cmd.alpha);
                break;
            case DrawType::GlassRoundedRectangle:
                drawGlassRoundedRect(cmd.position, cmd.size, cmd.cornerRadius, cmd.color);
                break;
            case DrawType::Polygon:
                drawPolygon(cmd.polygonCenter, cmd.polygonVertices, cmd.color, cmd.alpha);
                break;
            case DrawType::Circle:
                drawCircle(cmd.position, cmd.circleRadius, cmd.color, cmd.alpha);
                break;
            case DrawType::Text:
                if (cmd.glowWidth > 0.0f) setTextGlow(cmd.glowWidth, cmd.glowIntensity);
                if (cmd.hasGradient) setTextGradient(true, cmd.gradientEndColor, cmd.gradientDirection);
                if (cmd.strokeWidth > 0.0f) setTextStroke(cmd.strokeWidth, cmd.strokeColor);
                drawText(cmd.text, cmd.position, cmd.fontSize, cmd.color, cmd.fontFamily);
                if (cmd.glowWidth > 0.0f) setTextGlow(0.0f, 0.0f);
                if (cmd.hasGradient) setTextGradient(false, glm::vec3(0.0f), 0);
                if (cmd.strokeWidth > 0.0f) setTextStroke(0.0f, glm::vec3(0.0f));
                break;
            case DrawType::MultiLineText:
                drawMultiLineText(cmd.text, cmd.position, cmd.fontSize,
                                  cmd.color, cmd.maxWidth, cmd.lineHeight);
                break;
            case DrawType::TextPartial:
                drawTextPartial(cmd.textPartialKey, cmd.color, cmd.visibleChars);
                break;
            case DrawType::Image:
                drawImage(cmd.imagePath, cmd.position, cmd.size, cmd.rotation,
                          cmd.clipVertices, cmd.clipCenter);
                break;
            default: break;
            }
            stats.drawCalls++;
        }
        stats.commandCount = static_cast<uint32_t>(commands.size());
        onEndBatch(stats);
    }

protected:
    /**
     * @brief Hook: called once at the start of executeCommandBatch
     *
     * Override to reset per-batch state (e.g., pipeline change counters).
     */
    virtual void onBeginBatch() {}

    /**
     * @brief Hook: called when the DrawType changes between consecutive commands
     *
     * After sorting, same-type commands are contiguous. This hook fires on
     * transitions. Override to perform per-type setup (e.g., glass pass
     * preparation, pipeline change tracking).
     *
     * @param newType  The DrawType of the upcoming command(s)
     */
    virtual void onDrawTypeChange(DrawType newType) { (void)newType; }

    /**
     * @brief Hook: called once at the end of executeCommandBatch
     *
     * Override to finalize per-batch statistics.
     *
     * @param stats  Frame statistics (mutable — can write batchedDrawCalls etc.)
     */
    virtual void onEndBatch(FrameStats& stats) { (void)stats; }

public:
    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual GraphicsAPI getAPI() const = 0;
    virtual void waitIdle() = 0;
    virtual void recreateSwapChain() = 0;

    // Update the surface size before recreating the swapchain.
    // Must be called before recreateSwapChain() so that createSwapChain()
    // uses the correct dimensions from OnSurfaceChanged.
    virtual void updateSurfaceSize(int width, int height) = 0;

    // Set the coordinate mapping dimensions used for pixelToNDC conversion.
    // This should match the dimensions the content was originally designed for,
    // not necessarily the current swapchain extent.
    // Defaults to swapchain extent if never called.
    virtual void setCoordinateMapping(int width, int height) = 0;

    // Prepare for glass rendering: copy current framebuffer to a texture
    // that the glass shader can sample for refraction effects.
    // Default no-op for backends that don't support this.
    virtual void prepareGlassPass() {}

    // Access the FontManager for configuration-driven font registration
    virtual AgenUIEngine::FontManager* getFontManager() = 0;

    /**
     * @brief Pre-cache all glyphs for a given text and upload atlas once
     */
    virtual void precacheText(const std::string& text, uint32_t fontSize) {
        (void)text; (void)fontSize;
    }

    // -------------------------------------------------------------------------
    // Stream text support (typewriter effect)
    // -------------------------------------------------------------------------

    /**
     * @brief Prepare text layout and cache it for partial rendering (plain text)
     * @return Total glyph count, or 0 on failure
     */
    virtual uint32_t prepareTextLayout(const std::string& key, const std::string& text,
                                        const glm::vec2& position, uint32_t fontSize,
                                        const std::string& fontFamily = "default",
                                        float maxWidth = 0.0f) {
        (void)key; (void)text; (void)position; (void)fontSize; (void)fontFamily; (void)maxWidth;
        return 0;
    }

    /**
     * @brief Prepare multi-styled text layout and cache it for partial rendering
     *
     * Accepts pre-parsed styled blocks (e.g. from markdown, HTML, etc.).
     * The engine does not know or care about the source format.
     *
     * @param blocks    Pre-parsed styled text blocks
     * @return Total glyph count, or 0 on failure
     */
    virtual uint32_t prepareTextLayout(const std::string& key,
                                        const std::vector<TextBlock>& blocks,
                                        const glm::vec2& position, uint32_t baseFontSize,
                                        float maxWidth = 0.0f) {
        (void)key; (void)blocks; (void)position; (void)baseFontSize; (void)maxWidth;
        return 0;
    }

    /**
     * @brief Draw first N characters from a cached text layout
     */
    virtual void drawTextPartial(const std::string& key, const glm::vec3& color,
                                  uint32_t visibleChars) {
        (void)key; (void)color; (void)visibleChars;
    }

    /**
     * @brief Clear all cached text layouts
     */
    virtual void clearTextLayoutCache() {}
};

// -----------------------------------------------------------------------------
// Render Factory
// -----------------------------------------------------------------------------

/**
 * @brief Factory for creating render context instances
 */
class RenderFactory {
public:
    static std::unique_ptr<RenderContext> createRenderContext(GraphicsAPI api);
    static std::vector<GraphicsAPI> getSupportedAPIs();
    static bool isAPISupported(GraphicsAPI api);
};

} // namespace AgenUIEngine::Core

#endif // AGENUI_IGRAPHICS_API_H
