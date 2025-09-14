// ────────────────────────────────────────────
//  File: vulkan_descriptor_pool.hpp · Created by Yash Patel · 8-25-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan_config.hpp"

namespace keplar
{
    class VulkanDescriptorPool
    {
        public:   
            // creation and destruction
            VulkanDescriptorPool() noexcept;
            ~VulkanDescriptorPool();

            // disable copy semantics to enforce unique ownership
            VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
            VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;

            // move semantics
            VulkanDescriptorPool(VulkanDescriptorPool&&) noexcept;
            VulkanDescriptorPool& operator=(VulkanDescriptorPool&&) noexcept;

            bool initialize(VkDevice vkDevice, const VkDescriptorPoolCreateInfo& createInfo) noexcept;
            
            // accessor
            VkDescriptorPool get() const noexcept { return m_vkDescriptorPool; }

        private:
            // vulkan handles
            VkDevice m_vkDevice;
            VkDescriptorPool m_vkDescriptorPool;
    };  
}   // namespace keplar
