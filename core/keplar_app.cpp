// ────────────────────────────────────────────
//  File: keplar_app.cpp · Created by Yash Patel · 6-28-2025
// ────────────────────────────────────────────

#include "keplar_app.hpp"
#include "utils/logger.hpp"
#include "platform/platform_factory.hpp"

namespace keplar
{
    constexpr const char* kWindowTitle = "keplar_vk";
    constexpr int kDefaultWidth = 1280;
    constexpr int kDefaultHeight = 720;
}

namespace keplar
{
    KeplarApp::KeplarApp()
    {
    }

    bool KeplarApp::initialize()
    {
        m_platform = createPlatform();
        if (!m_platform)
        {
            VK_LOG_FATAL("failed to create platform instance");
            return false;
        }

        if (!m_platform->initialize(kWindowTitle, kDefaultWidth, kDefaultHeight))
        {
            VK_LOG_FATAL("failed to initialize platform");
            return false;
        }

        return true;
    }

    bool KeplarApp::run()
    {
        while (!m_platform->shouldClose())
        {
            m_platform->pollEvents();
        }
        return true;
    }

    void KeplarApp::shutdown()
    {
        if (m_platform)
        {
            m_platform->shutdown();
        }
    }

}   // namespace keplar