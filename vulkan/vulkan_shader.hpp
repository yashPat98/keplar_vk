// ────────────────────────────────────────────
//  File: vulkan_shader.hpp · Created by Yash Patel · 8-18-2025
// ────────────────────────────────────────────

#pragma once

#include "vk_config.hpp"

namespace keplar
{
    class VulkanShader final
    {
        public:
            // creation and destruction
            VulkanShader() noexcept;
            ~VulkanShader();

            // disable copy and move semantics to enforce unique ownership
            VulkanShader(const VulkanShader&) = delete;
            VulkanShader& operator=(const VulkanShader&) = delete;
            VulkanShader(VulkanShader&&) = delete;
            VulkanShader& operator=(VulkanShader&&) = delete;

            // usage
            bool initialize(VkDevice vkDevice, const std::string& spirvFile) noexcept;

            // accessor
            VkShaderModule get() const noexcept { return m_vkShaderModule; }
            bool isValid() const noexcept { return m_vkShaderModule != VK_NULL_HANDLE; }

        private:
            std::vector<uint32_t> loadSPIRVFile(const std::string& filepath) const noexcept;
        
        private:
            // vulkan handles
            VkShaderModule m_vkShaderModule;
            VkDevice m_vkDevice;
    };
}   // namespace keplar

