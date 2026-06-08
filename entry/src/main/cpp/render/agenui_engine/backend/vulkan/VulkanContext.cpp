#include "VulkanContext.h"
#include "logger_common.h"

// Platform detection and includes
#if AGENUI_PLATFORM_WINDOWS
    #include <GLFW/glfw3.h>
    #include <iostream>
    #include <fstream>
    #include <filesystem>
#elif AGENUI_PLATFORM_HARMONYOS
    #include <hilog/log.h>
    #include <vulkan/vulkan_ohos.h>
    #include <rawfile/raw_file.h>
    #include <rawfile/raw_file_manager.h>
    #include <native_window/external_window.h>
#endif

#include <vector>
#include <set>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <memory>
#include "thirdparty/glm/glm.hpp"
#include "thirdparty/glm/gtc/matrix_transform.hpp"
#include "DynamicVertexBufferPool.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace AgenUIEngine {

VulkanContext::VulkanContext() {
}

VulkanContext::~VulkanContext() {
    cleanup();
}

bool VulkanContext::initialize(void* nativeWindow, int width, int height) {
    m_nativeWindow = nativeWindow;
    m_width = width;
    m_height = height;

#if AGENUI_PLATFORM_WINDOWS
    LOGI("Initializing Windows Vulkan context: %dx%d", width, height);
#elif AGENUI_PLATFORM_HARMONYOS
    LOGI("Initializing HarmonyOS Vulkan context: %dx%d", width, height);
#endif

    if (!createInstance()) {
        LOGE("Failed to create Vulkan instance");
        return false;
    }

    if (!createSurface()) {
        LOGE("Failed to create Vulkan surface");
        return false;
    }

    if (!pickPhysicalDevice()) {
        LOGE("Failed to pick physical device");
        return false;
    }

    if (!createDevice()) {
        LOGE("Failed to create logical device");
        return false;
    }

    if (!createSwapChain()) {
        LOGE("Failed to create swap chain");
        return false;
    }

    if (!createRenderPass()) {
        LOGE("Failed to create render pass");
        return false;
    }

    if (!createFramebuffers()) {
        LOGE("Failed to create framebuffers");
        return false;
    }

    if (!createCommandBuffers()) {
        LOGE("Failed to create command buffers");
        return false;
    }

    if (!createSyncObjects()) {
        LOGE("Failed to create sync objects");
        return false;
    }

    // Initialize dynamic vertex buffer pool for performance
    if (m_useVertexBufferPool) {
        m_vertexBufferPool = std::make_unique<DynamicVertexBufferPool>(m_device, m_physicalDevice);
        if (!m_vertexBufferPool->initialize()) {
            LOGE("Failed to initialize vertex buffer pool");
            m_vertexBufferPool.reset();
            m_useVertexBufferPool = false;
        } else {
            LOGI("Dynamic vertex buffer pool initialized");
        }
    }

    LOGI("Vulkan context initialized successfully");
    return true;
}

void VulkanContext::setBackgroundColor(float r, float g, float b, float a) {
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
}

void VulkanContext::updateSwapChainExtent(int width, int height) {
    m_width = width;
    m_height = height;
    m_swapChainExtent.width = width;
    m_swapChainExtent.height = height;
}

void VulkanContext::waitIdle() {
    if (m_device) {
        vkDeviceWaitIdle(m_device);
    }
}

// ============================================================================
// Frame lifecycle
// ============================================================================

void VulkanContext::beginFrame() {
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    // Cleanup buffers and memories from this frame
    for (::VkBuffer buffer : m_cleanupBuffers[m_currentFrame]) {
        vkDestroyBuffer(m_device, buffer, nullptr);
    }
    for (VkDeviceMemory memory : m_cleanupMemories[m_currentFrame]) {
        vkFreeMemory(m_device, memory, nullptr);
    }
    m_cleanupBuffers[m_currentFrame].clear();
    m_cleanupMemories[m_currentFrame].clear();

    // Acquire next image from swap chain
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX,
                                            m_imageAvailableSemaphores[m_currentFrame],
                                            VK_NULL_HANDLE, &m_currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOGW("beginFrame: Swap chain out of date, recreating and retrying");
        recreateSwapChain();
        result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX,
                                        m_imageAvailableSemaphores[m_currentFrame],
                                        VK_NULL_HANDLE, &m_currentImageIndex);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            LOGE("beginFrame: Failed to acquire after recreation: %d", result);
            return;
        }
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOGE("beginFrame: Failed to acquire swap chain image: %d", result);
        return;
    }

    // Check if this image is still in use by a previous frame
    if (m_imagesInFlight[m_currentImageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_device, 1, &m_imagesInFlight[m_currentImageIndex], VK_TRUE, UINT64_MAX);
    }
    m_imagesInFlight[m_currentImageIndex] = m_inFlightFences[m_currentFrame];

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[m_currentImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapChainExtent;

    VkClearValue clearColor = {{{m_clearColor[2], m_clearColor[1], m_clearColor[0], m_clearColor[3]}}};
    VkClearValue depthClear = {};
    depthClear.depthStencil = {1.0f, 0};

    std::array<VkClearValue, 2> clearValues = {clearColor, depthClear};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChainExtent.width);
    viewport.height = static_cast<float>(m_swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;

    vkCmdSetViewport(m_commandBuffers[m_currentFrame], 0, 1, &viewport);
    vkCmdSetScissor(m_commandBuffers[m_currentFrame], 0, 1, &scissor);

    // Reset vertex buffer pool for new frame
    if (m_vertexBufferPool && m_vertexBufferPool->isInitialized()) {
        m_vertexBufferPool->resetFrame(m_currentFrame);
    }

    // Reset pipeline state
    m_currentPipeline = VK_NULL_HANDLE;
    m_currentLayout = VK_NULL_HANDLE;

    // Reset descriptor pools to free all descriptor sets from previous frame
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkResetDescriptorPool(m_device, m_descriptorPool, 0);
    }
}

void VulkanContext::endFrame() {
    vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);
    vkEndCommandBuffer(m_commandBuffers[m_currentFrame]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        LOGE("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &m_currentImageIndex;

    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOGW("endFrame: Swap chain out of date/suboptimal, attempting to recreate");
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        LOGE("endFrame: Failed to present swap chain image: %d", result);
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ============================================================================
// Drawing helpers
// ============================================================================

void VulkanContext::drawVertexBuffer(VkPipeline pipeline, VkPipelineLayout layout,
                                      const std::vector<float>& vertices,
                                      const std::vector<uint16_t>& indices,
                                      VkDescriptorSet descriptorSet) {
    if (vertices.empty() || indices.empty()) {
        LOGE("drawVertexBuffer: Empty vertices or indices!");
        return;
    }

    VkDeviceSize vertexBufferSize = sizeof(float) * vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint16_t) * indices.size();

    AgenUIEngine::VkBuffer vertexBufferObj;
    if (!vertexBufferObj.createStaging(m_device, m_physicalDevice, vertexBufferSize)) {
        LOGE("Failed to create vertex buffer");
        return;
    }
    vertexBufferObj.updateData(vertices.data(), vertexBufferSize);

    AgenUIEngine::VkBuffer indexBufferObj;
    if (!indexBufferObj.createStaging(m_device, m_physicalDevice, indexBufferSize)) {
        LOGE("Failed to create index buffer");
        return;
    }
    indexBufferObj.updateData(indices.data(), indexBufferSize);

    ::VkBuffer vertexBuffer = vertexBufferObj.getBuffer();
    ::VkBuffer indexBuffer = indexBufferObj.getBuffer();
    VkDeviceMemory vertexMemory = vertexBufferObj.getMemory();
    VkDeviceMemory indexMemory = indexBufferObj.getMemory();

    vertexBufferObj.detach();
    indexBufferObj.detach();

    float projection[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    VkCommandBuffer cmdBuf = m_commandBuffers[m_currentFrame];
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdPushConstants(cmdBuf, layout,
                      VK_SHADER_STAGE_VERTEX_BIT,
                      0, sizeof(projection), projection);

    if (descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                                0, 1, &descriptorSet, 0, nullptr);
    }

    ::VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(cmdBuf, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuf, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmdBuf, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    m_cleanupBuffers[m_currentFrame].push_back(vertexBuffer);
    m_cleanupBuffers[m_currentFrame].push_back(indexBuffer);
    m_cleanupMemories[m_currentFrame].push_back(vertexMemory);
    m_cleanupMemories[m_currentFrame].push_back(indexMemory);
}

void VulkanContext::drawVertexBufferFromPool(VkPipeline pipeline, VkPipelineLayout layout,
                                              const std::vector<float>& vertices,
                                              const std::vector<uint16_t>& indices,
                                              VkDescriptorSet descriptorSet) {
    if (vertices.empty() || indices.empty()) {
        LOGE("drawVertexBufferFromPool: Empty vertices or indices!");
        return;
    }

    if (!m_vertexBufferPool || !m_vertexBufferPool->isInitialized()) {
        drawVertexBuffer(pipeline, layout, vertices, indices, descriptorSet);
        return;
    }

    VkDeviceSize vertexBufferSize = sizeof(float) * vertices.size();
    auto vertexAlloc = m_vertexBufferPool->allocateVertexData(
        vertices.data(), static_cast<uint32_t>(vertexBufferSize), m_currentFrame);

    if (vertexAlloc.buffer == VK_NULL_HANDLE) {
        drawVertexBuffer(pipeline, layout, vertices, indices, descriptorSet);
        return;
    }

    VkDeviceSize indexBufferSize = sizeof(uint16_t) * indices.size();
    auto indexAlloc = m_vertexBufferPool->allocateIndexData(
        indices.data(), static_cast<uint32_t>(indexBufferSize), m_currentFrame);

    if (indexAlloc.buffer == VK_NULL_HANDLE) {
        drawVertexBuffer(pipeline, layout, vertices, indices, descriptorSet);
        return;
    }

    VkCommandBuffer cmdBuf = m_commandBuffers[m_currentFrame];
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    if (descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                                0, 1, &descriptorSet, 0, nullptr);
    }

    ::VkBuffer vertexBuffers[] = {vertexAlloc.buffer};
    VkDeviceSize offsets[] = {vertexAlloc.offset};
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuf, indexAlloc.buffer, indexAlloc.offset, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(cmdBuf, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}

// Vector-based overload — delegates to raw-pointer version
void VulkanContext::drawWithTempBuffers(const std::vector<float>& vertices,
                                         const std::vector<uint16_t>& indices,
                                         VkPipeline pipeline, VkPipelineLayout layout,
                                         const float* pushConstants, uint32_t pushConstantSize,
                                         VkDescriptorSet descriptorSet) {
    drawWithTempBuffers(vertices.data(), static_cast<uint32_t>(vertices.size()),
                        indices.data(), static_cast<uint32_t>(indices.size()),
                        pipeline, layout, pushConstants, pushConstantSize, descriptorSet);
}

// Raw-pointer overload — actual implementation
void VulkanContext::drawWithTempBuffers(const float* vertices, uint32_t floatCount,
                                         const uint16_t* indices, uint32_t indexCount,
                                         VkPipeline pipeline, VkPipelineLayout layout,
                                         const float* pushConstants, uint32_t pushConstantSize,
                                         VkDescriptorSet descriptorSet) {
    VkCommandBuffer cmdBuf = m_commandBuffers[m_currentFrame];
    if (cmdBuf == VK_NULL_HANDLE) return;
    if (floatCount == 0 || indexCount == 0) return;

    auto* bufferPool = m_vertexBufferPool.get();
    if (!bufferPool || !bufferPool->isInitialized()) return;

    bindPipelineIfChanged(pipeline, layout);

    if (descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                layout, 0, 1, &descriptorSet, 0, nullptr);
    }

    if (pushConstants && pushConstantSize > 0) {
        vkCmdPushConstants(cmdBuf, layout,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, pushConstantSize, pushConstants);
    }

    auto vertexAlloc = bufferPool->allocateVertexData(
        vertices, floatCount * sizeof(float), m_currentFrame);
    if (vertexAlloc.buffer == VK_NULL_HANDLE) return;

    auto indexAlloc = bufferPool->allocateIndexData(
        indices, indexCount * sizeof(uint16_t), m_currentFrame);
    if (indexAlloc.buffer == VK_NULL_HANDLE) return;

    VkDeviceSize vertexOffset = vertexAlloc.offset;
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexAlloc.buffer, &vertexOffset);
    vkCmdBindIndexBuffer(cmdBuf, indexAlloc.buffer, indexAlloc.offset, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmdBuf, indexCount, 1, 0, 0, 0);
}

void VulkanContext::bindPipelineIfChanged(VkPipeline pipeline, VkPipelineLayout layout) {
    if (pipeline != m_currentPipeline) {
        vkCmdBindPipeline(m_commandBuffers[m_currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        m_currentPipeline = pipeline;
        m_currentLayout = layout;
    }
}

void VulkanContext::pixelToNDC(float pixelX, float pixelY, float& ndcX, float& ndcY) {
    float swapW = static_cast<float>(m_swapChainExtent.width);
    float swapH = static_cast<float>(m_swapChainExtent.height);
    float refW = (m_coordMapWidth > 0) ? m_coordMapWidth : swapW;
    float refH = (m_coordMapHeight > 0) ? m_coordMapHeight : swapH;
    float contentAspect = refW / refH;
    float screenAspect = swapW / swapH;
    float scale, offsetX = 0.0f, offsetY = 0.0f;
    if (contentAspect > screenAspect) {
        scale = swapW / refW;
        offsetY = (swapH - refH * scale) * 0.5f;
    } else {
        scale = swapH / refH;
        offsetX = (swapW - refW * scale) * 0.5f;
    }
    ndcX = ((pixelX * scale + offsetX) / swapW) * 2.0f - 1.0f;
    ndcY = ((pixelY * scale + offsetY) / swapH) * 2.0f - 1.0f;
}

void VulkanContext::pixelSizeToNDC(float pixelW, float pixelH, float& ndcW, float& ndcH) {
    float swapW = static_cast<float>(m_swapChainExtent.width);
    float swapH = static_cast<float>(m_swapChainExtent.height);
    float refW = (m_coordMapWidth > 0) ? m_coordMapWidth : swapW;
    float refH = (m_coordMapHeight > 0) ? m_coordMapHeight : swapH;
    float contentAspect = refW / refH;
    float screenAspect = swapW / swapH;
    float scale = (contentAspect > screenAspect) ? (swapW / refW) : (swapH / refH);
    ndcW = (pixelW * scale / swapW) * 2.0f;
    ndcH = (pixelH * scale / swapH) * 2.0f;
}

void VulkanContext::setCoordinateMapping(int width, int height) {
    m_coordMapWidth = static_cast<float>(width);
    m_coordMapHeight = static_cast<float>(height);
}

// ============================================================================
// Memory helpers
// ============================================================================

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("Failed to find suitable memory type");
    return UINT32_MAX;
}

VkFormat VulkanContext::findSupportedDepthFormat() {
    const std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            LOGI("Found supported depth format: %u", static_cast<uint32_t>(format));
            return format;
        }
    }

    LOGE("Failed to find supported depth format!");
    return VK_FORMAT_D24_UNORM_S8_UINT;
}

// ============================================================================
// Vulkan initialization (moved from VkRenderer)
// ============================================================================

bool VulkanContext::createInstance() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

#if AGENUI_PLATFORM_WINDOWS
    LOGI("Available Vulkan extensions:");
    for (const auto& ext : availableExtensions) {
        LOGI("  - %s", ext.extensionName);
    }
#elif AGENUI_PLATFORM_HARMONYOS
    LOGI("Available Vulkan extensions:");
    for (const auto& ext : availableExtensions) {
        LOGI("  - %s", ext.extensionName);
    }
#endif

    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if AGENUI_PLATFORM_WINDOWS
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions) {
        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            extensions.push_back(glfwExtensions[i]);
            LOGI("Added GLFW extension: %s", glfwExtensions[i]);
        }
    }
#elif AGENUI_PLATFORM_HARMONYOS
    bool foundOHOSExtension = false;
    for (const auto& ext : availableExtensions) {
        if (strcmp(ext.extensionName, "VK_OHOS_surface_extension") == 0 ||
            strcmp(ext.extensionName, "VK_OHOS_SURFACE_EXTENSION_NAME") == 0) {
            extensions.push_back(ext.extensionName);
            foundOHOSExtension = true;
            LOGI("Found OHOS surface extension: %s", ext.extensionName);
            break;
        }
    }

    if (!foundOHOSExtension) {
        for (const auto& ext : availableExtensions) {
            if (strstr(ext.extensionName, "OHOS") != nullptr || strstr(ext.extensionName, "ohos") != nullptr) {
                extensions.push_back(ext.extensionName);
                LOGI("Found OHOS-related extension: %s", ext.extensionName);
                break;
            }
        }
    }
#endif

    for (const auto& ext : availableExtensions) {
        if (strcmp(ext.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
            extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            break;
        }
    }

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    LOGI("Available Vulkan layers (%u):", layerCount);
    for (const auto& layerProperties : availableLayers) {
        LOGI("  - %s", layerProperties.layerName);
    }

    std::vector<const char*> enabledLayers;
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_standard_validation",
        "VK_LAYER_GOOGLE_unique_objects"
    };

    for (const char* layer : validationLayers) {
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layer, layerProperties.layerName) == 0) {
                enabledLayers.push_back(layer);
                LOGI("Enabling validation layer: %s", layer);
                break;
            }
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AgenUIEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    appInfo.pEngineName = "AgenUIEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (!enabledLayers.empty()) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.data();
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        LOGE("Failed to create Vulkan instance");
        return false;
    }

    LOGI("Vulkan instance created successfully");
    return true;
}

bool VulkanContext::createSurface() {
    return createVulkanSurface(m_nativeWindow);
}

bool VulkanContext::createVulkanSurface(void* nativeWindow) {
#if AGENUI_PLATFORM_WINDOWS
    GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(nativeWindow);
    if (!glfwWindow) {
        LOGE("Invalid GLFW window");
        return false;
    }

    VkResult result = glfwCreateWindowSurface(m_instance, glfwWindow, nullptr, &m_surface);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create Vulkan surface: %d", result);
        return false;
    }

    LOGI("Vulkan surface created successfully (GLFW)");
    return true;

#elif AGENUI_PLATFORM_HARMONYOS
    OHNativeWindow* ohWindow = static_cast<OHNativeWindow*>(nativeWindow);
    if (!ohWindow) {
        LOGE("Invalid OHNativeWindow");
        return false;
    }

    auto vkCreateSurfaceOHOS = reinterpret_cast<PFN_vkCreateSurfaceOHOS>(
        vkGetInstanceProcAddr(m_instance, "vkCreateSurfaceOHOS"));

    if (!vkCreateSurfaceOHOS) {
        LOGE("Failed to load vkCreateSurfaceOHOS function");
        return false;
    }

    VkSurfaceCreateInfoOHOS surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.window = ohWindow;

    VkSurfaceKHR surface;
    VkResult result = vkCreateSurfaceOHOS(m_instance, &surfaceCreateInfo, nullptr, &surface);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create Vulkan surface: %d", result);
        return false;
    }

    m_surface = surface;
    LOGI("Vulkan surface created successfully (OHOS)");
    return true;

#else
    #error "Unsupported platform"
#endif
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        uint32_t graphicsFamily = findGraphicsQueueFamily(device);
        uint32_t presentFamily = findPresentQueueFamily(device);

        if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) {
            m_physicalDevice = device;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        LOGE("Failed to find a suitable GPU");
        return false;
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    LOGI("Selected GPU: %s", deviceProperties.deviceName);

    m_depthFormat = findSupportedDepthFormat();
    return true;
}

bool VulkanContext::createDevice() {
    uint32_t graphicsFamily = findGraphicsQueueFamily(m_physicalDevice);
    uint32_t presentFamily = findPresentQueueFamily(m_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        LOGE("Failed to create logical device");
        return false;
    }

    vkGetDeviceQueue(m_device, graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, presentFamily, 0, &m_presentQueue);

    m_descriptorManager = std::make_unique<DescriptorManager>(m_device, m_physicalDevice);

    LOGI("Logical device created successfully");
    return true;
}

uint32_t VulkanContext::findGraphicsQueueFamily(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t VulkanContext::findPresentQueueFamily(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool VulkanContext::createSwapChain() {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

    LOGI("createSwapChain: surface caps currentExtent=%ux%u, requested=%dx%d",
         surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height,
         m_width, m_height);

    VkExtent2D extent;
    if (m_width > 0 && m_height > 0) {
        extent.width = std::max(surfaceCapabilities.minImageExtent.width,
                               std::min(static_cast<uint32_t>(m_width),
                                       surfaceCapabilities.maxImageExtent.width));
        extent.height = std::max(surfaceCapabilities.minImageExtent.height,
                                std::min(static_cast<uint32_t>(m_height),
                                        surfaceCapabilities.maxImageExtent.height));
    } else {
        extent = surfaceCapabilities.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent.width = std::max(surfaceCapabilities.minImageExtent.width,
                                   std::min(static_cast<uint32_t>(m_width),
                                           surfaceCapabilities.maxImageExtent.width));
            extent.height = std::max(surfaceCapabilities.minImageExtent.height,
                                    std::min(static_cast<uint32_t>(m_height),
                                            surfaceCapabilities.maxImageExtent.height));
        }
    }

    VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t graphicsFamily = findGraphicsQueueFamily(m_physicalDevice);
    uint32_t presentFamily = findPresentQueueFamily(m_physicalDevice);

    if (graphicsFamily != presentFamily) {
        uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        LOGE("Failed to create swap chain");
        return false;
    }

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

    m_swapChainFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    m_width = static_cast<int>(extent.width);
    m_height = static_cast<int>(extent.height);

    m_swapChainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewCreateInfo{};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.image = m_swapChainImages[i];
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = m_swapChainFormat;
        viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &viewCreateInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
            LOGE("Failed to create image view");
            return false;
        }
    }

    LOGI("Swap chain created: %u images, %dx%d",
         imageCount, extent.width, extent.height);
    return true;
}

bool VulkanContext::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        LOGE("Failed to create render pass");
        return false;
    }

    // Create LOAD variant for glass pass (preserves existing framebuffer content)
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Separate subpass dependency for LOAD render pass -- must ensure previous
    // framebuffer writes (from CLEAR pass and transfer operations) are visible
    // to the LOAD_OP_LOAD read operation.
    VkSubpassDependency loadDependency{};
    loadDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    loadDependency.dstSubpass = 0;
    loadDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT;
    loadDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_TRANSFER_READ_BIT;
    loadDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    loadDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachmentsLoad = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassLoadInfo{};
    renderPassLoadInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassLoadInfo.attachmentCount = static_cast<uint32_t>(attachmentsLoad.size());
    renderPassLoadInfo.pAttachments = attachmentsLoad.data();
    renderPassLoadInfo.subpassCount = 1;
    renderPassLoadInfo.pSubpasses = &subpass;
    renderPassLoadInfo.dependencyCount = 1;
    renderPassLoadInfo.pDependencies = &loadDependency;

    if (vkCreateRenderPass(m_device, &renderPassLoadInfo, nullptr, &m_renderPassLoad) != VK_SUCCESS) {
        LOGE("Failed to create load render pass");
        return false;
    }

    LOGI("Render pass created successfully with depth attachment");
    return true;
}

bool VulkanContext::createFramebuffers() {
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
    m_depthImages.resize(m_swapChainImageViews.size());
    m_depthImageMemories.resize(m_swapChainImageViews.size());
    m_depthImageViews.resize(m_swapChainImageViews.size());

    for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.extent.width = m_swapChainExtent.width;
        depthImageInfo.extent.height = m_swapChainExtent.height;
        depthImageInfo.extent.depth = 1;
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.format = m_depthFormat;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device, &depthImageInfo, nullptr, &m_depthImages[i]) != VK_SUCCESS) {
            LOGE("Failed to create depth image");
            return false;
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, m_depthImages[i], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthImageMemories[i]) != VK_SUCCESS) {
            LOGE("Failed to allocate depth image memory");
            return false;
        }

        vkBindImageMemory(m_device, m_depthImages[i], m_depthImageMemories[i], 0);

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = m_depthImages[i];
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = m_depthFormat;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &depthViewInfo, nullptr, &m_depthImageViews[i]) != VK_SUCCESS) {
            LOGE("Failed to create depth image view");
            return false;
        }

        VkImageView attachments[] = {
            m_swapChainImageViews[i],
            m_depthImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            LOGE("Failed to create framebuffer");
            return false;
        }
    }

    LOGI("Framebuffers created: %zu with depth buffers", m_swapChainFramebuffers.size());
    return true;
}

bool VulkanContext::createCommandBuffers() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = findGraphicsQueueFamily(m_physicalDevice);

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        LOGE("Failed to create command pool");
        return false;
    }

    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers");
        return false;
    }

    LOGI("Command buffers allocated");
    return true;
}

bool VulkanContext::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            LOGE("Failed to create sync objects");
            return false;
        }
    }

    LOGI("Sync objects created");
    return true;
}

// ============================================================================
// Swapchain recreation
// ============================================================================

void VulkanContext::cleanupSwapChain() {
    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_swapChainFramebuffers.clear();

    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapChainImageViews.clear();

    for (size_t i = 0; i < m_depthImageViews.size(); i++) {
        if (m_depthImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depthImageViews[i], nullptr);
        }
        if (m_depthImages[i] != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_depthImages[i], nullptr);
        }
        if (m_depthImageMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_depthImageMemories[i], nullptr);
        }
    }

    m_depthImageViews.clear();
    m_depthImages.clear();
    m_depthImageMemories.clear();

    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}

void VulkanContext::recreateSwapChain() {
    LOGI("recreateSwapChain: Recreating swap chain with requested size %dx%d", m_width, m_height);

    vkDeviceWaitIdle(m_device);

    cleanupSwapChain();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    if (m_renderPassLoad != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPassLoad, nullptr);
        m_renderPassLoad = VK_NULL_HANDLE;
    }

    if (!createSwapChain()) {
        LOGE("recreateSwapChain: createSwapChain FAILED");
        return;
    }
    if (!createRenderPass()) {
        LOGE("recreateSwapChain: createRenderPass FAILED");
        return;
    }
    if (!createFramebuffers()) {
        LOGE("recreateSwapChain: createFramebuffers FAILED");
        return;
    }
    if (!createCommandBuffers()) {
        LOGE("recreateSwapChain: createCommandBuffers FAILED");
        return;
    }

    LOGI("recreateSwapChain: SUCCESS, actual extent = %dx%d", m_swapChainExtent.width, m_swapChainExtent.height);
}

void VulkanContext::cleanup() {
    // Cleanup vertex buffer pool
    if (m_vertexBufferPool) {
        m_vertexBufferPool->cleanup();
        m_vertexBufferPool.reset();
    }

    // Cleanup descriptor manager
    if (m_descriptorManager) {
        m_descriptorManager->cleanup();
        m_descriptorManager.reset();
    }

    // Cleanup any remaining per-frame buffers and memories
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        for (::VkBuffer buffer : m_cleanupBuffers[i]) {
            vkDestroyBuffer(m_device, buffer, nullptr);
        }
        for (VkDeviceMemory memory : m_cleanupMemories[i]) {
            vkFreeMemory(m_device, memory, nullptr);
        }
        m_cleanupBuffers[i].clear();
        m_cleanupMemories[i].clear();

        if (i < m_renderFinishedSemaphores.size()) {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }
    }

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    if (m_renderPassLoad != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPassLoad, nullptr);
        m_renderPassLoad = VK_NULL_HANDLE;
    }

    cleanupSwapChain();

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    LOGI("Vulkan context cleaned up");
}

} // namespace AgenUIEngine
