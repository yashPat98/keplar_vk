// ────────────────────────────────────────────
//  File: main.cpp · Created by Yash Patel · 6-22-2025
// ────────────────────────────────────────────

#include "utils/logger.hpp"
#include "core/keplar_app.hpp"

int main([[maybe_unused]]int argc, [[maybe_unused]] char* argv[])
{
    // start the logging thread early
    auto& logger = keplar::Logger::getInstance();

    keplar::KeplarApp app;
    if (!app.initialize())
    {
        VK_LOG_FATAL("failed to initialize Keplar application");
        logger.terminate();
        return EXIT_FAILURE;
    }

    // start game loop
    const bool status = app.run();

    // shutdown app and flush logs
    app.shutdown();
    logger.terminate();
    return status ? EXIT_SUCCESS : EXIT_FAILURE;;
}

