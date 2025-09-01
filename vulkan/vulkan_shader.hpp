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

            // disable copy semantics to enforce unique ownership
            VulkanShader(const VulkanShader&) = delete;
            VulkanShader& operator=(const VulkanShader&) = delete;

            // move semantics
            VulkanShader(VulkanShader&&) noexcept;
            VulkanShader& operator=(VulkanShader&&) noexcept;

            // usage
            bool initialize(VkDevice vkDevice, VkShaderStageFlagBits stage, const std::string& spirvFile) noexcept;

            // accessor
            VkShaderModule get() const noexcept { return m_vkShaderModule; }
            const VkPipelineShaderStageCreateInfo& getShaderStageInfo() const noexcept { return m_vkPipelineShaderStageCreateInfo; }
            bool isValid() const noexcept { return m_vkShaderModule != VK_NULL_HANDLE; }

        private:
            std::vector<uint32_t> loadSPIRVFile(const std::string& filepath) const noexcept;
        
        private:
            // vulkan handles
            VkDevice m_vkDevice;
            VkShaderModule m_vkShaderModule;
            VkPipelineShaderStageCreateInfo m_vkPipelineShaderStageCreateInfo;
    };
}   // namespace keplar

