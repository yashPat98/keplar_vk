// ────────────────────────────────────────────
//  File: vulkan_context.hpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>

#include "platform/platform.hpp"
#include "vulkan_instance.hpp"
#include "vulkan_surface.hpp"
#include "vulkan_device.hpp"
#include "vulkan_swapchain.hpp"

namespace keplar
{
    class VulkanContext
    {
        public: 
            // creation and destruction
            class Builder;
            virtual ~VulkanContext();

            // disable copy and move semantics to enforce unique ownership
            VulkanContext(const VulkanContext&) = delete;
            VulkanContext& operator=(const VulkanContext&) = delete;
            VulkanContext(VulkanContext&&) = delete;
            VulkanContext& operator=(VulkanContext&&) = delete;

            // accessors return const references to wrappers; guaranteed to be initialized.
            const VulkanInstance& getInstance() const noexcept { return *m_vulkanInstance; }
            const VulkanSurface& getSurface() const noexcept { return *m_vulkanSurface; }
            const VulkanDevice& getDevice() const noexcept { return *m_vulkanDevice; }
            const VulkanSwapchain& getSwapchain() const noexcept { return *m_vulkanSwapchain; }

        private:
            // only accessible by builder for controlled construction and initialization
            VulkanContext() noexcept;
            bool initialize(const Platform& platform, const VulkanContextConfig& config) noexcept;

        private:
            std::unique_ptr<VulkanInstance> m_vulkanInstance;
            std::unique_ptr<VulkanSurface> m_vulkanSurface;
            std::unique_ptr<VulkanDevice> m_vulkanDevice;
            std::unique_ptr<VulkanSwapchain> m_vulkanSwapchain;
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
