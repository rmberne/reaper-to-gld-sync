cmake_minimum_required(VERSION 3.20)

# 1. APPLE DEPLOYMENT TARGET (MUST BE BEFORE project())
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS version" FORCE)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)

# Enable C++, Objective-C, and Objective-C++
project(reaper_gld_sync VERSION 1.0.0 LANGUAGES CXX OBJC OBJCXX)

# C++17 is the safest modern standard for macOS 10.15
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force ASIO to run in standalone mode (no Boost required)
add_definitions(-DASIO_STANDALONE)
add_definitions(-DLINK_PLATFORM_MACOSX=1)

# Objective-C/C++ compatibility flags for 10.15
if (APPLE)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc -DOBJC_OLD_DISPATCH_PROTOTYPES=0")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc -DOBJC_OLD_DISPATCH_PROTOTYPES=0")
endif ()

# ==============================================================================
# DEPENDENCY MANAGEMENT (Fetch at Compile Time)
# ==============================================================================
include(FetchContent)

# 1. Fetch spdlog
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "Disable spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Disable spdlog tests" FORCE)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.0
)

# 2. Fetch ASIO (Standalone)
FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-30-2
)

# 3. Fetch RtMidi
set(RTMIDI_BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libs" FORCE)
set(RTMIDI_API_CORE ON CACHE BOOL "Enable CoreMIDI" FORCE)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "Disable tests" FORCE)
FetchContent_Declare(
        rtmidi
        GIT_REPOSITORY https://github.com/thestk/rtmidi.git
        GIT_TAG 5.0.0
)

# 4. Fetch Ableton Link
set(LINK_BUILD_TESTS OFF CACHE BOOL "Disable Link tests" FORCE)
set(LINK_BUILD_EXAMPLES OFF CACHE BOOL "Disable Link examples" FORCE)
set(LINK_BUILD_QT_EXAMPLES OFF CACHE BOOL "Disable Link Qt examples" FORCE)
set(LINK_BUILD_ASIO_EXAMPLES OFF CACHE BOOL "Disable Link Asio examples" FORCE)
FetchContent_Declare(
        ableton_link
        GIT_REPOSITORY https://github.com/Ableton/link.git
        GIT_TAG Link-3.1.2
        # We no longer strictly need submodules since we fetch ASIO manually, 
        # but Link's build system might still look for it, so we leave it ON.
        GIT_SUBMODULES_RECURSE ON
)

# Make all dependencies available to the build system
FetchContent_MakeAvailable(spdlog asio rtmidi)

# Ableton Link is header-only for our needs, we manually fetch it
FetchContent_GetProperties(ableton_link)
if(NOT ableton_link_POPULATED)
    FetchContent_Populate(ableton_link)
endif()
# ==============================================================================

# UI resources
set(XIB_FILE "src/ReaperSyncPrefsUI/ReaperSyncPrefsUI/ConfigWindow.xib")
set(NIB_FILE "${CMAKE_CURRENT_BINARY_DIR}/ConfigWindow.nib")
add_custom_command(
        OUTPUT ${NIB_FILE}
        COMMAND ibtool --compile ${NIB_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${XIB_FILE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${XIB_FILE}
        COMMENT "Compiling ConfigWindow.xib into ConfigWindow.nib..."
)
set_source_files_properties(${NIB_FILE} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
)

set(ICON_FILE "AppIcon.icns")

set_source_files_properties(${ICON_FILE} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
)

# Define our executable
add_executable(${PROJECT_NAME}
        MACOSX_BUNDLE
        src/main.mm
        src/engine.cpp
        src/engine.hpp
        src/ConfigWindowController.h
        src/ConfigWindowController.mm
        ${NIB_FILE}
        ${ICON_FILE}
        src/mixer.cpp
        src/mixer.hpp
        src/midi_manager.cpp
        src/midi_manager.hpp
        src/pulse_manager.cpp
        src/pulse_manager.hpp
        src/arduino_manager.cpp
        src/arduino_manager.hpp
        src/logger.hpp
        src/logger.cpp
)

# Set bundle properties to run as a Menu Bar app (LSUIElement)
set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.rmberne.reapersync"
        MACOSX_BUNDLE_BUNDLE_NAME "ReaperSync"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 YourName. All rights reserved."
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
        RESOURCE "${NIB_FILE};${ICON_FILE}"
)

# Dynamic Include Paths based on FetchContent downloads
target_include_directories(${PROJECT_NAME} PRIVATE
        ${rtmidi_SOURCE_DIR}
        ${ableton_link_SOURCE_DIR}/include
        ${asio_SOURCE_DIR}/asio/include  # Points directly to the dedicated ASIO fetch
)

# Linking
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads
        rtmidi
        spdlog::spdlog_header_only
        "-framework AppKit"
        "-framework Foundation"
        "-framework Cocoa"
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework CoreServices"
)

# Print architecture info at the end of the build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND lipo -info $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Verifying binary architectures..."
)
