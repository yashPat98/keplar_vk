# ───────────────────────────────────────────────
# Project Setup
# ───────────────────────────────────────────────
cmake_minimum_required(VERSION 3.15)
project(keplar_vk VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ───────────────────────────────────────────────
# Vulkan SDK Check
# ───────────────────────────────────────────────
set(REQUIRED_VULKAN_VERSION "1.4.304")

if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK not set. Expected Vulkan SDK ${REQUIRED_VULKAN_VERSION}.")
endif()

string(FIND "$ENV{VULKAN_SDK}" ${REQUIRED_VULKAN_VERSION} SDK_VERSION_MATCH)
if(${SDK_VERSION_MATCH} EQUAL -1)
    message(FATAL_ERROR "Incorrect Vulkan SDK version. Expected ${REQUIRED_VULKAN_VERSION}, got $ENV{VULKAN_SDK}")
endif()

find_package(Vulkan REQUIRED)

# ───────────────────────────────────────────────
# Sample Selection
# ───────────────────────────────────────────────
set(KEPLAR_SAMPLES
    TRIANGLE
    TEXTURE
)

if(NOT DEFINED KEPLAR_SAMPLE)
    message(FATAL_ERROR
        "❌ No sample selected.\n"
        "Please configure with one of the following flags:\n"
        "  -DKEPLAR_SAMPLE=TRIANGLE   # Build the Triangle sample\n"
        "  -DKEPLAR_SAMPLE=TEXTURE    # Build the Texture sample\n"
    )
endif()

string(TOUPPER "${KEPLAR_SAMPLE}" KEPLAR_SAMPLE_UPPER)
string(TOLOWER "${KEPLAR_SAMPLE}" KEPLAR_SAMPLE_LOWER)

list(FIND KEPLAR_SAMPLES "${KEPLAR_SAMPLE_UPPER}" SAMPLE_INDEX)

if(SAMPLE_INDEX EQUAL -1)
    string(JOIN "\n    " ALLOWED_SAMPLES ${KEPLAR_SAMPLES})
    message(FATAL_ERROR
        "❌ Invalid sample: ${KEPLAR_SAMPLE}\n"
        "Allowed values are:\n    ${ALLOWED_SAMPLES}\n"
    )
endif()

# ───────────────────────────────────────────────
# Source Files
# ───────────────────────────────────────────────
file(GLOB_RECURSE CORE_SRC     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/core/*.cpp     ${CMAKE_SOURCE_DIR}/core/*.hpp)
file(GLOB_RECURSE PLATFORM_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/platform/*.cpp ${CMAKE_SOURCE_DIR}/platform/*.hpp)
file(GLOB_RECURSE UTILS_SRC    CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/utils/*.cpp    ${CMAKE_SOURCE_DIR}/utils/*.hpp)
file(GLOB_RECURSE VULKAN_SRC   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/vulkan/*.cpp   ${CMAKE_SOURCE_DIR}/vulkan/*.hpp)
file(GLOB_RECURSE GRAPHICS_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/graphics/*.cpp ${CMAKE_SOURCE_DIR}/graphics/*.hpp)
file(GLOB_RECURSE SAMPLE_SRC   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/samples/${KEPLAR_SAMPLE_LOWER}/*)
file(GLOB_RECURSE SHADERS_SRC  CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/shaders/${KEPLAR_SAMPLE_LOWER}/*)

set(KEPLAR_SRC
    ${CORE_SRC}
    ${PLATFORM_SRC}
    ${UTILS_SRC}
    ${VULKAN_SRC}
    ${GRAPHICS_SRC}
    ${SAMPLE_SRC}
    ${SHADERS_SRC}
)

# ───────────────────────────────────────────────
# Platform Specific
# ───────────────────────────────────────────────
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
    list(APPEND KEPLAR_SRC ${CMAKE_SOURCE_DIR}/platform/win32_platform.cpp)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
    list(APPEND KEPLAR_SRC ${CMAKE_SOURCE_DIR}/platform/x11_platform.cpp)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ───────────────────────────────────────────────
# Include Paths
# ───────────────────────────────────────────────
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/tinygltf
)

# ───────────────────────────────────────────────
# Executable Target
# ───────────────────────────────────────────────
add_executable(keplar_vk WIN32 ${KEPLAR_SRC})

if(MSVC)
    target_compile_options(keplar_vk PRIVATE /W4 /WX)
else()
    target_compile_options(keplar_vk PRIVATE -Wall -Wextra -Werror)
endif()

target_link_libraries(keplar_vk PRIVATE Vulkan::Vulkan)

# ───────────────────────────────────────────────
# Visual Studio Groups
# ───────────────────────────────────────────────
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT keplar_vk)

source_group(TREE ${CMAKE_SOURCE_DIR}/core     PREFIX "core"     FILES ${CORE_SRC})
source_group(TREE ${CMAKE_SOURCE_DIR}/platform PREFIX "platform" FILES ${PLATFORM_SRC})
source_group(TREE ${CMAKE_SOURCE_DIR}/utils    PREFIX "utils"    FILES ${UTILS_SRC})
source_group(TREE ${CMAKE_SOURCE_DIR}/vulkan   PREFIX "vulkan"   FILES ${VULKAN_SRC})
source_group(TREE ${CMAKE_SOURCE_DIR}/graphics PREFIX "graphics" FILES ${GRAPHICS_SRC})
source_group(TREE ${CMAKE_SOURCE_DIR}/samples/${KEPLAR_SAMPLE_LOWER} PREFIX "${KEPLAR_SAMPLE_LOWER}" FILES ${SAMPLE_SRC})
source_group(TREE ${CMAKE_SOURCE_DIR}/shaders/${KEPLAR_SAMPLE_LOWER} PREFIX "shaders" FILES ${SHADERS_SRC})

# ───────────────────────────────────────────────
# Windows Executable Flags
# ───────────────────────────────────────────────
if(WIN32)
    set_target_properties(keplar_vk PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
