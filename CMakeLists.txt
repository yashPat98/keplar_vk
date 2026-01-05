# ───────────────────────────────────────────────
# Project Setup
# ───────────────────────────────────────────────
cmake_minimum_required(VERSION 3.15)
project(keplar VERSION 1.0 LANGUAGES CXX)

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
    GLTFLOADER
    PBR
)

if(NOT DEFINED KEPLAR_SAMPLE)
    message(FATAL_ERROR
        "❌ No sample selected.\n"
        "Please configure with one of the following flags:\n"
        "  -DKEPLAR_SAMPLE=TRIANGLE   \n"
        "  -DKEPLAR_SAMPLE=TEXTURE    \n"
        "  -DKEPLAR_SAMPLE=GLTFLOADER \n"
        "  -DKEPLAR_SAMPLE=PBR        \n"
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
    list(APPEND KEPLAR_SRC 
         ${CMAKE_SOURCE_DIR}/platform/windows/win32_platform.cpp
         ${CMAKE_SOURCE_DIR}/platform/windows/keplar.rc)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
    list(APPEND KEPLAR_SRC ${CMAKE_SOURCE_DIR}/platform/linux/x11_platform.cpp)
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
    ${CMAKE_SOURCE_DIR}/external/imgui 
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

# ───────────────────────────────────────────────
# ImGui Integration
# ───────────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
file(GLOB IMGUI_SRC 
    ${IMGUI_DIR}/*.cpp 
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# platform-specific backend
if(WIN32)
    list(APPEND IMGUI_SRC ${IMGUI_DIR}/backends/imgui_impl_win32.cpp)
elseif(UNIX)
    list(APPEND IMGUI_SRC ${IMGUI_DIR}/backends/imgui_impl_x11.cpp)
endif()

add_library(imgui STATIC ${IMGUI_SRC})

# ───────────────────────────────────────────────
# Executable Target
# ───────────────────────────────────────────────
if(WIN32)
    # windows
    add_executable(keplar WIN32 ${KEPLAR_SRC})
    target_compile_options(keplar PRIVATE /W4 /WX)
else()
    # linux
    add_executable(keplar ${KEPLAR_SRC})
    target_compile_options(keplar PRIVATE -Wall -Wextra -Werror)
endif()

target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
target_link_libraries(keplar PRIVATE imgui)

# ───────────────────────────────────────────────
# copy resources to target directory
# ───────────────────────────────────────────────
set(KEPLAR_RESOURCES_SRC "${CMAKE_SOURCE_DIR}/resources")
set(KEPLAR_RESOURCES_DST "$<TARGET_FILE_DIR:keplar>/resources")
set(KEPLAR_RESOURCES_STAMP "${CMAKE_BINARY_DIR}/keplar_resources.stamp")
file(GLOB_RECURSE KEPLAR_RESOURCE_DEPS CONFIGURE_DEPENDS "${KEPLAR_RESOURCES_SRC}/*")

add_custom_command(
    OUTPUT "${KEPLAR_RESOURCES_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${KEPLAR_RESOURCES_SRC}"
            "${KEPLAR_RESOURCES_DST}"
    COMMAND ${CMAKE_COMMAND} -E touch
            "${KEPLAR_RESOURCES_STAMP}"
    DEPENDS ${KEPLAR_RESOURCE_DEPS}
    COMMENT "Copying resources"
)

add_custom_target(copy_keplar_resources ALL DEPENDS "${KEPLAR_RESOURCES_STAMP}")

# ───────────────────────────────────────────────
# Visual Studio Groups
# ───────────────────────────────────────────────
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT keplar)

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
    set_target_properties(keplar PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
