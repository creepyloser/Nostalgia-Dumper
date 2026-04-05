cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(Nostalgia
    VERSION 1.4.0
    DESCRIPTION "Nostalgia - Multi-engine game module and export harvester"
    LANGUAGES CXX
)

# Set C++ standard to C++23 with fallback to C++20
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-specific settings
if(NOT WIN32)
    message(FATAL_ERROR "Nostalgia requires Windows platform")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# FetchContent for dependencies
include(FetchContent)

# Fetch ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.6
    GIT_SHALLOW TRUE
)

# GLFW for window management
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

# Fetch all dependencies
FetchContent_MakeAvailable(imgui glfw)

# Add ImGui files
set(IMGUI_SRC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# Source files
set(NOSTALGIA_DUMPERS_SRC
    src/dumpers/dumper_shared.cpp
    src/dumpers/unreal_dumper.cpp
    src/dumpers/unity_il2cpp_dumper.cpp
    src/dumpers/unity_mono_dumper.cpp
    src/dumpers/source_engine_dumper.cpp
    src/dumpers/godot_dumper.cpp
    src/dumpers/cryengine_dumper.cpp
    src/dumpers/universal_dumper.cpp
    src/dumpers/gamemaker_dumper.cpp
    src/dumpers/unigine_dumper.cpp
)

set(NOSTALGIA_SRC
    src/main.cpp
    src/memory.cpp
    src/patterns.cpp
    src/dumper.cpp
    src/pe_remote.cpp
    src/engine_scans.cpp
    src/gui.cpp
    ${NOSTALGIA_DUMPERS_SRC}
)

set(NOSTALGIA_HEADERS
    include/memory.h
    include/patterns.h
    include/dumper.h
    include/pe_remote.h
    include/engine_scans.h
    include/gui.h
    include/version.h
    include/types.h
    include/dumpers/dumper_shared.h
    include/dumpers/engine_dumpers.h
)

source_group("dumpers" FILES ${NOSTALGIA_DUMPERS_SRC})

add_executable(Nostalgia
    ${NOSTALGIA_SRC}
    ${NOSTALGIA_HEADERS}
    ${IMGUI_SRC}
)

target_include_directories(Nostalgia PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(Nostalgia PRIVATE
    kernel32
    user32
    psapi
    d3d11
    d3dcompiler
    dxgi
    glfw
)

# Compiler flags
if(MSVC)
    target_compile_options(Nostalgia PRIVATE
        /W4
        /WX
        $<$<CONFIG:Release>:/O2 /Gy /fp:fast>
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
    )
    target_link_options(Nostalgia PRIVATE
        $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO>
    )
else()
    target_compile_options(Nostalgia PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:Debug>:-g -O0>
    )
endif()

# Define for ImGui backends
target_compile_definitions(Nostalgia PRIVATE
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WINDOWS
    _CRT_SECURE_NO_WARNINGS
)

# Set startup project (for Visual Studio)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Nostalgia)

# Print configuration info
message(STATUS "====================================")
message(STATUS "Nostalgia v${PROJECT_VERSION}")
message(STATUS "====================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Output: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "====================================")
