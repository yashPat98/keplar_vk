// ────────────────────────────────────────────
//  File: renderer.hpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#pragma once

#include "vulkan/vulkan_context.hpp"

namespace keplar
{
    class Renderer final
    {
        public:
            // creation and destruction
            explicit Renderer(const VulkanContext& context) noexcept;
            ~Renderer();

            // disable copy and move semantics to enforce unique ownership
            Renderer(const Renderer&) = delete;
            Renderer& operator=(const Renderer&) = delete;
            Renderer(Renderer&&) = delete;
            Renderer& operator=(Renderer&&) = delete; 

            bool initialize() noexcept;

        private:
            // vulkan wrapper objects owned by context
            const VulkanDevice& m_vulkanDevice;
            const VulkanSwapchain& m_vulkanSwapchain;
    };
}   // namespace keplar
