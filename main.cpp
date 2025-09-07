// ────────────────────────────────────────────
//  File: main.cpp · Created by Yash Patel · 6-22-2025
// ────────────────────────────────────────────

#include "utils/logger.hpp"
#include "core/keplar_app.hpp"

int main([[maybe_unused]]int argc, [[maybe_unused]] char* argv[])
{
    // start the logging thread early
    auto& logger = keplar::Logger::getInstance();

    // app init 
    keplar::KeplarApp app;
    if (!app.initialize())
    {
        VK_LOG_FATAL("failed to initialize Keplar application");
        logger.terminate();
        return EXIT_FAILURE;
    }

    // run the main loop; returns exit code    
    return app.run();
}

