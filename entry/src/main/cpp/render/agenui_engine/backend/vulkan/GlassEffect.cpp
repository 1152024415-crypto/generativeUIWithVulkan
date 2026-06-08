#include "GlassEffect.h"
#include "VulkanContext.h"
#include "PipelineManager.h"
#include "logger_common.h"

namespace AgenUIEngine {

GlassEffect::~GlassEffect() {
    // Caller should invoke cleanup() before destruction.
}

// ---------------------------------------------------------------------------
// Descriptor set layout
// ---------------------------------------------------------------------------

bool GlassEffect::ensureDescriptorSetLayout(VkDevice device) {
    if (m_bgDescriptorSetLayout != VK_NULL_HANDLE) return true;

    VkDescriptorSetLayoutBinding bgBinding{};
    bgBinding.binding = 1;
    bgBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bgBinding.descriptorCount = 1;
    bgBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &bgBinding;
    return vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_bgDescriptorSetLayout) == VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// Background texture creation
// ---------------------------------------------------------------------------

bool GlassEffect::createBackgroundTexture(VulkanContext& ctx, ImagePipeline* imagePipeline) {
    if (m_bgTextureCreated) return true;
    VkDevice device = ctx.getDevice();
    VkExtent2D extent = ctx.getSwapChainExtent();
    if (extent.width == 0 || extent.height == 0) return false;
    if (!ensureDescriptorSetLayout(device)) return false;

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1;
    imageInfo.format = ctx.getSwapChainFormat();
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &imageInfo, nullptr, &m_bgImage) != VK_SUCCESS) return false;

    // Allocate memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_bgImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_bgImageMemory) != VK_SUCCESS) {
        vkDestroyImage(device, m_bgImage, nullptr); m_bgImage = VK_NULL_HANDLE; return false;
    }
    vkBindImageMemory(device, m_bgImage, m_bgImageMemory, 0);

    // Image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_bgImage; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = ctx.getSwapChainFormat();
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_bgImageView) != VK_SUCCESS) return false;

    // Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE; samplerInfo.maxLod = 1.0f;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_bgSampler) != VK_SUCCESS) return false;

    // Transition layout
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandPool = ctx.getCommandPool(); cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer cmdBuf; vkAllocateCommandBuffers(device, &cmdAlloc, &cmdBuf);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_bgImage; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmdBuf);
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount = 1; si.pCommandBuffers = &cmdBuf;
    vkQueueSubmit(ctx.getGraphicsQueue(), 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.getGraphicsQueue());
    vkFreeCommandBuffers(device, ctx.getCommandPool(), 1, &cmdBuf);

    // Descriptor pool
    VkDescriptorPoolSize poolSize{}; poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{}; poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize; poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_bgDescriptorPool) != VK_SUCCESS) return false;

    // Rounded-rect compatible descriptor set (binding=1)
    VkDescriptorSetAllocateInfo dsAlloc{}; dsAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAlloc.descriptorPool = m_bgDescriptorPool; dsAlloc.descriptorSetCount = 1; dsAlloc.pSetLayouts = &m_bgDescriptorSetLayout;
    if (vkAllocateDescriptorSets(device, &dsAlloc, &m_bgDescriptorSet) != VK_SUCCESS) return false;
    VkDescriptorImageInfo imgDS{}; imgDS.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgDS.imageView = m_bgImageView; imgDS.sampler = m_bgSampler;
    VkWriteDescriptorSet w{}; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = m_bgDescriptorSet; w.dstBinding = 1; w.dstArrayElement = 0;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w.descriptorCount = 1; w.pImageInfo = &imgDS;
    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);

    // Image-pipeline-compatible descriptor set (binding=0, for fullscreen restore)
    if (imagePipeline && imagePipeline->getDescriptorPool() != VK_NULL_HANDLE) {
        VkDescriptorSetLayout imgLayout = imagePipeline->getDescriptorSetLayout();
        VkDescriptorSetAllocateInfo imgAlloc{}; imgAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        imgAlloc.descriptorPool = imagePipeline->getDescriptorPool();
        imgAlloc.descriptorSetCount = 1; imgAlloc.pSetLayouts = &imgLayout;
        if (vkAllocateDescriptorSets(device, &imgAlloc, &m_bgImageDescriptorSet) == VK_SUCCESS) {
            VkDescriptorImageInfo imgInfo{}; imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgInfo.imageView = m_bgImageView; imgInfo.sampler = m_bgSampler;
            VkWriteDescriptorSet iw{}; iw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            iw.dstSet = m_bgImageDescriptorSet; iw.dstBinding = 0; iw.dstArrayElement = 0;
            iw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; iw.descriptorCount = 1; iw.pImageInfo = &imgInfo;
            vkUpdateDescriptorSets(device, 1, &iw, 0, nullptr);
        }
    }

    m_ctx = &ctx;
    m_bgTextureCreated = true;
    return true;
}

// ---------------------------------------------------------------------------
// Framebuffer copy
// ---------------------------------------------------------------------------

void GlassEffect::copyFramebufferToBackground(VulkanContext& ctx, ImagePipeline* imagePipeline) {
    if (!m_bgTextureCreated && !createBackgroundTexture(ctx, imagePipeline)) return;
    VkCommandBuffer cmdBuf = ctx.getCurrentCommandBuffer();
    if (cmdBuf == VK_NULL_HANDLE) return;
    VkImage swapImg = ctx.getSwapChainImages()[ctx.getCurrentImageIndex()];
    VkExtent2D ext = ctx.getSwapChainExtent();

    auto makeBarrier = [](VkImage img, VkAccessFlags src, VkAccessFlags dst, VkImageLayout oldL, VkImageLayout newL) {
        VkImageMemoryBarrier b{}; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask = src; b.dstAccessMask = dst; b.oldLayout = oldL; b.newLayout = newL;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        return b;
    };

    auto srcB = makeBarrier(swapImg, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,nullptr,0,nullptr,1,&srcB);

    auto dstB = makeBarrier(m_bgImage, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,nullptr,0,nullptr,1,&dstB);

    VkImageCopy copy{}; copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.extent = {ext.width, ext.height, 1};
    vkCmdCopyImage(cmdBuf, swapImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_bgImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    auto bgR = makeBarrier(m_bgImage, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,nullptr,0,nullptr,1,&bgR);

    auto srcR = makeBarrier(swapImg, VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,nullptr,0,nullptr,1,&srcR);
}

// ---------------------------------------------------------------------------
// Render pass control
// ---------------------------------------------------------------------------

void GlassEffect::endRenderPassForCopy(VulkanContext& ctx) {
    VkCommandBuffer cmdBuf = ctx.getCurrentCommandBuffer();
    if (cmdBuf == VK_NULL_HANDLE) return;
    vkCmdEndRenderPass(cmdBuf);
}

void GlassEffect::beginRenderPassAfterCopy(VulkanContext& ctx, ImagePipeline* imagePipeline) {
    (void)imagePipeline;
    VkCommandBuffer cmdBuf = ctx.getCurrentCommandBuffer();
    if (cmdBuf == VK_NULL_HANDLE) return;
    VkExtent2D ext = ctx.getSwapChainExtent();

    // Use LOAD render pass to preserve existing framebuffer content after the copy.
    // The CLEAR variant would wipe everything rendered before the glass pass.
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = ctx.getRenderPassLoad();
    rpInfo.framebuffer = ctx.getSwapChainFramebuffers()[ctx.getCurrentImageIndex()];
    rpInfo.renderArea = {{0,0}, ext};
    // No clear values needed — LOAD_OP_LOAD preserves previous content
    vkCmdBeginRenderPass(cmdBuf, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{}; vp.width = (float)ext.width; vp.height = (float)ext.height; vp.maxDepth = 1.0f;
    VkRect2D sc{}; sc.extent = ext;
    vkCmdSetViewport(cmdBuf, 0, 1, &vp); vkCmdSetScissor(cmdBuf, 0, 1, &sc);
    ctx.bindPipelineIfChanged(VK_NULL_HANDLE, VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

bool GlassEffect::ensureResources(VulkanContext& ctx, ImagePipeline* imagePipeline) {
    if (!createBackgroundTexture(ctx, imagePipeline)) return false;
    return true;
}

void GlassEffect::preparePass(VulkanContext& ctx, ImagePipeline* imagePipeline) {
    endRenderPassForCopy(ctx);
    copyFramebufferToBackground(ctx, imagePipeline);
    beginRenderPassAfterCopy(ctx, imagePipeline);
}

void GlassEffect::cleanup(VkDevice device) {
    m_bgDescriptorSet = VK_NULL_HANDLE; m_bgImageDescriptorSet = VK_NULL_HANDLE;
    if (m_bgDescriptorPool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, m_bgDescriptorPool, nullptr); m_bgDescriptorPool = VK_NULL_HANDLE; }
    if (m_bgDescriptorSetLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, m_bgDescriptorSetLayout, nullptr); m_bgDescriptorSetLayout = VK_NULL_HANDLE; }
    if (m_bgSampler != VK_NULL_HANDLE) { vkDestroySampler(device, m_bgSampler, nullptr); m_bgSampler = VK_NULL_HANDLE; }
    if (m_bgImageView != VK_NULL_HANDLE) { vkDestroyImageView(device, m_bgImageView, nullptr); m_bgImageView = VK_NULL_HANDLE; }
    if (m_bgImage != VK_NULL_HANDLE) { vkDestroyImage(device, m_bgImage, nullptr); m_bgImage = VK_NULL_HANDLE; }
    if (m_bgImageMemory != VK_NULL_HANDLE) { vkFreeMemory(device, m_bgImageMemory, nullptr); m_bgImageMemory = VK_NULL_HANDLE; }
    m_bgTextureCreated = false;
    m_ctx = nullptr;
}

} // namespace AgenUIEngine
