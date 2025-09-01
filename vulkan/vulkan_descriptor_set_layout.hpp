// ────────────────────────────────────────────
//  File: vulkan_descriptor_set_layout.hpp · Created by Yash Patel · 8-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"

namespace keplar
{
    class VulkanDescriptorSetLayout
    {
        public:   
            // creation and destruction
            VulkanDescriptorSetLayout() noexcept;
            ~VulkanDescriptorSetLayout();

            // disable copy semantics to enforce unique ownership
            VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
            VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

            // move semantics
            VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&&) noexcept;
            VulkanDescriptorSetLayout& operator=(VulkanDescriptorSetLayout&&) noexcept;

            bool initialize(VkDevice vkDevice, const VkDescriptorSetLayoutCreateInfo& createInfo) noexcept;
            
            // accessor
            VkDescriptorSetLayout get() const noexcept { return m_vkDescriptorSetLayout; }

        private:
            // vulkan handles
            VkDevice m_vkDevice;
            VkDescriptorSetLayout m_vkDescriptorSetLayout;
    };  
}   // namespace keplar
