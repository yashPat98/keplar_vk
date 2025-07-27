// ────────────────────────────────────────────
//  File: renderer.cpp · Created by Yash Patel · 7-24-2025
// ────────────────────────────────────────────

#include "renderer.hpp"

namespace keplar
{
    Renderer::Renderer(const VulkanContext& context) noexcept
        : m_vulkanDevice(context.getDevice())
        , m_vulkanSwapchain(context.getSwapchain())
    {
    }

    Renderer::~Renderer()
    {
    }

    bool Renderer::initialize() noexcept
    {
        
    }
}   // namespace keplar
