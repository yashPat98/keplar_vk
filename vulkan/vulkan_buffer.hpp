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
            VulkanBuffer(const VulkanDevice& vulkanDevice) noexcept;
            ~VulkanBuffer();  

            // disable copy and move semantics to enforce unique ownership
            VulkanBuffer(const VulkanBuffer&) = delete;
            VulkanBuffer& operator=(const VulkanBuffer&) = delete;
            VulkanBuffer(VulkanBuffer&&) = delete;
            VulkanBuffer& operator=(VulkanBuffer&&) = delete;
 
            // usage
            bool createHostVisible(const VkBufferCreateInfo& createInfo, const void* data, size_t size) noexcept;
            bool createDeviceLocal(const VulkanCommandPool& commandPool, 
                                   const VkBufferCreateInfo& createInfo, 
                                   const void* data, 
                                   size_t size, 
                                   std::optional<std::reference_wrapper<ThreadPool>> threadPool = std::nullopt) noexcept;
            void waitForUpload() noexcept;

            // accessors
            VkBuffer get() const noexcept { return m_vkBuffer; }  

        private:
            bool createBuffer(const VkBufferCreateInfo& createInfo, 
                              VkMemoryPropertyFlags propertyFlags, 
                              VkBuffer& vkBuffer, 
                              VkDeviceMemory& vkDeviceMemory,
                              VkDeviceSize& allocationSize);

        private:  
            // immutable vulkan components
            const VulkanDevice& m_vulkanDevice;

            // vulkan handles
            VkDevice m_vkDevice;
            VkBuffer m_vkBuffer;
            VkDeviceMemory m_vkDeviceMemory;
            VulkanFence m_copyFence;
    }; 
}   // namespace keplar

