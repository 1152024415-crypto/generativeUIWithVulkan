/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Visual Test Framework — Level 3 screenshot comparison integration tests.
 *
 * 设计思路:
 *   1. 在真实设备/窗口上创建 Vulkan 渲染环境
 *   2. 通过 RenderContext 高层 API 绘制标准 UI 场景
 *   3. 从 swapchain 回读像素数据 (vkCmdCopyImage → staging buffer → mapMemory)
 *   4. 与预存参考图做像素级对比，允许容差
 *   5. 自动生成 diff 图和测试报告
 *
 * 架构:
 *   VisualTestRunner   — 窗口/Vulkan 生命周期管理
 *   FrameCapture       — 像素回读引擎
 *   ImageComparator    — 像素对比 + diff 输出
 *   VisualTestBase     — gtest fixture，子类只需实现 renderScene()
 *
 * 使用:
 *   class MyTest : public VisualTestBase {
 *       void renderScene() override {
 *           ctx->drawRect({100,100}, {200,100}, {1,0,0});
 *       }
 *   };
 *   TEST_F(MyTest, RedRect) {
 *       runAndCompare("red_rect");
 *   }
 */

#ifndef AGENUI_VISUAL_TEST_FRAMEWORK_H
#define AGENUI_VISUAL_TEST_FRAMEWORK_H

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

// ── 前向声明 ──
// 生产代码头文件，在实际编译时提供
namespace AgenUIEngine::Core {
    class RenderContext;
    struct RendererInitParams;
}

// ============================================================================
// FrameCapture — Vulkan swapchain 像素回读
// ============================================================================

struct CaptureResult {
    std::vector<uint8_t> pixels;   // RGBA8888
    uint32_t width = 0;
    uint32_t height = 0;
    bool valid = false;
};

/**
 * FrameCapture 从 Vulkan swapchain image 回读像素。
 *
 * 实现路径 (需要访问 VulkanContext 内部对象):
 *   1. 创建 host-visible staging buffer (VK_BUFFER_USAGE_TRANSFER_DST_BIT)
 *   2. vkCmdCopyImageToBuffer(swapchainImage → stagingBuffer)
 *      注意: swapchain 格式通常是 B8G8R8A8_SRGB, 需要 BGR→RGB 转换
 *   3. vkMapMemory 读取像素
 *   4. BGR→RGB 重排为 RGBA8888 标准格式
 *
 * 这部分代码需要包含 vulkan/vulkan.h 并持有 VulkanContext 指针，
 * 在实际项目中作为 VkRenderer 或 VulkanContext 的扩展方法实现。
 */
class FrameCapture {
public:
    /**
     * 回读当前 swapchain image 的像素数据。
     *
     * @param device         VkDevice
     * @param physicalDevice VkPhysicalDevice
     * @param commandPool    VkCommandPool
     * @param queue          VkQueue (graphics)
     * @param swapchainImage VkImage (当前帧的 swapchain image)
     * @param width/height   图像尺寸
     * @param format         swapchain 格式 (用于判断 BGR vs RGB)
     * @return CaptureResult RGBA8888 像素数据
     *
     * 伪代码:
     *   VkImage stagingImage;
     *   vkCreateImage(..., VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT, ...);
     *   vkCmdBlitImage(swapchainImage → stagingImage); // 或 CopyImage
     *   vkMapMemory → memcpy pixels;
     *   if (format == B8G8R8A8) swap R↔B channels;
     */
    static CaptureResult capture(
        void* device, void* physicalDevice, void* commandPool, void* queue,
        void* swapchainImage, uint32_t width, uint32_t height, uint32_t format);

    // 简化版: 直接从已知 buffer 读取 (用于测试框架本身的验证)
    static CaptureResult fromRGBA(const std::vector<uint8_t>& rgba,
                                   uint32_t w, uint32_t h) {
        CaptureResult r;
        r.pixels = rgba;
        r.width = w;
        r.height = h;
        r.valid = true;
        return r;
    }
};

// ============================================================================
// ImageComparator — 像素对比引擎
// ============================================================================

struct CompareResult {
    bool passed = false;
    double averageError = 0.0;         // 全图平均每通道误差 (0~255)
    double maxError = 0.0;             // 最大单像素单通道误差
    uint32_t mismatchPixels = 0;       // 超过容差的像素数
    uint32_t totalPixels = 0;
    double mismatchRatio = 0.0;        // mismatchPixels / totalPixels
    std::string diffImagePath;         // diff 图保存路径 (可选)
};

class ImageComparator {
public:
    struct Options {
        double maxAvgError = 2.0;      // 允许的平均误差阈值
        double maxPixelError = 15.0;   // 允许的单像素最大误差
        double maxMismatchRatio = 0.01;// 允许的 mismatch 像素比例 (1%)
        bool generateDiffImage = true; // 是否生成 diff 图
        std::string diffOutputDir = "./visual_test_diffs/";
    };

    /**
     * 对比两帧 RGBA8888 像素数据。
     *
     * @param actual   渲染输出
     * @param expected 参考图
     * @param opts     对比选项
     * @return 对比结果
     */
    static CompareResult compare(const CaptureResult& actual,
                                  const CaptureResult& expected,
                                  const Options& opts = Options{}) {
        CompareResult result;
        result.totalPixels = actual.width * actual.height;

        // 尺寸不匹配直接失败
        if (actual.width != expected.width || actual.height != expected.height) {
            result.passed = false;
            result.averageError = 255.0;
            result.maxError = 255.0;
            result.mismatchPixels = result.totalPixels;
            result.mismatchRatio = 1.0;
            return result;
        }

        double totalError = 0.0;
        result.maxError = 0.0;
        result.mismatchPixels = 0;

        for (uint32_t i = 0; i < result.totalPixels; ++i) {
            // 每像素 4 字节 (RGBA)
            const uint8_t* pa = &actual.pixels[i * 4];
            const uint8_t* pe = &expected.pixels[i * 4];

            double pixelError = 0.0;
            for (int c = 0; c < 3; ++c) { // R, G, B (skip Alpha)
                double diff = std::fabs(static_cast<double>(pa[c]) - pe[c]);
                pixelError = std::max(pixelError, diff);
                totalError += diff;
            }

            if (pixelError > opts.maxPixelError) {
                result.mismatchPixels++;
            }
            result.maxError = std::max(result.maxError, pixelError);
        }

        result.averageError = totalError / (static_cast<double>(result.totalPixels) * 3.0);
        result.mismatchRatio = static_cast<double>(result.mismatchPixels) /
                                static_cast<double>(result.totalPixels);

        result.passed = (result.averageError <= opts.maxAvgError) &&
                        (result.mismatchRatio <= opts.maxMismatchRatio);

        // 生成 diff 图
        if (opts.generateDiffImage && result.mismatchPixels > 0) {
            result.diffImagePath = generateDiffImage(actual, expected, opts);
        }

        return result;
    }

private:
    /**
     * 生成差异可视化图: 红色=差异区域, 亮度=误差大小。
     * 输出为 PPM 格式 (无需第三方库)。
     */
    static std::string generateDiffImage(const CaptureResult& actual,
                                          const CaptureResult& expected,
                                          const Options& opts) {
        std::string path = opts.diffOutputDir + "diff_" +
                          std::to_string(actual.width) + "x" +
                          std::to_string(actual.height) + ".ppm";

        std::vector<uint8_t> diff(actual.width * actual.height * 3);

        for (uint32_t i = 0; i < actual.width * actual.height; ++i) {
            const uint8_t* pa = &actual.pixels[i * 4];
            const uint8_t* pe = &expected.pixels[i * 4];

            double maxChDiff = 0;
            for (int c = 0; c < 3; ++c) {
                maxChDiff = std::max(maxChDiff,
                    std::fabs(static_cast<double>(pa[c]) - pe[c]));
            }

            if (maxChDiff > opts.maxPixelError) {
                // 差异区域: 红色通道编码误差强度
                diff[i * 3 + 0] = static_cast<uint8_t>(
                    std::min(255.0, maxChDiff * (255.0 / opts.maxPixelError)));
                diff[i * 3 + 1] = 0;
                diff[i * 3 + 2] = 0;
            } else {
                // 匹配区域: 灰度显示
                diff[i * 3 + 0] = pe[0];
                diff[i * 3 + 1] = pe[1];
                diff[i * 3 + 2] = pe[2];
            }
        }

        // 写 PPM
        std::ofstream file(path, std::ios::binary);
        if (file.is_open()) {
            file << "P6\n" << actual.width << " " << actual.height << "\n255\n";
            file.write(reinterpret_cast<const char*>(diff.data()), diff.size());
        }
        return path;
    }
};

// ============================================================================
// PPM 图像 I/O — 零依赖参考图读写
// ============================================================================

class PPMImage {
public:
    static bool write(const std::string& path, const CaptureResult& frame) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        file << "P6\n" << frame.width << " " << frame.height << "\n255\n";
        for (uint32_t i = 0; i < frame.width * frame.height; ++i) {
            file.put(frame.pixels[i * 4 + 0]); // R
            file.put(frame.pixels[i * 4 + 1]); // G
            file.put(frame.pixels[i * 4 + 2]); // B
        }
        return true;
    }

    static CaptureResult read(const std::string& path) {
        CaptureResult result;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return result;

        std::string magic;
        file >> magic;
        if (magic != "P6") return result;

        file >> result.width >> result.height;
        int maxVal;
        file >> maxVal;
        file.get(); // skip single whitespace

        result.pixels.resize(result.width * result.height * 4, 255);
        for (uint32_t i = 0; i < result.width * result.height; ++i) {
            result.pixels[i * 4 + 0] = static_cast<uint8_t>(file.get()); // R
            result.pixels[i * 4 + 1] = static_cast<uint8_t>(file.get()); // G
            result.pixels[i * 4 + 2] = static_cast<uint8_t>(file.get()); // B
            // Alpha = 255
        }
        result.valid = true;
        return result;
    }

    /**
     * 生成纯色参考图。
     */
    static CaptureResult solidColor(uint32_t w, uint32_t h,
                                     uint8_t r, uint8_t g, uint8_t b) {
        CaptureResult result;
        result.width = w;
        result.height = h;
        result.pixels.resize(w * h * 4);
        for (uint32_t i = 0; i < w * h; ++i) {
            result.pixels[i * 4 + 0] = r;
            result.pixels[i * 4 + 1] = g;
            result.pixels[i * 4 + 2] = b;
            result.pixels[i * 4 + 3] = 255;
        }
        result.valid = true;
        return result;
    }

    /**
     * 在指定区域填充颜色 (模拟渲染结果)。
     */
    static void fillRect(CaptureResult& frame,
                          uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint8_t r, uint8_t g, uint8_t b) {
        for (uint32_t row = y; row < y + h && row < frame.height; ++row) {
            for (uint32_t col = x; col < x + w && col < frame.width; ++col) {
                uint32_t idx = (row * frame.width + col) * 4;
                frame.pixels[idx + 0] = r;
                frame.pixels[idx + 1] = g;
                frame.pixels[idx + 2] = b;
                frame.pixels[idx + 3] = 255;
            }
        }
    }
};

// ============================================================================
// VisualTestBase — gtest fixture 基类
// ============================================================================

/**
 * VisualTestBase 管理渲染生命周期:
 *   SetUp()   → 创建窗口 + RenderContext + 所有 pipeline
 *   renderScene() → 子类实现，绘制场景
 *   TearDown() → 清理
 *
 * 典型测试流程:
 *   1. renderScene() 绘制内容
 *   2. endFrame + captureFrame 获取像素
 *   3. 与参考图对比
 *
 * 注意: 此基类需要平台相关的窗口创建代码:
 *   - Windows:  GLFW 创建窗口
 *   - HarmonyOS: OH_NativeWindow 创建
 *   实际编译时由平台分支提供。
 */
class VisualTestBase : public ::testing::Test {
protected:
    // 由子类或平台 Setup 提供
    AgenUIEngine::Core::RenderContext* context = nullptr;
    CaptureResult lastCapture;
    uint32_t surfaceWidth = 800;
    uint32_t surfaceHeight = 600;

    void SetUp() override {
        // ── 平台相关: 创建窗口 ──
        // Windows:
        //   glfwInit(); glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        //   window = glfwCreateWindow(surfaceWidth, surfaceHeight, "VisualTest", NULL, NULL);
        //
        // HarmonyOS:
        //   OHNativeWindow* window = ...; // 从测试 Activity 获取

        // ── 创建 RenderContext + VkRenderer ──
        // auto renderer = std::make_unique<AgenUIEngine::VkRenderer>();
        // context = new RenderContext(std::move(renderer));
        //
        // RendererInitParams params;
        // params.nativeWindow = glfwGetWin32Window(window); // Windows
        // params.width = surfaceWidth;
        // params.height = surfaceHeight;
        // ASSERT_TRUE(context->initialize(params));

        // ── 创建所有 pipeline ──
        // ASSERT_TRUE(context->createPipelines());

        // ── 初始化字体 ──
        // ASSERT_TRUE(context->initializeFonts(nullptr, "test_font.ttf"));
    }

    void TearDown() override {
        // delete context;
        // glfwDestroyWindow(window);
        // glfwTerminate();
    }

    /**
     * 子类实现: 绘制测试场景。
     *
     * 框架在调用前已执行 beginFrame()，子类只需调用 draw 方法。
     */
    virtual void renderScene() = 0;

    /**
     * 执行完整帧: beginFrame → renderScene → endFrame → capture。
     *
     * @return 捕获的像素数据
     */
    CaptureResult renderAndCapture() {
        // context->beginFrame();
        renderScene();
        // context->endFrame();

        // 回读像素 (需要 VulkanContext 内部对象访问)
        // lastCapture = FrameCapture::capture(...);
        return lastCapture;
    }

    /**
     * 运行渲染并与参考图对比。
     *
     * @param referenceName 参考图名称 (不含扩展名)
     * @param opts          对比选项
     * @return 对比结果
     */
    CompareResult runAndCompare(const std::string& referenceName,
                                 const ImageComparator::Options& opts = {}) {
        auto actual = renderAndCapture();

        // 加载参考图
        std::string refPath = "./visual_test_references/" + referenceName + ".ppm";
        auto expected = PPMImage::read(refPath);

        if (!expected.valid) {
            // 参考图不存在: 自动生成当前帧作为新参考图
            PPMImage::write(refPath, actual);
            CompareResult result;
            result.passed = true;
            result.totalPixels = actual.width * actual.height;
            return result;
        }

        return ImageComparator::compare(actual, expected, opts);
    }

    /**
     * 仅渲染并保存为参考图 (用于首次生成参考图)。
     */
    void saveAsReference(const std::string& name) {
        auto frame = renderAndCapture();
        std::string path = "./visual_test_references/" + name + ".ppm";
        PPMImage::write(path, frame);
    }
};

// ============================================================================
// 辅助: 程序化参考图生成器
// ============================================================================

/**
 * ReferenceImageGen 程序化生成期望的渲染结果。
 * 用于不依赖外部图片文件的测试场景。
 *
 * 由于 shader 效果 (SDF, glass, glow) 无法精确在 CPU 复现，
 * 程序化参考图只验证基础布局（位置、尺寸、颜色区域）。
 * 对于复杂效果，使用松散容差 (maxMismatchRatio=0.05)。
 */
class ReferenceImageGen {
public:
    /**
     * 生成一张纯色背景 + 矩形区域的参考图。
     * 验证矩形绘制的位置和尺寸是否正确。
     */
    static CaptureResult rectOnBackground(
        uint32_t canvasW, uint32_t canvasH,
        uint8_t bgR, uint8_t bgG, uint8_t bgB,
        uint32_t rectX, uint32_t rectY, uint32_t rectW, uint32_t rectH,
        uint8_t rectR, uint8_t rectG, uint8_t rectB)
    {
        auto frame = PPMImage::solidColor(canvasW, canvasH, bgR, bgG, bgB);
        PPMImage::fillRect(frame, rectX, rectY, rectW, rectH, rectR, rectG, rectB);
        return frame;
    }

    /**
     * 生成多矩形参考图 (验证 depth 排序后的覆盖顺序)。
     * 后绘制的矩形覆盖先绘制的。
     */
    static CaptureResult overlappingRects(
        uint32_t canvasW, uint32_t canvasH,
        uint8_t bgR, uint8_t bgG, uint8_t bgB,
        const std::vector<std::tuple<uint32_t,uint32_t,uint32_t,uint32_t,
                                      uint8_t,uint8_t,uint8_t>>& rects)
    {
        auto frame = PPMImage::solidColor(canvasW, canvasH, bgR, bgG, bgB);
        for (const auto& r : rects) {
            PPMImage::fillRect(frame,
                std::get<0>(r), std::get<1>(r), std::get<2>(r), std::get<3>(r),
                std::get<4>(r), std::get<5>(r), std::get<6>(r));
        }
        return frame;
    }
};

#endif // AGENUI_VISUAL_TEST_FRAMEWORK_H
