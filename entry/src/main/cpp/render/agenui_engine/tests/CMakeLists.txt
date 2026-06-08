cmake_minimum_required(VERSION 3.4.1)

# ────────────────────────────────────────────────────────────────────────────
# AgenUI Engine Test Suite — HarmonyOS
#
# 通过主 CMakeLists.txt 的 BUILD_TESTS=ON 启用。
# 在 HarmonyOS 设备上自动化运行全部用例并输出结果。
#
# 构建产物:
#   agenui_tests        — 单元测试 (Level 1/2, 无需 GPU)
#   agenui_visual_tests — 视觉对比测试 (Level 3, 需要 GPU, 可选)
#
# 运行:
#   hdc file send agenui_tests /data/local/tmp/
#   hdc shell chmod +x /data/local/tmp/agenui_tests
#   hdc shell /data/local/tmp/agenui_tests --gtest_output=xml:/data/local/tmp/test_results.xml
#   hdc file recv /data/local/tmp/test_results.xml ./test_results/
# ────────────────────────────────────────────────────────────────────────────

# ── Google Test (本地 thirdparty) ──
set(GOOGLETEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty/googletest"
    CACHE PATH "GoogleTest source directory")
add_subdirectory("${GOOGLETEST_DIR}" "${CMAKE_BINARY_DIR}/googletest")

set(LPC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../lpc")

# ────────────────────────────────────────────────────────────────────────────
# 单元测试 (Level 1/2) — 纯 CPU, 无需 GPU
# ────────────────────────────────────────────────────────────────────────────

set(UNIT_TEST_SOURCES
    # Core — 数据结构 + 队列逻辑
    core/TestRenderQueue.cpp
    core/TestRenderCommand.cpp
    core/TestRenderContext.cpp
    # Text — Unicode 分析、排版算法
    text/TestUnicodeUtils.cpp
    text/TestTextAnalysis.cpp
    text/TestTextLineBreak.cpp
    text/TestDirtyRegion.cpp
    text/TestGlyph.cpp
    ${LPC_ROOT}/tests/TestEmotionDiagnostics.cpp
    ${LPC_ROOT}/src/EmotionDiagnostics.cpp
)

add_executable(agenui_tests ${UNIT_TEST_SOURCES})

# Set RPATH to $ORIGIN so the binary finds libsimpleengine.so next to itself
set_target_properties(agenui_tests PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
)

target_include_directories(agenui_tests PRIVATE
    ${SIMPLEENGINE_PATH}
    ${SIMPLEENGINE_PATH}/core
    ${SIMPLEENGINE_PATH}/backend
    ${SIMPLEENGINE_PATH}/backend/vulkan
    ${SIMPLEENGINE_PATH}/modules/text
    ${SIMPLEENGINE_PATH}/modules/text/text_layout
    ${SIMPLEENGINE_PATH}/thirdparty
    ${SIMPLEENGINE_PATH}/thirdparty/glm
    ${SIMPLEENGINE_PATH}/tests            # for mocks/MockRenderer.h
    ${SIMPLEENGINE_PATH}/tests/mocks      # direct mock include
    ${LPC_ROOT}/include
    ${LPC_ROOT}/interface
)

target_link_libraries(agenui_tests PRIVATE
    gtest_main
    gmock
    simpleengine
)

# HarmonyOS 平台依赖
if(AGENUI_PLATFORM_HARMONYOS OR DEFINED OHOS_PLATFORM)
    target_link_libraries(agenui_tests PRIVATE icu freetype)
    # hilog 用于测试日志输出
    find_library(hilog-lib hilog_ndk.z)
    if(hilog-lib)
        target_link_libraries(agenui_tests PRIVATE ${hilog-lib})
    endif()
endif()

include(GoogleTest)
# Use gtest_add_tests (source-based discovery) instead of gtest_discover_tests
# because cross-compilation for HarmonyOS prevents running the binary at build time.
gtest_add_tests(TARGET agenui_tests)

# ────────────────────────────────────────────────────────────────────────────
# 视觉对比测试 (Level 3) — 需要 GPU 设备
#
# 启用: cmake -DBUILD_VISUAL_TESTS=ON ..
# 在 HarmonyOS 真机或模拟器上运行
# ────────────────────────────────────────────────────────────────────────────

option(BUILD_VISUAL_TESTS "Build Level 3 visual integration tests (requires GPU)" OFF)

if(BUILD_VISUAL_TESTS)
    set(VISUAL_TEST_SOURCES
        visual/FrameCapture.cpp
        visual/TestVisualRect.cpp
        visual/TestVisualRoundedRect.cpp
        visual/TestVisualGlassEffect.cpp
        visual/TestVisualDepthSort.cpp
        visual/TestVisualTextGlow.cpp
        visual/TestVisualImage.cpp
    )

    add_executable(agenui_visual_tests ${VISUAL_TEST_SOURCES})

    target_include_directories(agenui_visual_tests PRIVATE
        ${SIMPLEENGINE_PATH}
        ${SIMPLEENGINE_PATH}/core
        ${SIMPLEENGINE_PATH}/backend
        ${SIMPLEENGINE_PATH}/backend/vulkan
        ${SIMPLEENGINE_PATH}/modules/text
        ${SIMPLEENGINE_PATH}/modules/text/text_layout
        ${SIMPLEENGINE_PATH}/thirdparty
        ${SIMPLEENGINE_PATH}/thirdparty/glm
    )

    target_link_libraries(agenui_visual_tests PRIVATE
        gtest_main
        gmock
        simpleengine
    )

    # HarmonyOS 平台
    if(AGENUI_PLATFORM_HARMONYOS OR DEFINED OHOS_PLATFORM)
        target_link_libraries(agenui_visual_tests PRIVATE
            icu
            freetype
            ${libnative_window-lib}
            ${libvulkan-lib}
        )
        if(hilog-lib)
            target_link_libraries(agenui_visual_tests PRIVATE ${hilog-lib})
        endif()
    endif()

    include(GoogleTest)
    gtest_add_tests(TARGET agenui_visual_tests)
endif()
