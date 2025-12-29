// ────────────────────────────────────────────
//  File: main.cpp · Created by Yash Patel · 6-22-2025
// ────────────────────────────────────────────

#include "core/keplar_app.hpp"
#include "utils/logger.hpp"
#include "samples/PBR/pbr.hpp"

int main([[maybe_unused]]int argc, [[maybe_unused]] char* argv[])
{
    // start the logging thread early
    keplar::Logger::getInstance();

    // create and initialize app
    keplar::KeplarApp app;
    if (!app.initialize(std::make_unique<keplar::PBR>()))
    {
        VK_LOG_FATAL("failed to initialize Keplar application");
        return EXIT_FAILURE;
    }

    // run thr main loop until app exit
    return app.run();
}

