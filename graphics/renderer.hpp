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

            // initialize vulkan resources
            virtual bool initialize(std::weak_ptr<Platform> platform, std::weak_ptr<VulkanContext> context) noexcept = 0;

            // update per-frame data
            virtual bool update(float dt) noexcept = 0;

            // submit current frame
            virtual bool renderFrame() noexcept = 0;
    };
}   // namespace keplar
