// ────────────────────────────────────────────
//  File: keplar_app.hpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>
#include "platform/platform.hpp"
#include "vulkan/vulkan_context.hpp"

namespace keplar
{
    class KeplarApp final
    {
        public:
            // creation and destruction
            KeplarApp() noexcept;
            ~KeplarApp() = default;

            // disable copy and move semantics to enforce unique ownership
            KeplarApp(const KeplarApp&) = delete;
            KeplarApp& operator=(const KeplarApp&) = delete;
            KeplarApp(KeplarApp&&) = delete;
            KeplarApp& operator=(KeplarApp&&) = delete;

            // app lifecycle functions
            bool initialize() noexcept;
            bool run() noexcept;
            void shutdown() noexcept;

        private:
            std::unique_ptr<Platform> m_platform;
            std::unique_ptr<VulkanContext> m_vulkanContext;
    };
}   // namespace keplar
