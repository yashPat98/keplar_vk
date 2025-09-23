// ────────────────────────────────────────────
//  File: platform_factory.hpp · Created by Yash Patel · 6-25-2025
// ────────────────────────────────────────────

#pragma once

#include <memory>
#include "platform.hpp"

namespace keplar::platform
{
    std::shared_ptr<Platform> createPlatform();
}   // namespace keplar
