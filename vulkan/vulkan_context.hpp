// ────────────────────────────────────────────
//  File: vulkan_context.hpp · Created by Yash Patel · 6-29-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>
#include <optional>

#include "vulkan_config.hpp"

namespace keplar
{
    // forward declarations
    class Platform;
    class VulkanInstance;
    class VulkanSurface;
    class VulkanDevice;

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
            std::weak_ptr<VulkanInstance> getInstance() const noexcept { return m_vulkanInstance; }
            std::weak_ptr<VulkanSurface> getSurface() const noexcept { return m_vulkanSurface; }
            std::weak_ptr<VulkanDevice> getDevice() const noexcept { return m_vulkanDevice; }

        private:
            // only accessible by builder for controlled construction and initialization
            VulkanContext() noexcept;
            bool initialize(const Platform& platform, const VulkanContextConfig& config) noexcept;

        private:
            std::shared_ptr<VulkanInstance> m_vulkanInstance;
            std::shared_ptr<VulkanSurface>  m_vulkanSurface;
            std::shared_ptr<VulkanDevice>   m_vulkanDevice;
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

            // set required dependencies for vulkan context construction
            Builder& withPlatform(const std::shared_ptr<Platform>& platform) noexcept;
            Builder& withConfig(const VulkanContextConfig& config) noexcept;

            // builds vulkan context based on provided configuration
            std::shared_ptr<VulkanContext> build();

        private:
            std::shared_ptr<Platform> m_platform;
            std::optional<VulkanContextConfig> m_config;
    };
}   // namespace keplar
