#ifndef AGENUI_VULKAN_CONTEXT_H
#define AGENUI_VULKAN_CONTEXT_H

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define AGENUI_PLATFORM_WINDOWS 1
#elif defined(__OHOS__) || defined(AGENUI_PLATFORM_HARMONYOS)
    #define AGENUI_PLATFORM_HARMONYOS 1
#endif

// Platform-specific includes
#if AGENUI_PLATFORM_WINDOWS
    #define VK_USE_PLATFORM_WIN32_KHR 1
    #include <vulkan/vulkan.h>
    struct GLFWwindow;
#elif AGENUI_PLATFORM_HARMONYOS
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_ohos.h>
    #include <native_window/external_window.h>
    #include <rawfile/raw_file.h>
    #include <rawfile/raw_file_manager.h>
#else
    #include <vulkan/vulkan.h>
#endif

#include <string>
#include <vector>
#include <memory>
#include "DynamicVertexBufferPool.h"
#include "DescriptorManager.h"
#include "VkBuffer.h"

namespace AgenUIEngine {

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool initialize(void* nativeWindow, int width, int height);
    void cleanup();

    // Frame lifecycle
    void beginFrame();
    void endFrame();

    // Swapchain recreation
    void recreateSwapChain();
    void cleanupSwapChain();
    void updateSwapChainExtent(int width, int height);

    // Background color
    void setBackgroundColor(float r, float g, float b, float a);

    // Getters - Vulkan core objects
    VkInstance getInstance() const { return m_instance; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice getDevice() const { return m_device; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkSwapchainKHR getSwapChain() const { return m_swapChain; }
    VkRenderPass getRenderPass() const { return m_renderPass; }
    VkRenderPass getRenderPassLoad() const { return m_renderPassLoad; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    VkCommandBuffer getCurrentCommandBuffer() const { return m_commandBuffers[m_currentFrame]; }
    VkFormat getSwapChainFormat() const { return m_swapChainFormat; }
    VkExtent2D getSwapChainExtent() const { return m_swapChainExtent; }
    VkFormat getDepthFormat() const { return m_depthFormat; }

    // Frame state
    uint32_t getCurrentFrame() const { return m_currentFrame; }
    uint32_t getCurrentImageIndex() const { return m_currentImageIndex; }
    static const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    // Swap chain images (needed for glass effect copy)
    const std::vector<VkImage>& getSwapChainImages() const { return m_swapChainImages; }
    const std::vector<VkFramebuffer>& getSwapChainFramebuffers() const { return m_swapChainFramebuffers; }

    // Rendering helpers
    void drawVertexBuffer(VkPipeline pipeline, VkPipelineLayout layout,
                          const std::vector<float>& vertices,
                          const std::vector<uint16_t>& indices,
                          VkDescriptorSet descriptorSet = VK_NULL_HANDLE);
    void drawVertexBufferFromPool(VkPipeline pipeline, VkPipelineLayout layout,
                                   const std::vector<float>& vertices,
                                   const std::vector<uint16_t>& indices,
                                   VkDescriptorSet descriptorSet = VK_NULL_HANDLE);
    void drawWithTempBuffers(const std::vector<float>& vertices,
                             const std::vector<uint16_t>& indices,
                             VkPipeline pipeline, VkPipelineLayout layout,
                             const float* pushConstants, uint32_t pushConstantSize,
                             VkDescriptorSet descriptorSet = VK_NULL_HANDLE);
    // Raw-pointer overload — avoids std::vector heap allocation for callers
    void drawWithTempBuffers(const float* vertices, uint32_t floatCount,
                             const uint16_t* indices, uint32_t indexCount,
                             VkPipeline pipeline, VkPipelineLayout layout,
                             const float* pushConstants, uint32_t pushConstantSize,
                             VkDescriptorSet descriptorSet = VK_NULL_HANDLE);
    void bindPipelineIfChanged(VkPipeline pipeline, VkPipelineLayout layout);

    // Coordinate conversion
    void pixelToNDC(float pixelX, float pixelY, float& ndcX, float& ndcY);
    void pixelSizeToNDC(float pixelW, float pixelH, float& ndcW, float& ndcH);
    void setCoordinateMapping(int width, int height);

    // Memory helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findSupportedDepthFormat();

    // Resource accessors
    DynamicVertexBufferPool* getVertexBufferPool() const { return m_vertexBufferPool.get(); }
    DescriptorManager* getDescriptorManager() const { return m_descriptorManager.get(); }
    VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
    void setDescriptorPool(VkDescriptorPool pool) { m_descriptorPool = pool; }

    // Per-frame cleanup lists
    std::vector<::VkBuffer>& getCleanupBuffers(uint32_t frame) { return m_cleanupBuffers[frame]; }
    std::vector<VkDeviceMemory>& getCleanupMemories(uint32_t frame) { return m_cleanupMemories[frame]; }

    // Wait for device idle
    void waitIdle();

    // Width/height accessors
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    // Platform-specific resource manager
#if AGENUI_PLATFORM_HARMONYOS
    NativeResourceManager* getResourceManager() const { return m_resourceManager; }
    void setResourceManager(NativeResourceManager* rm) { m_resourceManager = rm; }
#endif

private:
    bool createInstance();
    bool createSurface();
    bool createVulkanSurface(void* nativeWindow);
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapChain();
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandBuffers();
    bool createSyncObjects();

    uint32_t findGraphicsQueueFamily(VkPhysicalDevice device);
    uint32_t findPresentQueueFamily(VkPhysicalDevice device);

    // Vulkan objects
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkRenderPass m_renderPassLoad = VK_NULL_HANDLE;  // LOAD_OP_LOAD for glass pass

    // Swap chain images and framebuffers
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    VkFormat m_swapChainFormat;
    VkExtent2D m_swapChainExtent;

    // Depth buffer resources
    std::vector<VkImage> m_depthImages;
    std::vector<VkDeviceMemory> m_depthImageMemories;
    std::vector<VkImageView> m_depthImageViews;
    VkFormat m_depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    // Clear color for background
    float m_clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};

    // Command buffers
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Synchronization
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;

    // Queues
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    // Native window handle
    void* m_nativeWindow = nullptr;

    // Frame tracking
    uint32_t m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;

    // Per-frame cleanup lists
    std::vector<::VkBuffer> m_cleanupBuffers[MAX_FRAMES_IN_FLIGHT];
    std::vector<VkDeviceMemory> m_cleanupMemories[MAX_FRAMES_IN_FLIGHT];

    // Dynamic vertex buffer pool
    std::unique_ptr<DynamicVertexBufferPool> m_vertexBufferPool;
    bool m_useVertexBufferPool = true;

    // Descriptor manager
    std::unique_ptr<DescriptorManager> m_descriptorManager;

    // External descriptor pool (from VkRenderer, reset each frame in beginFrame)
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    // Dimensions
    int m_width = 0;
    int m_height = 0;

    // Coordinate mapping (for aspect-ratio preserving pixelToNDC)
    float m_coordMapWidth = 0.0f;   // 0 means use swapchain extent
    float m_coordMapHeight = 0.0f;

    // Current pipeline state
    VkPipeline m_currentPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_currentLayout = VK_NULL_HANDLE;

#if AGENUI_PLATFORM_HARMONYOS
    NativeResourceManager* m_resourceManager = nullptr;
#endif
};

} // namespace AgenUIEngine

#endif // AGENUI_VULKAN_CONTEXT_H
