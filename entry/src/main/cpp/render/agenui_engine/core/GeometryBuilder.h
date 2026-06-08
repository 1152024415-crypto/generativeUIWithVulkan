/*
 * GeometryBuilder — backend-agnostic geometry construction
 * ========================================================
 * Provides CoordinateMapper (pure math, zero GPU dependency) and
 * GeometryBuilder static methods that fill DrawPacket structs for
 * fixed-size primitives (rect, circle, roundedRect) or write into
 * external scratch buffers for variable-size primitives (polygon, image).
 *
 * Performance guarantee:
 *   Fixed-size primitives use stack-allocated DrawPacket — zero heap allocation.
 *   Variable-size primitives write to caller-owned vectors whose .clear()
 *   preserves capacity (zero allocation after the first frame's largest draw).
 */

#ifndef AGENUI_GEOMETRY_BUILDER_H
#define AGENUI_GEOMETRY_BUILDER_H

#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include "thirdparty/glm/glm.hpp"

namespace AgenUIEngine::Core {

// ---------------------------------------------------------------------------
// Vertex format tags — each backend knows how to interpret these
// ---------------------------------------------------------------------------
enum class VertexFormat {
    Position3Color4,  // 7 floats per vertex: x,y,z, r,g,b,a
    Position3UV2,     // 5 floats per vertex: x,y,z, u,v
};

// ---------------------------------------------------------------------------
// DrawPacket — stack-allocated geometry payload
// ---------------------------------------------------------------------------
struct DrawPacket {
    // Inline storage: covers all fixed-size primitives (max 4 verts × 7 floats = 28)
    float verts[40];          // 40 > 28, headroom for safety
    uint16_t indices[6];      // Standard quad indices
    float pushConstants[24];  // Rounded rect maximum

    // External buffer pointers for variable-size primitives (polygon/image).
    // When null, vertexData()/indexData() return the inline arrays.
    float* extVerts = nullptr;
    uint16_t* extIndices = nullptr;

    uint32_t vertFloatCount = 0;   // Number of floats written to vertex data
    uint32_t indexCount = 0;       // Number of indices written
    uint32_t pcFloatCount = 0;     // Number of floats written to pushConstants
    uint32_t vertStride = 0;       // Floats per vertex (5 or 7)

    VertexFormat format = VertexFormat::Position3Color4;
    bool needsIdentityMVP = false;
    bool glass = false;

    const float* vertexData() const { return extVerts ? extVerts : verts; }
    const uint16_t* indexData() const { return extIndices ? extIndices : indices; }
};

// ---------------------------------------------------------------------------
// CoordinateMapper — pure math, zero GPU dependency
// ---------------------------------------------------------------------------
class CoordinateMapper {
    float m_swapW = 0, m_swapH = 0;
    float m_refW = 0, m_refH = 0;

public:
    CoordinateMapper() = default;
    CoordinateMapper(float swapW, float swapH, float refW, float refH)
        : m_swapW(swapW), m_swapH(swapH), m_refW(refW > 0 ? refW : swapW), m_refH(refH > 0 ? refH : swapH) {}

    /** Convert pixel position to NDC. Logic identical to VulkanContext::pixelToNDC. */
    void pixelToNDC(float px, float py, float& ndcX, float& ndcY) const {
        float contentAspect = m_refW / m_refH;
        float screenAspect = m_swapW / m_swapH;
        float scale, offsetX = 0.0f, offsetY = 0.0f;
        if (contentAspect > screenAspect) {
            scale = m_swapW / m_refW;
            offsetY = (m_swapH - m_refH * scale) * 0.5f;
        } else {
            scale = m_swapH / m_refH;
            offsetX = (m_swapW - m_refW * scale) * 0.5f;
        }
        ndcX = ((px * scale + offsetX) / m_swapW) * 2.0f - 1.0f;
        ndcY = ((py * scale + offsetY) / m_swapH) * 2.0f - 1.0f;
    }

    /** Convert pixel size to NDC size. Logic identical to VulkanContext::pixelSizeToNDC. */
    void pixelSizeToNDC(float pw, float ph, float& ndcW, float& ndcH) const {
        float contentAspect = m_refW / m_refH;
        float screenAspect = m_swapW / m_swapH;
        float scale = (contentAspect > screenAspect) ? (m_swapW / m_refW) : (m_swapH / m_refH);
        ndcW = (pw * scale / m_swapW) * 2.0f;
        ndcH = (ph * scale / m_swapH) * 2.0f;
    }

    float swapW() const { return m_swapW; }
    float swapH() const { return m_swapH; }
    float refW() const { return m_refW; }
    float refH() const { return m_refH; }
};

// ---------------------------------------------------------------------------
// GeometryBuilder — static methods for constructing DrawPackets
// ---------------------------------------------------------------------------
class GeometryBuilder {
public:
    // --- Fixed-size primitives (fill inline arrays, zero heap allocation) ---

    static void buildRect(DrawPacket& out, const glm::vec2& pos, const glm::vec2& size,
                          const glm::vec3& color, const CoordinateMapper& m) {
        float ndcX, ndcY, ndcW, ndcH;
        m.pixelToNDC(pos.x, pos.y, ndcX, ndcY);
        m.pixelSizeToNDC(size.x, size.y, ndcW, ndcH);

        // Vertex format: x, y, z, r, g, b, a (7-float stride)
        // Output RGBA order — backend does BGR swap at submit time
        const float v[28] = {
            ndcX,        ndcY,        0.0f,  color.r, color.g, color.b, 1.0f,
            ndcX + ndcW, ndcY,        0.0f,  color.r, color.g, color.b, 1.0f,
            ndcX + ndcW, ndcY + ndcH, 0.0f,  color.r, color.g, color.b, 1.0f,
            ndcX,        ndcY + ndcH, 0.0f,  color.r, color.g, color.b, 1.0f
        };
        memcpy(out.verts, v, sizeof(v));
        const uint16_t idx[6] = {0, 2, 1, 0, 3, 2};
        memcpy(out.indices, idx, sizeof(idx));

        out.vertFloatCount = 28;
        out.indexCount = 6;
        out.pcFloatCount = 0;
        out.vertStride = 7;
        out.format = VertexFormat::Position3Color4;
        out.needsIdentityMVP = false;
        out.glass = false;
    }

    static void buildRoundedRect(DrawPacket& out, const glm::vec2& pos, const glm::vec2& size,
                                  float radius, const glm::vec3& color, float alpha,
                                  bool glass, const CoordinateMapper& m) {
        float ndcX, ndcY, ndcW, ndcH;
        m.pixelToNDC(pos.x, pos.y, ndcX, ndcY);
        m.pixelSizeToNDC(size.x, size.y, ndcW, ndcH);
        float radiusNDC_w, radiusNDC_h;
        m.pixelSizeToNDC(radius, radius, radiusNDC_w, radiusNDC_h);
        float radiusNDC = radiusNDC_w;

        // Push constants — common fields (24 floats)
        float pc[24] = {};
        pc[0] = ndcX;  pc[1] = ndcY;
        pc[2] = ndcW;  pc[3] = ndcH;
        pc[4] = color.r; pc[5] = color.g;
        pc[6] = color.b; pc[7] = alpha;
        pc[8] = radiusNDC;  pc[9] = radiusNDC;
        pc[10] = radiusNDC; pc[11] = radiusNDC;
        pc[12] = 0.0f;
        pc[23] = radiusNDC_h;

        // Glass-specific parameters
        if (glass) {
            pc[13] = 15.0f; pc[14] = 6.0f;
            pc[15] = 0.03f; pc[16] = 0.4f;
            pc[17] = 0.14f; pc[18] = 0.0f;
            pc[19] = 0.06f; pc[20] = 0.35f;
            pc[21] = 1.0f;  pc[22] = 0.0f;
        }
        memcpy(out.pushConstants, pc, sizeof(pc));

        // Geometry — 4 vertices × 5 floats (Position3UV2)
        const float v[20] = {
            ndcX,        ndcY,        0.0f, 0.0f, 0.0f,
            ndcX + ndcW, ndcY,        0.0f, 1.0f, 0.0f,
            ndcX + ndcW, ndcY + ndcH, 0.0f, 1.0f, 1.0f,
            ndcX,        ndcY + ndcH, 0.0f, 0.0f, 1.0f
        };
        memcpy(out.verts, v, sizeof(v));
        const uint16_t idx[6] = {0, 2, 1, 0, 3, 2};
        memcpy(out.indices, idx, sizeof(idx));

        out.vertFloatCount = 20;
        out.indexCount = 6;
        out.pcFloatCount = 24;
        out.vertStride = 5;
        out.format = VertexFormat::Position3UV2;
        out.needsIdentityMVP = true;
        out.glass = glass;
    }

    static void buildCircle(DrawPacket& out, const glm::vec2& center, float radius,
                             const glm::vec3& color, float alpha, const CoordinateMapper& m) {
        // NDC bounds — shader uses center + radius + pad for aspect-ratio correction
        float ndcX, ndcY, ndcW, ndcH;
        m.pixelToNDC(center.x - radius, center.y - radius, ndcX, ndcY);
        m.pixelSizeToNDC(radius * 2.0f, radius * 2.0f, ndcW, ndcH);

        // Push constants: center(2) + radius(1) + pad(1) + color(4) = 8 floats
        float pc[8] = {};
        pc[0] = ndcX;
        pc[1] = ndcY;
        pc[2] = ndcW;
        pc[3] = ndcH;
        pc[4] = color.r;
        pc[5] = color.g;
        pc[6] = color.b;
        pc[7] = alpha;
        memcpy(out.pushConstants, pc, sizeof(pc));

        // Geometry — 4 vertices × 5 floats (Position3UV2)
        const float v[20] = {
            ndcX,        ndcY,        0.0f,  0.0f, 0.0f,
            ndcX + ndcW, ndcY,        0.0f,  1.0f, 0.0f,
            ndcX + ndcW, ndcY + ndcH, 0.0f,  1.0f, 1.0f,
            ndcX,        ndcY + ndcH, 0.0f,  0.0f, 1.0f
        };
        memcpy(out.verts, v, sizeof(v));
        const uint16_t idx[6] = {0, 2, 1, 0, 3, 2};
        memcpy(out.indices, idx, sizeof(idx));

        out.vertFloatCount = 20;
        out.indexCount = 6;
        out.pcFloatCount = 8;
        out.vertStride = 5;
        out.format = VertexFormat::Position3UV2;
        out.needsIdentityMVP = true;
        out.glass = false;
    }

    // --- Variable-size primitives (write to external scratch vectors) ---

    static void buildPolygon(DrawPacket& out, std::vector<float>& scratchV,
                              std::vector<uint16_t>& scratchI,
                              const glm::vec2& center, const std::vector<glm::vec2>& perimeter,
                              const glm::vec3& color, float alpha, const CoordinateMapper& m) {
        int n = static_cast<int>(perimeter.size());
        if (n < 3) { out.vertFloatCount = 0; out.indexCount = 0; return; }

        float cNdcX, cNdcY;
        m.pixelToNDC(center.x, center.y, cNdcX, cNdcY);

        scratchV.clear();
        scratchI.clear();

        // Center vertex — RGBA order
        scratchV.push_back(cNdcX); scratchV.push_back(cNdcY); scratchV.push_back(0.0f);
        scratchV.push_back(color.r); scratchV.push_back(color.g); scratchV.push_back(color.b); scratchV.push_back(alpha);
        // Perimeter vertices
        for (int i = 0; i < n; i++) {
            float ndcX, ndcY;
            m.pixelToNDC(perimeter[i].x, perimeter[i].y, ndcX, ndcY);
            scratchV.push_back(ndcX); scratchV.push_back(ndcY); scratchV.push_back(0.0f);
            scratchV.push_back(color.r); scratchV.push_back(color.g); scratchV.push_back(color.b); scratchV.push_back(alpha);
        }

        // Triangle fan indices: center(0), perimeter(i+1), perimeter(i+2)
        for (int i = 0; i < n; i++) {
            uint16_t j = static_cast<uint16_t>((i + 1) % n + 1);
            scratchI.push_back(0);
            scratchI.push_back(static_cast<uint16_t>(i + 1));
            scratchI.push_back(j);
        }

        out.extVerts = scratchV.data();
        out.extIndices = scratchI.data();
        out.vertFloatCount = static_cast<uint32_t>(scratchV.size());
        out.indexCount = static_cast<uint32_t>(scratchI.size());
        out.pcFloatCount = 0;
        out.vertStride = 7;
        out.format = VertexFormat::Position3Color4;
        out.needsIdentityMVP = false;
        out.glass = false;
    }

    static void buildImage(DrawPacket& out, std::vector<float>& scratchV,
                            std::vector<uint16_t>& scratchI,
                            const glm::vec2& pos, const glm::vec2& size, float rotation,
                            const std::vector<glm::vec2>& clipVertices, const glm::vec2& clipCenter,
                            const CoordinateMapper& m) {
        scratchV.clear();
        scratchI.clear();

        // MVP identity (stored as push constants)
        float pc[16] = {};
        pc[0] = 1.0f; pc[5] = 1.0f; pc[10] = 1.0f; pc[15] = 1.0f;
        memcpy(out.pushConstants, pc, sizeof(pc));
        out.pcFloatCount = 16;

        if (clipVertices.size() >= 3) {
            // --- Polygon clip mode (triangle-fan) ---
            int n = static_cast<int>(clipVertices.size());

            float cxPx = pos.x + size.x * 0.5f;
            float cyPx = pos.y + size.y * 0.5f;

            float cu = size.x > 0.0f ? std::clamp((clipCenter.x - pos.x) / size.x, 0.0f, 1.0f) : 0.0f;
            float cv = size.y > 0.0f ? std::clamp(1.0f - (clipCenter.y - pos.y) / size.y, 0.0f, 1.0f) : 0.0f;

            float ccPx = clipCenter.x, ccPy = clipCenter.y;
            struct ClipVert { float px, py, u, v; };
            std::vector<ClipVert> clipVerts(n);
            for (int i = 0; i < n; i++) {
                clipVerts[i].px = clipVertices[i].x;
                clipVerts[i].py = clipVertices[i].y;
                clipVerts[i].u  = size.x > 0.0f ? std::clamp((clipVertices[i].x - pos.x) / size.x, 0.0f, 1.0f) : 0.0f;
                clipVerts[i].v  = size.y > 0.0f ? std::clamp(1.0f - (clipVertices[i].y - pos.y) / size.y, 0.0f, 1.0f) : 0.0f;
            }

            if (std::fabs(rotation) > 0.5f) {
                float rad = rotation * 3.14159265358979323846f / 180.0f;
                float cosR = std::cos(rad);
                float sinR = std::sin(rad);
                auto rotateAround = [&](float& px, float& py) {
                    float dx = px - cxPx, dy = py - cyPx;
                    px = cxPx + dx * cosR - dy * sinR;
                    py = cyPx + dx * sinR + dy * cosR;
                };
                rotateAround(ccPx, ccPy);
                for (int i = 0; i < n; i++) rotateAround(clipVerts[i].px, clipVerts[i].py);
            }

            auto pushVert = [&](float px, float py, float u, float v) {
                float ndcX, ndcY;
                m.pixelToNDC(px, py, ndcX, ndcY);
                scratchV.push_back(ndcX);
                scratchV.push_back(ndcY);
                scratchV.push_back(0.0f);
                scratchV.push_back(u);
                scratchV.push_back(v);
            };

            pushVert(ccPx, ccPy, cu, cv);
            for (int i = 0; i < n; i++) {
                pushVert(clipVerts[i].px, clipVerts[i].py, clipVerts[i].u, clipVerts[i].v);
            }

            for (int i = 0; i < n; i++) {
                uint16_t j = static_cast<uint16_t>((i + 1) % n + 1);
                scratchI.push_back(0);
                scratchI.push_back(static_cast<uint16_t>(i + 1));
                scratchI.push_back(j);
            }

        } else if (std::fabs(rotation) > 0.5f) {
            // --- Rotated rectangle mode ---
            float cx = pos.x + size.x * 0.5f;
            float cy = pos.y + size.y * 0.5f;
            float rad = rotation * 3.14159265358979323846f / 180.0f;
            float cosR = std::cos(rad);
            float sinR = std::sin(rad);

            struct CornerUV { float dx, dy, u, v; };
            CornerUV corners[4] = {
                { -size.x * 0.5f, -size.y * 0.5f, 0.0f, 1.0f },
                {  size.x * 0.5f, -size.y * 0.5f, 1.0f, 1.0f },
                {  size.x * 0.5f,  size.y * 0.5f, 1.0f, 0.0f },
                { -size.x * 0.5f,  size.y * 0.5f, 0.0f, 0.0f },
            };

            for (int i = 0; i < 4; i++) {
                float rx = corners[i].dx * cosR - corners[i].dy * sinR;
                float ry = corners[i].dx * sinR + corners[i].dy * cosR;
                float ndcX, ndcY;
                m.pixelToNDC(cx + rx, cy + ry, ndcX, ndcY);
                scratchV.push_back(ndcX);
                scratchV.push_back(ndcY);
                scratchV.push_back(0.0f);
                scratchV.push_back(corners[i].u);
                scratchV.push_back(corners[i].v);
            }
            const uint16_t idx[6] = {0, 1, 2, 2, 3, 0};
            scratchI.assign(idx, idx + 6);

        } else {
            // --- Default axis-aligned rectangle ---
            float x0, y0, x1, y1;
            m.pixelToNDC(pos.x, pos.y, x0, y0);
            m.pixelToNDC(pos.x + size.x, pos.y + size.y, x1, y1);
            const float v[20] = { x0,y0,0,0,1, x1,y0,0,1,1, x1,y1,0,1,0, x0,y1,0,0,0 };
            scratchV.assign(v, v + 20);
            const uint16_t idx[6] = {0, 1, 2, 2, 3, 0};
            scratchI.assign(idx, idx + 6);
        }

        out.extVerts = scratchV.data();
        out.extIndices = scratchI.data();
        out.vertFloatCount = static_cast<uint32_t>(scratchV.size());
        out.indexCount = static_cast<uint32_t>(scratchI.size());
        out.vertStride = 5;
        out.format = VertexFormat::Position3UV2;
        out.needsIdentityMVP = false;
        out.glass = false;
    }
};

} // namespace AgenUIEngine::Core

#endif // AGENUI_GEOMETRY_BUILDER_H
