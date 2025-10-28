// ────────────────────────────────────────────
//  File: vulkan_command_buffer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vulkan_config.hpp"

namespace keplar
{
    // forward declarations
    class VulkanCommandPool;

    class VulkanCommandBuffer final
    {
        public:
            // creation and destruction
            VulkanCommandBuffer() = delete;
            ~VulkanCommandBuffer() = default;

            // copy semantics
            VulkanCommandBuffer(const VulkanCommandBuffer&) noexcept;
            VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) noexcept;

            // move semantics
            VulkanCommandBuffer(VulkanCommandBuffer&&) noexcept;
            VulkanCommandBuffer& operator=(VulkanCommandBuffer&&) noexcept; 

            // command recording
            bool begin(VkCommandBufferUsageFlags flags = 0) const noexcept;
            bool end() const noexcept;
            bool reset(VkCommandBufferResetFlags flags = 0) const noexcept;

            // render pass helpers
            void beginRenderPass(const VkRenderPassBeginInfo& beginInfo, VkSubpassContents contents) const noexcept;
            void endRenderPass() const noexcept;
  
            // copy helpers
            void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* copyRegions) const noexcept;

            // draw helpers
            void bindPipeline(VkPipelineBindPoint bindPoint, VkPipeline pipeline) const noexcept;
            void bindDescriptorSets(VkPipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, 
                const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount = 0, const uint32_t* pDynamicOffsets = nullptr) const noexcept;
            void bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const noexcept;
            void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const noexcept;

            // pipeline barrier
            void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                const VkImageSubresourceRange& subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }) const noexcept;

            // accessor
            VkCommandBuffer get() const noexcept { return m_vkCommandBuffer; }

        private:
            // only command pool can construct
            friend class VulkanCommandPool;
            explicit VulkanCommandBuffer(VkCommandBuffer vkCommandBuffer) noexcept;

        private:
            VkCommandBuffer m_vkCommandBuffer;
    };
}   // namespace keplar

