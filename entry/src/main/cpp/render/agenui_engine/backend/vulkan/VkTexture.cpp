/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Vulkan Texture Implementation
 */

#include "VkTexture.h"
#include "VkBuffer.h"
#include <stdexcept>
#include <vector>
#include <cstring>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"
#include "logger_common.h"

#if defined(AGENUI_PLATFORM_HARMONYOS)
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#endif

namespace AgenUIEngine {

VkTexture::VkTexture() {
}

VkTexture::~VkTexture() {
}

// -------------------------------------------------------------------------
// ITexture interface implementation
// -------------------------------------------------------------------------

void VkTexture::update(const void* data, uint32_t x, uint32_t y,
                       uint32_t width, uint32_t height) {
    if (m_device != VK_NULL_HANDLE) {
        // Need to find commandPool and graphicsQueue - these are not stored.
        // This is a limitation - the caller should use updateData() directly
        // when they have access to commandPool and graphicsQueue.
        // The ITexture::update() interface is kept for completeness.
    }
}

void VkTexture::generateMipmaps() {
    // TODO: Implement mipmap generation
}

uint32_t VkTexture::getWidth() const {
    return m_width;
}

uint32_t VkTexture::getHeight() const {
    return m_height;
}

Core::TextureFormat VkTexture::getFormat() const {
    return m_coreFormat;
}

uint64_t VkTexture::getNativeHandle() const {
    return reinterpret_cast<uint64_t>(m_image);
}

// -------------------------------------------------------------------------
// Vulkan-specific creation methods
// -------------------------------------------------------------------------

bool VkTexture::create(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool, VkQueue graphicsQueue,
                    uint32_t width, uint32_t height, const unsigned char* pixels,
                    VkFormat format) {
    m_width = width;
    m_height = height;
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_vkFormat = format;
    m_coreFormat = vkFormatToCore(format);

    // Calculate image size based on format
    size_t bytesPerPixel = 4;  // Default for RGBA
    if (format == VK_FORMAT_R8_UNORM) {
        bytesPerPixel = 1;
    }
    VkDeviceSize imageSize = width * height * bytesPerPixel;

    // Create staging buffer using VkBuffer class
    AgenUIEngine::VkBuffer stagingBuffer;
    if (!stagingBuffer.createStaging(device, physicalDevice, imageSize)) {
        LOGE("VkTexture: Failed to create staging buffer");
        return false;
    }

    // Copy data to staging buffer
    stagingBuffer.updateData(pixels, imageSize);

    // Create texture image
    if (!createImage(device, physicalDevice, width, height, VK_SAMPLE_COUNT_1_BIT,
                    m_vkFormat, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory)) {
        stagingBuffer.cleanup(device);
        return false;
    }

    // Transition and copy
    transitionImageLayout(device, commandPool, graphicsQueue, m_image,
                         m_vkFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer.getBuffer(),
                     m_image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    transitionImageLayout(device, commandPool, graphicsQueue, m_image,
                         m_vkFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Cleanup staging buffer
    stagingBuffer.cleanup(device);

    // Create image view
    if (!createImageView(device, m_image, m_vkFormat)) {
        return false;
    }

    // Create sampler
    if (!createTextureSampler(device)) {
        return false;
    }

    return true;
}

bool VkTexture::createEmpty(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue graphicsQueue,
                         uint32_t width, uint32_t height, VkFormat format) {
    m_width = width;
    m_height = height;
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_vkFormat = format;
    m_coreFormat = vkFormatToCore(format);

    // Calculate image size based on format
    size_t bytesPerPixel = 4;  // Default for RGBA
    if (format == VK_FORMAT_R8_UNORM) {
        bytesPerPixel = 1;
    }
    VkDeviceSize imageSize = width * height * bytesPerPixel;

    // Create staging buffer for initialization
    AgenUIEngine::VkBuffer stagingBuffer;
    if (!stagingBuffer.createStaging(device, physicalDevice, imageSize)) {
        LOGE("VkTexture: Failed to create staging buffer for empty texture");
        return false;
    }

    // Initialize with transparent black (zeros)
    std::vector<uint8_t> zeroData(imageSize, 0);
    stagingBuffer.updateData(zeroData.data(), imageSize);

    // Create texture image with transfer dst usage for updates
    if (!createImage(device, physicalDevice, width, height, VK_SAMPLE_COUNT_1_BIT,
                    m_vkFormat, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory)) {
        stagingBuffer.cleanup(device);
        return false;
    }

    // Transition and copy initial data
    transitionImageLayout(device, commandPool, graphicsQueue, m_image,
                         m_vkFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer.getBuffer(),
                     m_image, width, height);
    transitionImageLayout(device, commandPool, graphicsQueue, m_image,
                         m_vkFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Cleanup staging buffer
    stagingBuffer.cleanup(device);

    // Create image view
    if (!createImageView(device, m_image, m_vkFormat)) {
        return false;
    }

    // Create sampler
    if (!createTextureSampler(device)) {
        return false;
    }

    return true;
}

bool VkTexture::updateData(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                        uint32_t xOffset, uint32_t yOffset,
                        uint32_t width, uint32_t height, const unsigned char* pixels) {
    // Calculate image size based on format
    size_t bytesPerPixel = 4;  // Default for RGBA
    if (m_vkFormat == VK_FORMAT_R8_UNORM) {
        bytesPerPixel = 1;
    }
    VkDeviceSize imageSize = width * height * bytesPerPixel;

    // Create staging buffer
    AgenUIEngine::VkBuffer stagingBuffer;
    if (!stagingBuffer.createStaging(device, m_physicalDevice, imageSize)) {
        LOGE("VkTexture: Failed to create staging buffer for update");
        return false;
    }

    // Copy data to staging buffer
    stagingBuffer.updateData(pixels, imageSize);

    // Transition to transfer dst optimal for update
    transitionImageLayout(device, commandPool, graphicsQueue, m_image,
                         m_vkFormat, m_layout,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy buffer to image region
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(xOffset), static_cast<int32_t>(yOffset), 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.getBuffer(), m_image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

    // Transition back to shader read only
    transitionImageLayout(device, commandPool, graphicsQueue, m_image,
                         m_vkFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Cleanup staging buffer
    stagingBuffer.cleanup(device);

    return true;
}

bool VkTexture::loadFromFile(VkDevice device, VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool, VkQueue graphicsQueue,
                          const std::string& filepath) {
    LOGI("VkTexture: Loading image from file: %s", filepath.c_str());

    // Load image using stb_image
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        LOGE("VkTexture: Failed to load image file: %s", filepath.c_str());
        return false;
    }

    LOGI("VkTexture: Loaded image %dx%d with %d channels", width, height, channels);

    // Create texture from loaded pixel data
    bool result = create(device, physicalDevice, commandPool, graphicsQueue,
                        width, height, pixels, VK_FORMAT_R8G8B8A8_UNORM);

    // Free the loaded image data
    stbi_image_free(pixels);

    if (!result) {
        LOGE("VkTexture: Failed to create texture from file: %s", filepath.c_str());
        return false;
    }

    LOGI("VkTexture: Successfully loaded texture from file: %s", filepath.c_str());
    return true;
}

#if defined(AGENUI_PLATFORM_HARMONYOS)
bool VkTexture::loadFromRawFile(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkCommandPool commandPool, VkQueue graphicsQueue,
                             void* resourceManager, const std::string& filename,
                             VkFormat format) {
    LOGI("VkTexture: Loading image from rawfile: %s", filename.c_str());

    NativeResourceManager* mgr = static_cast<NativeResourceManager*>(resourceManager);
    if (!mgr) {
        LOGE("VkTexture: Invalid resource manager");
        return false;
    }

    // Open raw file using HarmonyOS API
    RawFile* rawFile = OH_ResourceManager_OpenRawFile(mgr, filename.c_str());
    if (!rawFile) {
        LOGE("VkTexture: Failed to open rawfile: %s", filename.c_str());
        return false;
    }

    // Get file size
    int fileSize = OH_ResourceManager_GetRawFileSize(rawFile);
    if (fileSize <= 0) {
        LOGE("VkTexture: Invalid rawfile size: %d", fileSize);
        OH_ResourceManager_CloseRawFile(rawFile);
        return false;
    }

    LOGI("VkTexture: Rawfile size: %d bytes", fileSize);

    // Read file content
    std::vector<uint8_t> fileData(fileSize);
    int bytesRead = OH_ResourceManager_ReadRawFile(rawFile, fileData.data(), fileSize);
    OH_ResourceManager_CloseRawFile(rawFile);

    if (bytesRead != fileSize) {
        LOGE("VkTexture: Failed to read complete rawfile. Expected %d, got %d", fileSize, bytesRead);
        return false;
    }

    // Decode image using stb_image from memory
    int width, height, channels;
    stbi_uc* pixels = stbi_load_from_memory(fileData.data(), fileSize, &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        LOGE("VkTexture: Failed to decode image from rawfile: %s", filename.c_str());
        return false;
    }

    LOGI("VkTexture: Decoded image %dx%d with %d channels", width, height, channels);

    // Create texture from decoded pixel data
    bool result = create(device, physicalDevice, commandPool, graphicsQueue,
                        width, height, pixels, format);

    // Free the decoded image data
    stbi_image_free(pixels);

    if (!result) {
        LOGE("VkTexture: Failed to create texture from rawfile: %s", filename.c_str());
        return false;
    }

    LOGI("VkTexture: Successfully loaded texture from rawfile: %s", filename.c_str());
    return true;
}
#endif

void VkTexture::cleanup(VkDevice device) {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_imageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_imageMemory, nullptr);
        m_imageMemory = VK_NULL_HANDLE;
    }
}

// -------------------------------------------------------------------------
// Format conversion utility
// -------------------------------------------------------------------------

Core::TextureFormat VkTexture::vkFormatToCore(VkFormat vkFormat) {
    switch (vkFormat) {
        case VK_FORMAT_R8G8B8A8_SRGB:
            return Core::TextureFormat::R8G8B8A8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return Core::TextureFormat::R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8_UNORM:
            return Core::TextureFormat::R8G8B8_UNORM;
        case VK_FORMAT_R8_UNORM:
            return Core::TextureFormat::R8_UNORM;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return Core::TextureFormat::R32G32B32A32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return Core::TextureFormat::D24_UNORM_S8_UINT;
        default:
            return Core::TextureFormat::UNKNOWN;
    }
}

// -------------------------------------------------------------------------
// Private helpers
// -------------------------------------------------------------------------

bool VkTexture::createImage(VkDevice device, VkPhysicalDevice physicalDevice,
                         uint32_t width, uint32_t height, VkSampleCountFlagBits numSamples,
                         VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }

    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        return false;
    }

    vkBindImageMemory(device, image, imageMemory, 0);

    return true;
}

bool VkTexture::createImageView(VkDevice device, VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    // Fix color channel order for BGRA formats (e.g. swapchain images).
    // RGBA formats upload data in R,G,B order — no swizzle needed.
    if (format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM) {
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    } else {
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    }

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool VkTexture::createTextureSampler(VkDevice device) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        return false;
    }

    return true;
}

void VkTexture::transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                                   VkImage image, VkFormat format, VkImageLayout oldLayout,
                                   VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == newLayout) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return;
    } else {
        LOGE("VkTexture: Unsupported layout transition: %d -> %d", oldLayout, newLayout);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VkTexture::copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                               ::VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

} // namespace AgenUIEngine
