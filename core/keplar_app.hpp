// ────────────────────────────────────────────
//  File: keplar_app.hpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>
#include "utils/time.hpp"
#include "utils/frame_pacer.hpp"

namespace keplar
{
    // forward declarations
    class Platform;
    class VulkanContext;
    class Renderer;

    class KeplarApp final
    {
        public:
            // creation and destruction
            KeplarApp() noexcept;
            ~KeplarApp();

            // disable copy and move semantics to enforce unique ownership
            KeplarApp(const KeplarApp&) = delete;
            KeplarApp& operator=(const KeplarApp&) = delete;
            KeplarApp(KeplarApp&&) = delete;
            KeplarApp& operator=(KeplarApp&&) = delete;

            // app lifecycle functions
            bool initialize(std::unique_ptr<Renderer> renderer) noexcept;
            int run() noexcept;
            void shutdown() noexcept;

        private:
            // initialization helpers
            bool initializePlatform() noexcept;
            bool initializeContext() noexcept;
            bool initializeRenderer() noexcept;

        private:
            std::shared_ptr<Platform>       m_platform;
            std::shared_ptr<VulkanContext>  m_vulkanContext;
            std::shared_ptr<Renderer>       m_renderer;
            Time                            m_time;
            FramePacer                      m_framePacer;
    };
}   // namespace keplar
