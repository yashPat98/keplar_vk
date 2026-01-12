// ────────────────────────────────────────────
//  File: vulkan_command_buffer.cpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#include "vulkan_command_buffer.hpp"
#include "vulkan_utils.hpp"
#include "utils/logger.hpp"

namespace keplar
{
    VulkanCommandBuffer::VulkanCommandBuffer() noexcept
        : m_vkCommandBuffer(VK_NULL_HANDLE)
    {
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer vkCommandBuffer) noexcept
        : m_vkCommandBuffer(vkCommandBuffer)
    {
    }

    VulkanCommandBuffer::VulkanCommandBuffer(const VulkanCommandBuffer& other) noexcept
        : m_vkCommandBuffer(other.m_vkCommandBuffer)
    {

    }

    VulkanCommandBuffer& VulkanCommandBuffer::operator=(const VulkanCommandBuffer& other) noexcept
    {
        if (this != &other)
        {
            m_vkCommandBuffer = other.m_vkCommandBuffer;
        }
        return *this;
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept
        : m_vkCommandBuffer(other.m_vkCommandBuffer)
    {
        other.m_vkCommandBuffer = VK_NULL_HANDLE;
    }

    VulkanCommandBuffer& VulkanCommandBuffer::operator=(VulkanCommandBuffer&& other) noexcept
    {
        if (this != &other)
        {
            m_vkCommandBuffer = other.m_vkCommandBuffer;
            other.m_vkCommandBuffer = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool VulkanCommandBuffer::begin(const VkCommandBufferBeginInfo& beginInfo) const noexcept
    {
        return VK_CHECK(vkBeginCommandBuffer(m_vkCommandBuffer, &beginInfo));
    }

    bool VulkanCommandBuffer::begin(VkCommandBufferUsageFlags flags) const noexcept
    {
        VkCommandBufferBeginInfo vkCommandBufferBeginInfo{};
        vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkCommandBufferBeginInfo.pNext = nullptr;
        vkCommandBufferBeginInfo.flags = flags;

        return VK_CHECK(vkBeginCommandBuffer(m_vkCommandBuffer, &vkCommandBufferBeginInfo));
    }

    bool VulkanCommandBuffer::end() const noexcept
    {
        return VK_CHECK(vkEndCommandBuffer(m_vkCommandBuffer));
    }

    bool VulkanCommandBuffer::reset(VkCommandBufferResetFlags flags) const noexcept
    {
        return VK_CHECK(vkResetCommandBuffer(m_vkCommandBuffer, flags));
    }

    void VulkanCommandBuffer::executeCommands(const VulkanCommandBuffer& commandBuffers) noexcept
    {
        // execute command buffers
        VkCommandBuffer vkCommandBuffer = commandBuffers.get();
        vkCmdExecuteCommands(m_vkCommandBuffer, 1, &vkCommandBuffer);
    }

    void VulkanCommandBuffer::beginRenderPass(const VkRenderPassBeginInfo& beginInfo, VkSubpassContents contents) const noexcept
    {
        vkCmdBeginRenderPass(m_vkCommandBuffer, &beginInfo, contents);
    }

    void VulkanCommandBuffer::endRenderPass() const noexcept
    {
        vkCmdEndRenderPass(m_vkCommandBuffer);
    }

    void VulkanCommandBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* copyRegions) const noexcept
    {
        vkCmdCopyBuffer(m_vkCommandBuffer, srcBuffer, dstBuffer, regionCount, copyRegions);
    }

    void VulkanCommandBuffer::bindPipeline(VkPipelineBindPoint bindPoint, VkPipeline pipeline) const noexcept
    {
        vkCmdBindPipeline(m_vkCommandBuffer, bindPoint, pipeline);
    }
    
    void VulkanCommandBuffer::bindDescriptorSets(VkPipelineBindPoint bindPoint, 
                                                 VkPipelineLayout layout, 
                                                 uint32_t firstSet, 
                                                 uint32_t descriptorSetCount, 
                                                 const VkDescriptorSet* pDescriptorSets, 
                                                 uint32_t dynamicOffsetCount, 
                                                 const uint32_t* pDynamicOffsets) const noexcept
    {
        vkCmdBindDescriptorSets(m_vkCommandBuffer, bindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }

    void VulkanCommandBuffer::bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const noexcept
    {
        vkCmdBindVertexBuffers(m_vkCommandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    }

    void VulkanCommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const noexcept
    {
        vkCmdDraw(m_vkCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance); 
    }

    void VulkanCommandBuffer::transitionImageLayout(VkImage image, 
                                                    VkImageLayout oldLayout, 
                                                    VkImageLayout newLayout, 
                                                    const VkImageSubresourceRange& subresourceRange) const noexcept
    {
        // image memory barrier info
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.pNext                = nullptr;
        imageMemoryBarrier.oldLayout            = oldLayout;
        imageMemoryBarrier.newLayout            = newLayout;
        imageMemoryBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image                = image;
        imageMemoryBarrier.subresourceRange     = subresourceRange;

        // source and destination pipeline stages
        VkPipelineStageFlags srcStageMask{};
        VkPipelineStageFlags dstStageMask{};

        switch (oldLayout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                imageMemoryBarrier.srcAccessMask = 0; 
                srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                break;
            
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                imageMemoryBarrier.srcAccessMask = 0;
                srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;

            default:
                imageMemoryBarrier.srcAccessMask = 0;
                srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                break;
        }

        switch (newLayout)
        {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                imageMemoryBarrier.dstAccessMask = 0;
                dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;

            default:
                imageMemoryBarrier.dstAccessMask = 0;
                dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;
        }

        // issue an image memory barrier to synchronize access and transition the image layout between 
        // pipeline stages, ensuring proper ordering of read/write operations
        vkCmdPipelineBarrier(m_vkCommandBuffer,                         // command buffer
                             srcStageMask, dstStageMask,                // pipeline stages
                             0,                                         // dependency flag
                             0, nullptr,                                // global memory barriers
                             0, nullptr,                                // buffer memory barriers
                             1, &imageMemoryBarrier);                   // image memory barriers
    }
}   // namespace keplar
