// ────────────────────────────────────────────
//  File: vulkan_samplers.hpp · Created by Yash Patel · 10-04-2025
// ────────────────────────────────────────────

#pragma once

#include <array>

#include "vulkan_config.hpp"
#include "vulkan_device.hpp"

namespace keplar
{   
    class VulkanSamplers final
    {
        public:
            // common sampler presets
            enum class Type : uint8_t 
            {
                NearestClamp = 0,
                LinearClamp,
                NearestRepeat,
                LinearRepeat,
                AnisotropicRepeat,
                Count
            };

        public:
            // creation and destruction
            VulkanSamplers() noexcept;
            ~VulkanSamplers();

            // disable copy semantics to enforce unique ownership
            VulkanSamplers(const VulkanSamplers&) = delete;
            VulkanSamplers& operator=(const VulkanSamplers&) = delete;

            // move semantics
            VulkanSamplers(VulkanSamplers&&) noexcept;
            VulkanSamplers& operator=(VulkanSamplers&&) noexcept;

            bool initialize(const VulkanDevice& device) noexcept;
            void destroy() noexcept;

            VkSampler get(Type type) const noexcept { return m_samplers[static_cast<size_t>(type)]; }
 
        private:
            VkDevice m_vkDevice;
            std::array<VkSampler, static_cast<size_t>(Type::Count)> m_samplers;
    };
}   // namespace keplar
