// ────────────────────────────────────────────
//  File: vulkan_instance.hpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#pragma once 

#include <vector>
#include "vulkan/vk_config.hpp"

namespace keplar
{
    class VulkanInstance final
    {
        public:
            VulkanInstance();
            ~VulkanInstance();

            // disable copy and move semantics
            VulkanInstance(const VulkanInstance&) = delete;
            VulkanInstance& operator=(const VulkanInstance&) = delete;
            VulkanInstance(VulkanInstance&&) = delete;
            VulkanInstance& operator=(VulkanInstance&&) = delete;

            // not thread-safe
            bool initialize(const VulkanContextConfig& config);
            void destroy();

            // thread-safe
            VkInstance get() const;
            bool isValid() const;
            bool isExtensionEnabled(const char* extensionName) const;
            bool isValidationLayerEnabled(const char* layerName) const;

        private:
            bool validateAndSetExtensions(const std::vector<const char*>& requestedExtensions);
            bool validateAndSetValidationLayers(const std::vector<const char*>& requestedLayers);

            VkInstance m_vkInstance;
            std::vector<const char*> m_enabledExtensions;
            std::vector<const char*> m_enabledValidationLayers;
    };
}   // namespace keplar

