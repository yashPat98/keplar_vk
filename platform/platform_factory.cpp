// ────────────────────────────────────────────
//  File: platform_factory.cpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#include "platform_factory.hpp"

#ifdef _WIN32
#include "win32_platform.hpp"
#elif defined(__linux__)
#include "x11_platform.hpp"
#else
#error "Unsupported platform"
#endif

namespace keplar
{
    // factory function that returns the platform-specific instance
    std::unique_ptr<Platform> createPlatform()
    {
        #ifdef _WIN32
            return std::make_unique<Win32Platform>();
        #elif defined(__linux__)
            return std::make_unique<X11Platform>();
        #endif
    }
}   // namespace keplar
