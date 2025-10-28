// ────────────────────────────────────────────
//  File: platform.hpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "vulkan/vulkan_config.hpp"
#include "event_listener.hpp"

namespace keplar
{
    class Platform
    {
        public:
            virtual ~Platform() = default;

            // lifecycle
            virtual bool initialize(const std::string& title, int width, int height, bool maximized) noexcept = 0;
            virtual void pollEvents() noexcept = 0;
            virtual bool shouldClose() noexcept = 0;
            virtual void shutdown() noexcept = 0;
            
            // window queries
            virtual void* getWindowHandle() const noexcept = 0;
            virtual uint32_t getWindowWidth() const noexcept = 0;
            virtual uint32_t getWindowHeight() const noexcept = 0;

            // event listeners
            virtual void addListener(const std::shared_ptr<EventListener>& listener) noexcept = 0;
            virtual void removeListener(const std::shared_ptr<EventListener>& listener) noexcept = 0;
            virtual void enableImGuiEvents(bool enabled) noexcept = 0;

            // vulkan 
            virtual VkSurfaceKHR createSurface(VkInstance vkInstance) const noexcept = 0;
            virtual std::vector<std::string_view> getSurfaceExtensions() const noexcept = 0;
    };
}   // namespace keplar
