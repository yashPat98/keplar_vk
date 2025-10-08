// ────────────────────────────────────────────
//  File: platform_factory.cpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#include "platform_factory.hpp"

#ifdef _WIN32
#include "windows/win32_platform.hpp"
#elif defined(__linux__)
#include "linux/x11_platform.hpp"
#else
#error "Unsupported platform"
#endif

namespace keplar::platform
{
    // factory function that returns the platform-specific instance
    std::shared_ptr<Platform> createPlatform()
    {
        #ifdef _WIN32
            return std::make_shared<Win32Platform>();
        #elif defined(__linux__)
            return std::make_shared<X11Platform>();
        #endif
    }
}   // namespace keplar
