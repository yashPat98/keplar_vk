// ────────────────────────────────────────────
//  File: vulkan_instance.hpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#pragma once 

#include <memory>
#include <vector>

#include "vulkan/vk_config.hpp"

namespace keplar
{
    class VulkanInstance final
    {
        public:
            // creation and destruction
            static std::unique_ptr<VulkanInstance> create(const VulkanContextConfig& config) noexcept;
            ~VulkanInstance();

            // disable copy and move semantics to enforce unique ownership
            VulkanInstance(const VulkanInstance&) = delete;
            VulkanInstance& operator=(const VulkanInstance&) = delete;
            VulkanInstance(VulkanInstance&&) = delete;
            VulkanInstance& operator=(VulkanInstance&&) = delete;

            // accessors
            VkInstance get() const noexcept;
            bool isValid() const noexcept;
            bool isExtensionEnabled(const char* extensionName) const noexcept;
            bool isValidationLayerEnabled(const char* layerName) const noexcept;

        private:
            // construction helpers
            VulkanInstance() noexcept;
            bool initialize(const VulkanContextConfig& config) noexcept;
            bool validateAndSetExtensions(const std::vector<std::string_view>& requestedExtensions) noexcept;
            bool validateAndSetValidationLayers(const std::vector<std::string_view>& requestedLayers) noexcept;

        private:
            VkInstance m_vkInstance;
            std::vector<const char*> m_enabledExtensions;
            std::vector<const char*> m_enabledValidationLayers;
    };
}   // namespace keplar
