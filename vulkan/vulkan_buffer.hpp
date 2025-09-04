// ────────────────────────────────────────────
//  File: vulkan_buffer.hpp · Created by Yash Patel · 8-2-2025
// ────────────────────────────────────────────

#include "vulkan/vk_config.hpp"
#include "vulkan/vulkan_device.hpp"
#include "vulkan/vulkan_command_pool.hpp"
#include "vulkan/vulkan_fence.hpp"
#include "utils/thread_pool.hpp"

namespace keplar
{
    class VulkanBuffer final
    {
        public:
            // creation and destruction
            VulkanBuffer() noexcept;
            ~VulkanBuffer();  

            // disable copy semantics to enforce unique ownership
            VulkanBuffer(const VulkanBuffer&) = delete;
            VulkanBuffer& operator=(const VulkanBuffer&) = delete;

            // move semantics
            VulkanBuffer(VulkanBuffer&&) noexcept;
            VulkanBuffer& operator=(VulkanBuffer&&) noexcept;
 
            // usage
            bool createHostVisible(const VulkanDevice& device, const VkBufferCreateInfo& createInfo, const void* data, size_t size, bool persistMapped = false) noexcept;
            bool createDeviceLocal(const VulkanDevice& device, const VulkanCommandPool& commandPool, const VkBufferCreateInfo& createInfo, const void* data, size_t size) noexcept;
            bool uploadHostVisible(const void* data, size_t size, VkDeviceSize offset = 0, bool mapFullAllocation = false) noexcept;

            // accessors
            VkBuffer get() const noexcept { return m_vkBuffer; }  

        private:
            bool createBuffer(const VulkanDevice& device,
                              const VkBufferCreateInfo& createInfo, 
                              VkMemoryPropertyFlags propertyFlags, 
                              VkBuffer& vkBuffer, 
                              VkDeviceMemory& vkDeviceMemory) noexcept;

        private:  
            // vulkan handles
            VkDevice         m_vkDevice;
            VkBuffer         m_vkBuffer;
            VkDeviceMemory   m_vkDeviceMemory;
            VkDeviceSize     m_allocationSize;
            void*            m_mappedData;
    }; 
}   // namespace keplar

