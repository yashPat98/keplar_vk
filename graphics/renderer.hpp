// ────────────────────────────────────────────
//  File: renderer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "platform/event_listener.hpp"
#include "platform/platform.hpp"
#include "vulkan/vulkan_context.hpp"

namespace keplar
{
    class Renderer : public EventListener
    {
        public:
            virtual ~Renderer() = default;

            // core renderer interface: initialize resources, update state, and submit frames
            virtual bool initialize(std::weak_ptr<Platform> platform, std::weak_ptr<VulkanContext> context) noexcept = 0;
            virtual void update(float dt) noexcept = 0;
            virtual bool render() noexcept = 0;

            // configure vulkan instance, layers, extensions, features, and queue preferences
            virtual void configureVulkan(VulkanContextConfig& /* config */) noexcept {}
    };
}   // namespace keplar
