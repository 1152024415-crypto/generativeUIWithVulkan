# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(VulkanDemo)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SIMPLEENGINE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/render/agenui_engine)

# Platform detection
if(DEFINED OHOS_PLATFORM OR DEFINED CMAKE_OHOS_ARCHITECTURE)
    # HarmonyOS build
    ADD_DEFINITIONS(-DVK_USE_PLATFORM_OHOS=1)
    ADD_DEFINITIONS(-DAGENUI_PLATFORM_HARMONYOS=1)
    message(STATUS "Configuring for HarmonyOS platform")
elseif(WIN32)
    # Windows build (handled by windows/CMakeLists.txt)
    message(STATUS "Configuring for Windows platform - use windows/CMakeLists.txt instead")
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/common
                    ${NATIVERENDER_ROOT_PATH}/render
                    ${NATIVERENDER_ROOT_PATH}/render/application
                    ${NATIVERENDER_ROOT_PATH}/render/application/game
                    ${NATIVERENDER_ROOT_PATH}/lpc/interface
                    ${NATIVERENDER_ROOT_PATH}/lpc/include
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${NATIVERENDER_ROOT_PATH}/thirdparty
                    ${SIMPLEENGINE_PATH}
                    ${SIMPLEENGINE_PATH}/core
                    ${SIMPLEENGINE_PATH}/backend
                    ${SIMPLEENGINE_PATH}/backend/vulkan
                    ${SIMPLEENGINE_PATH}/modules/text
                    ${SIMPLEENGINE_PATH}/modules/text/text_layout
                    ${SIMPLEENGINE_PATH}/include
                    ${SIMPLEENGINE_PATH}/shaders
                    ${SIMPLEENGINE_PATH}/thirdparty)

find_library( # Sets the name of the path variable.
              hilog-lib
              # Specifies the name of the NDK library that
              # you want CMake to locate.
              hilog_ndk.z )

find_library( # Sets the name of the path variable.
              libuv-lib
              # Specifies the name of the NDK library that
              # you want CMake to locate.
              uv )

find_library( # Sets the name of the path variable.
            libnapi-lib
            # Specifies the name of the NDK library that
            # you want CMake to locate.
            ace_napi.z )

find_library( # Sets the name of the path variable.
              libace-lib
              # Specifies the name of the NDK library that
              # you want CMake to locate.
              ace_ndk.z )

find_library(
             libnative_window-lib
             native_window )

find_library(
             libnative_vsync-lib
             native_vsync )

find_library(
             libvulkan-lib
             vulkan )

add_library(nativerender SHARED
            render/plugin_manager.cpp
            render/plugin_render.cpp
            lpc/src/EmotionManager.cpp
            lpc/src/EmotionDiagnostics.cpp
            render/application/application/Application.cpp
            render/application/application/vulkan_utils.cpp
            render/application/FontConfig.cpp
            # DSL render layer (new) — each renderer self-registers
            render/application/dsl/DslRenderRegistry.cpp
            render/application/dsl/CustomDslRender.cpp
            render/application/dsl/CustomV2DslRender.cpp
            render/application/dsl/A2uiDslRender.cpp
            render/application/dsl/CssStyleConverter.cpp
            render/application/dsl/GameDslRender.cpp
            render/application/dsl/MdParser.cpp
            render/application/dsl/MdStyleMapper.cpp
            render/application/game/GameModule.cpp
            # md4c — CommonMark parser (C library)
            thirdparty/md4c/md4c.c
            plugin.cpp)

target_link_libraries(nativerender PUBLIC ${hilog-lib}  ${libuv-lib} ${libnapi-lib} ${libace-lib} librawfile.z.so simpleengine ${libnative_vsync-lib} ${libvulkan-lib} yoga)

# SimpleEngine library - Cross-platform (HarmonyOS and Windows)
set(SIMPLEENGINE_SOURCES
    # Core Vulkan backend
    ${SIMPLEENGINE_PATH}/backend/vulkan/VkRenderer.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/VkTextureCache.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/GlassEffect.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/VulkanContext.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/PipelineManager.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/VkShader.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/VkTexture.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/VkBuffer.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/DynamicVertexBufferPool.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/PipelineBuilder.cpp
    ${SIMPLEENGINE_PATH}/backend/vulkan/DescriptorManager.cpp
    # Core components
    ${SIMPLEENGINE_PATH}/core/RenderQueue.cpp
    ${SIMPLEENGINE_PATH}/core/RenderContext.cpp
    # Modules - text
    ${SIMPLEENGINE_PATH}/modules/text/FontManager.cpp
    ${SIMPLEENGINE_PATH}/modules/text/TextAtlas.cpp
    ${SIMPLEENGINE_PATH}/modules/text/TextLayoutEngine.cpp
    ${SIMPLEENGINE_PATH}/modules/text/MsdfAtlas.cpp
    ${SIMPLEENGINE_PATH}/modules/text/MsdfAtlasData.cpp
    ${SIMPLEENGINE_PATH}/modules/text/text_layout/TextLayout.cpp
    ${SIMPLEENGINE_PATH}/modules/text/text_layout/TextAnalysis.cpp
    ${SIMPLEENGINE_PATH}/modules/text/text_layout/TextLineBreak.cpp
    ${SIMPLEENGINE_PATH}/modules/text/text_layout/TextMeasurement.cpp
    ${SIMPLEENGINE_PATH}/modules/text/text_layout/UnicodeUtils.cpp
    # Backend factory
    ${SIMPLEENGINE_PATH}/backend/RenderFactory.cpp
)

add_library(simpleengine SHARED ${SIMPLEENGINE_SOURCES})

target_compile_definitions(simpleengine PUBLIC AGENUI_PLATFORM_HARMONYOS=1)

target_include_directories(simpleengine PUBLIC
    ${SIMPLEENGINE_PATH}/core
    ${SIMPLEENGINE_PATH}/backend
    ${SIMPLEENGINE_PATH}/backend/vulkan
    ${SIMPLEENGINE_PATH}/modules/text
    ${SIMPLEENGINE_PATH}/modules/text/text_layout
    ${SIMPLEENGINE_PATH}/thirdparty/freetype/include
    ${SIMPLEENGINE_PATH}/thirdparty
)

target_link_libraries(simpleengine PUBLIC
    ${hilog-lib}
    ${libnative_window-lib}
    ${libvulkan-lib}
    librawfile.z.so
    libhitrace_ndk.z.so
    icu
    freetype
)

# Add FreeType as subdirectory for OHOS build
set(FREETYPE_DIR ${SIMPLEENGINE_PATH}/thirdparty/freetype)
if(EXISTS ${FREETYPE_DIR})
    add_subdirectory(${FREETYPE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/freetype)
endif()

# Add GLM (header-only library for HarmonyOS)
# GLM is header-only, we just need the include path
# If GLM sources are available in the project, add them here
# Otherwise, user needs to ensure GLM headers are available

# Yoga Flexbox layout engine (built from thirdparty source)
set(YOGA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/yoga)
file(GLOB YOGA_SOURCES "${YOGA_DIR}/*.cpp")
add_library(yoga SHARED ${YOGA_SOURCES})
target_include_directories(yoga PUBLIC "${YOGA_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")
target_compile_options(yoga PRIVATE
    -fno-omit-frame-pointer
    -fexceptions
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)
set_target_properties(yoga PROPERTIES
    CXX_VISIBILITY_PRESET default
    POSITION_INDEPENDENT_CODE ON
)

# DSL auto-scaling: maps JSON coordinate space to screen pixels when they differ.
# Enable only if your JSON root size does not match designWidth/designHeight.
option(ENABLE_DSL_AUTO_SCALE "Enable DSL V2 auto-scaling" OFF)
if(ENABLE_DSL_AUTO_SCALE)
    add_definitions(-DDSL_AUTO_SCALE)
endif()

# Unit tests (enabled with -DBUILD_TESTS=ON)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(${SIMPLEENGINE_PATH}/tests ${CMAKE_CURRENT_BINARY_DIR}/agenui_tests)
endif()
