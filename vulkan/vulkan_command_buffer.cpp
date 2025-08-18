// ────────────────────────────────────────────
//  File: vulkan_command_buffer.cpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#include "vulkan_command_buffer.hpp"
#include "utils/logger.hpp"
#include "vk_utils.hpp"

namespace keplar
{
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

    bool VulkanCommandBuffer::begin(VkCommandBufferUsageFlags flags) noexcept
    {
        VkCommandBufferBeginInfo vkCommandBufferBeginInfo{};
        vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkCommandBufferBeginInfo.pNext = nullptr;
        vkCommandBufferBeginInfo.flags = flags;

        return VK_CHECK(vkBeginCommandBuffer(m_vkCommandBuffer, &vkCommandBufferBeginInfo));
    }

    bool VulkanCommandBuffer::end() noexcept
    {
        return VK_CHECK(vkEndCommandBuffer(m_vkCommandBuffer));
    }

    bool VulkanCommandBuffer::reset(VkCommandBufferResetFlags flags) noexcept
    {
        return VK_CHECK(vkResetCommandBuffer(m_vkCommandBuffer, flags));
    }

    void VulkanCommandBuffer::beginRenderPass(const VkRenderPassBeginInfo& beginInfo, VkSubpassContents contents) noexcept
    {
        vkCmdBeginRenderPass(m_vkCommandBuffer, &beginInfo, contents);
    }

    void VulkanCommandBuffer::endRenderPass() noexcept
    {
        vkCmdEndRenderPass(m_vkCommandBuffer);
    }

    void VulkanCommandBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* copyRegions) noexcept
    {
        vkCmdCopyBuffer(m_vkCommandBuffer, srcBuffer, dstBuffer, regionCount, copyRegions);
    }

}   // namespace keplar
