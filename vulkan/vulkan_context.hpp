// ────────────────────────────────────────────
//  File: vulkan_context.hpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>

#include "platform/platform.hpp"
#include "vulkan_instance.hpp"
#include "vulkan_surface.hpp"

namespace keplar
{
    struct VulkanContextConfig
    {
        std::vector<const char*> mExtensions;
        std::vector<const char*> mValidationLayers;
    };

    class VulkanContext
    {
        public: 
            VulkanContext();
            virtual ~VulkanContext();

            // non-copyable & non-movable
            VulkanContext(const VulkanContext&) = delete;
            VulkanContext& operator=(const VulkanContext&) = delete;
            VulkanContext(VulkanContext&&) noexcept = delete;
            VulkanContext& operator=(VulkanContext&&) noexcept = delete;

            // manage lifecycle
            bool initialize(const Platform& platform, const VulkanContextConfig& config);
            void destroy();

        private:
            std::unique_ptr<VulkanInstance> m_vulkanInstance;
            std::unique_ptr<VulkanSurface> m_vulkanSurface;
    };
}   // namespace keplar
