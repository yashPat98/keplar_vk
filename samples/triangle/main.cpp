// ────────────────────────────────────────────
//  File: main.cpp · Created by Yash Patel · 6-22-2025
// ────────────────────────────────────────────

#include "core/keplar_app.hpp"
#include "utils/logger.hpp"
#include "samples/triangle/triangle.hpp"

int main([[maybe_unused]]int argc, [[maybe_unused]] char* argv[])
{
    // start the logging thread early
    auto& logger = keplar::Logger::getInstance();

    // app init 
    keplar::KeplarApp app;
    if (!app.initialize(std::make_unique<keplar::Triangle>()))
    {
        VK_LOG_FATAL("failed to initialize Keplar application");
        logger.terminate();
        return EXIT_FAILURE;
    }

    // run the main loop
    const int exitCode = app.run();

    // clean up logging thread 
    logger.terminate();
    return exitCode;
}

