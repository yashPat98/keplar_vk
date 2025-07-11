// ────────────────────────────────────────────
//  File: vulkan_context.hpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>

#include "platform/platform.hpp"
#include "vulkan_instance.hpp"
#include "vulkan_surface.hpp"
#include "vulkan_device.hpp"

namespace keplar
{
    class VulkanContext
    {
        public: 
            class Builder;
            virtual ~VulkanContext();

            // disable copy and move semantics to enforce unique ownership
            VulkanContext(const VulkanContext&) = delete;
            VulkanContext& operator=(const VulkanContext&) = delete;
            VulkanContext(VulkanContext&&) noexcept = delete;
            VulkanContext& operator=(VulkanContext&&) noexcept = delete;

            // explicit resource cleanup
            void destroy();

        private:
            // only accessible by builder for controlled construction and initialization
            VulkanContext();
            bool initialize(const Platform& platform, const VulkanContextConfig& config);

        private:
            std::unique_ptr<VulkanInstance> m_vulkanInstance;
            std::unique_ptr<VulkanSurface> m_vulkanSurface;
            std::unique_ptr<VulkanDevice> m_vulkanDevice;
    };

    class VulkanContext::Builder
    {
        public:
            Builder() = default;

            // disable copy and move to avoid dangling references
            Builder(const Builder&) = delete;
            Builder& operator=(const Builder&) = delete;
            Builder(Builder&&) = delete;
            Builder& operator=(Builder&&) = delete;

            // application info overrides
            Builder& withApplicationName(std::string_view appName);
            Builder& withApplicationVersion(uint32_t appVersion);
            Builder& withEngineName(std::string_view engineName);
            Builder& withEngineVersion(uint32_t engineVersion);
            Builder& withApiVersion(uint32_t apiVersion);

            // core vulkan configuration
            Builder& enableValidation(bool enable);
            Builder& withPlatform(const Platform& platform);
            Builder& withInstanceExtensions(const std::vector<std::string_view>& instanceExtensions);
            Builder& withInstanceExtensionsIfValidation(const std::vector<std::string_view>& instanceExtensions);
            Builder& withValidationLayers(const std::vector<std::string_view>& validationLayers);
            Builder& withDeviceExtensions(const std::vector<std::string_view>& deviceExtensions);
            Builder& withDeviceFeatures(VkPhysicalDeviceFeatures features);
            Builder& preferDedicatedComputeQueue();
            Builder& preferDedicatedTransferQueue();

            // builds vulkan context based on provided configuration
            std::unique_ptr<VulkanContext> build();

        private:
            const Platform* m_platform;
            VulkanContextConfig m_vulkanContextConfig;
    };
}   // namespace keplar
