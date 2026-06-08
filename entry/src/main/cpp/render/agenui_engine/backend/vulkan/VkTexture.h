/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Vulkan Texture - implements Core::ITexture directly
 * No adapter layer needed.
 */

#ifndef AGENUI_ENGINE_VK_TEXTURE_H
#define AGENUI_ENGINE_VK_TEXTURE_H

#include "core/IGraphicsAPI.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AgenUIEngine {

/**
 * @brief Vulkan texture class implementing Core::ITexture
 *
 * Manages Vulkan image, image view, sampler, and associated memory.
 * Provides the abstract ITexture interface for cross-platform compatibility.
 */
class VkTexture : public Core::ITexture {
public:
    VkTexture();
    ~VkTexture() override;

    // -------------------------------------------------------------------------
    // ITexture interface implementation
    // -------------------------------------------------------------------------

    void update(const void* data, uint32_t x, uint32_t y,
               uint32_t width, uint32_t height) override;
    void generateMipmaps() override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    Core::TextureFormat getFormat() const override;
    uint64_t getNativeHandle() const override;

    // -------------------------------------------------------------------------
    // Vulkan-specific creation methods
    // -------------------------------------------------------------------------

    bool create(VkDevice device, VkPhysicalDevice physicalDevice,
               VkCommandPool commandPool, VkQueue graphicsQueue,
               uint32_t width, uint32_t height, const unsigned char* pixels,
               VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

    bool loadFromFile(VkDevice device, VkPhysicalDevice physicalDevice,
                     VkCommandPool commandPool, VkQueue graphicsQueue,
                     const std::string& filepath);

    bool loadFromRawFile(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool, VkQueue graphicsQueue,
                        void* resourceManager, const std::string& filename,
                        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

    bool createEmpty(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool, VkQueue graphicsQueue,
                    uint32_t width, uint32_t height,
                    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

    bool updateData(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                   uint32_t xOffset, uint32_t yOffset,
                   uint32_t width, uint32_t height, const unsigned char* pixels);

    void cleanup(VkDevice device);

    // -------------------------------------------------------------------------
    // Vulkan getters
    // -------------------------------------------------------------------------

    VkImage getImage() const { return m_image; }
    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }
    VkDeviceMemory getImageMemory() const { return m_imageMemory; }
    VkImageLayout getLayout() const { return m_layout; }
    void setLayout(VkImageLayout layout) { m_layout = layout; }

    // -------------------------------------------------------------------------
    // Format conversion utility
    // -------------------------------------------------------------------------

    static Core::TextureFormat vkFormatToCore(VkFormat vkFormat);

private:
    bool createImage(VkDevice device, VkPhysicalDevice physicalDevice,
                    uint32_t width, uint32_t height, VkSampleCountFlagBits numSamples,
                    VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    bool createImageView(VkDevice device, VkImage image, VkFormat format);
    bool createTextureSampler(VkDevice device);

    void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                              VkImage image, VkFormat format, VkImageLayout oldLayout,
                              VkImageLayout newLayout);

    void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue,
                          ::VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkFormat m_vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    Core::TextureFormat m_coreFormat = Core::TextureFormat::R8G8B8A8_UNORM;
};

} // namespace AgenUIEngine

#endif // AGENUI_ENGINE_VK_TEXTURE_H
