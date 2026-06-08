/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * FrameCapture — Vulkan swapchain pixel readback implementation.
 *
 * Implements FrameCapture::capture() declared in VisualTestFramework.h.
 *
 * Pipeline:
 *   1. Create host-visible staging image (LINEAR tiling, TRANSFER_DST)
 *   2. Transition swapchain image to TRANSFER_SRC layout
 *   3. vkCmdBlitImage (or vkCmdCopyImage) swapchain → staging
 *   4. Transition swapchain back to its original layout
 *   5. vkMapMemory → memcpy pixels from staging
 *   6. If format is B8G8R8A8: swap R↔B channels for RGBA8888 output
 *   7. Cleanup staging resources
 *
 * This file requires vulkan/vulkan.h. It is only compiled when
 * BUILD_VISUAL_TESTS=ON and Vulkan SDK is available.
 *
 * Platform notes:
 *   - On HarmonyOS, VkDevice/Memory management uses OH-Vulkan adapters.
 *   - The staging image uses LINEAR tiling for host-visible access.
 *   - Swapchain format detection: VK_FORMAT_B8G8R8A8_SRGB is the most common.
 */

#include "VisualTestFramework.h"

// Only compile the Vulkan implementation when the header is available
#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#include <cstring>
#include <algorithm>

CaptureResult FrameCapture::capture(
    void* device, void* physicalDevice, void* commandPool, void* queue,
    void* swapchainImage, uint32_t width, uint32_t height, uint32_t format)
{
    CaptureResult result;
    result.width  = width;
    result.height = height;

    VkDevice         dev  = static_cast<VkDevice>(device);
    VkPhysicalDevice pdev = static_cast<VkPhysicalDevice>(physicalDevice);
    VkCommandPool    pool = static_cast<VkCommandPool>(commandPool);
    VkQueue          q    = static_cast<VkQueue>(queue);
    VkImage          src  = static_cast<VkImage>(swapchainImage);
    VkFormat         fmt  = static_cast<VkFormat>(format);

    // ── 1. Create staging image (LINEAR, host-visible) ──

    VkImage stagingImage = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkImageCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    stagingInfo.imageType   = VK_IMAGE_TYPE_2D;
    stagingInfo.format      = fmt;
    stagingInfo.extent      = {width, height, 1};
    stagingInfo.mipLevels   = 1;
    stagingInfo.arrayLayers = 1;
    stagingInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    stagingInfo.tiling      = VK_IMAGE_TILING_LINEAR;
    stagingInfo.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    stagingInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult vr = vkCreateImage(dev, &stagingInfo, nullptr, &stagingImage);
    if (vr != VK_SUCCESS) {
        result.valid = false;
        return result;
    }

    // Allocate memory for staging image
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(dev, stagingImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    // Find host-visible memory type
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pdev, &memProps);
    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        vkDestroyImage(dev, stagingImage, nullptr);
        result.valid = false;
        return result;
    }
    allocInfo.memoryTypeIndex = memTypeIndex;

    vr = vkAllocateMemory(dev, &allocInfo, nullptr, &stagingMemory);
    if (vr != VK_SUCCESS) {
        vkDestroyImage(dev, stagingImage, nullptr);
        result.valid = false;
        return result;
    }

    vkBindImageMemory(dev, stagingImage, stagingMemory, 0);

    // ── 2. Create command buffer ──

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = pool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmdBuf;
    vkAllocateCommandBuffers(dev, &cmdAlloc, &cmdBuf);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // ── 3. Transition swapchain to TRANSFER_SRC ──

    VkImageMemoryBarrier srcBarrier{};
    srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    srcBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    srcBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    srcBarrier.image         = src;
    srcBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &srcBarrier);

    // ── 4. Transition staging to TRANSFER_DST ──

    VkImageMemoryBarrier dstBarrier{};
    dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    dstBarrier.srcAccessMask = 0;
    dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstBarrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    dstBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstBarrier.image         = stagingImage;
    dstBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &dstBarrier);

    // ── 5. Blit/Copy swapchain → staging ──

    VkImageBlit blitRegion{};
    blitRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.srcOffsets[0] = {0, 0, 0};
    blitRegion.srcOffsets[1] = {static_cast<int32_t>(width),
                                static_cast<int32_t>(height), 1};
    blitRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.dstOffsets[0] = {0, 0, 0};
    blitRegion.dstOffsets[1] = {static_cast<int32_t>(width),
                                static_cast<int32_t>(height), 1};

    // Blit supports format conversion; fall back to Copy if layouts match
    vkCmdBlitImage(cmdBuf,
        src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blitRegion, VK_FILTER_NEAREST);

    // ── 6. Transition staging to GENERAL for host read ──

    VkImageMemoryBarrier readBarrier{};
    readBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    readBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    readBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    readBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    readBarrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
    readBarrier.image         = stagingImage;
    readBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &readBarrier);

    // ── 7. Restore swapchain layout ──

    VkImageMemoryBarrier restoreBarrier{};
    restoreBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    restoreBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    restoreBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    restoreBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    restoreBarrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    restoreBarrier.image         = src;
    restoreBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &restoreBarrier);

    // ── 8. Submit and wait ──

    vkEndCommandBuffer(cmdBuf);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    vkQueueSubmit(q, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(q);

    // ── 9. Map staging memory and read pixels ──

    VkImageSubresource subresource{};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;
    subresource.arrayLayer = 0;

    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(dev, stagingImage, &subresource, &layout);

    void* mappedData = nullptr;
    vkMapMemory(dev, stagingMemory, 0, VK_WHOLE_SIZE, 0, &mappedData);

    result.pixels.resize(width * height * 4);

    bool isBGR = (fmt == VK_FORMAT_B8G8R8A8_SRGB ||
                  fmt == VK_FORMAT_B8G8R8A8_UNORM ||
                  fmt == VK_FORMAT_B8G8R8A8_SNORM);

    auto* srcPtr = static_cast<uint8_t*>(mappedData) + layout.offset;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t srcIdx = static_cast<uint32_t>(
                y * layout.rowPitch + x * 4);
            uint32_t dstIdx = (y * width + x) * 4;

            if (isBGR) {
                // B8G8R8A8 → RGBA8888
                result.pixels[dstIdx + 0] = srcPtr[srcIdx + 2]; // R ← B position
                result.pixels[dstIdx + 1] = srcPtr[srcIdx + 1]; // G
                result.pixels[dstIdx + 2] = srcPtr[srcIdx + 0]; // B ← R position
                result.pixels[dstIdx + 3] = srcPtr[srcIdx + 3]; // A
            } else {
                // R8G8B8A8 → RGBA8888 (direct copy)
                result.pixels[dstIdx + 0] = srcPtr[srcIdx + 0];
                result.pixels[dstIdx + 1] = srcPtr[srcIdx + 1];
                result.pixels[dstIdx + 2] = srcPtr[srcIdx + 2];
                result.pixels[dstIdx + 3] = srcPtr[srcIdx + 3];
            }
        }
    }

    vkUnmapMemory(dev, stagingMemory);

    // ── 10. Cleanup ──

    vkFreeCommandBuffers(dev, pool, 1, &cmdBuf);
    vkFreeMemory(dev, stagingMemory, nullptr);
    vkDestroyImage(dev, stagingImage, nullptr);

    result.valid = true;
    return result;
}

#else // No vulkan/vulkan.h available

// Stub implementation when Vulkan is not available
CaptureResult FrameCapture::capture(
    void* /*device*/, void* /*physicalDevice*/, void* /*commandPool*/,
    void* /*queue*/, void* /*swapchainImage*/,
    uint32_t width, uint32_t height, uint32_t /*format*/)
{
    // Return empty/invalid result — Vulkan not available
    CaptureResult result;
    result.width  = width;
    result.height = height;
    result.valid  = false;
    return result;
}

#endif // __has_include(<vulkan/vulkan.h>)
