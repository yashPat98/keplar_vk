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
            KeplarApp();
            ~KeplarApp() = default;

            // disable copy and move
            KeplarApp(const KeplarApp&) = delete;
            KeplarApp& operator=(const KeplarApp&) = delete;
            KeplarApp(KeplarApp&&) = delete;
            KeplarApp& operator=(KeplarApp&&) = delete;

            // app lifecycle functions
            bool initialize();
            bool run();
            void shutdown();

        private:
            std::unique_ptr<Platform> m_platform;
            std::unique_ptr<VulkanContext> m_vulkanContext;
    };
}   // namespace keplar
