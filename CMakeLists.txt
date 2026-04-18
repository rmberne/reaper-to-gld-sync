cmake_minimum_required(VERSION 3.20)
project(reaper_gld_sync VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Multi-Architecture (Universal Binary) Support
# This ensures the binary contains both ARM (M1/M2) and Intel (x86_64) slices.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)

add_definitions(-DLINK_PLATFORM_MACOSX=1)

# 2. Add RtMidi as a sub-project
# We disable shared libraries to force static linking of RtMidi
set(RTMIDI_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(RTMIDI_API_CORE ON CACHE BOOL "" FORCE)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/rtmidi EXCLUDE_FROM_ALL)

# Enable Objective-C++
enable_language(OBJCXX)

# Enable ARC for Objective-C++
set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")

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

# 3. Define our executable
add_executable(${PROJECT_NAME}
        MACOSX_BUNDLE
        src/main.mm
        src/engine.cpp
        src/engine.hpp
        src/ConfigWindowController.h
        src/ConfigWindowController.mm
        ${NIB_FILE}
        src/mixer.cpp
        src/mixer.hpp
        src/midi_manager.cpp
        src/midi_manager.hpp
        src/pulse_manager.cpp
        src/pulse_manager.hpp
        src/arduino_manager.cpp
        src/arduino_manager.hpp
)

# Set bundle properties to run as a Menu Bar app (LSUIElement)
set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.rmberne.reapersync"
        MACOSX_BUNDLE_BUNDLE_NAME "ReaperSync"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 YourName. All rights reserved."
        RESOURCE "${NIB_FILE}"
)

# 4. Include paths
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/rtmidi
        ${CMAKE_SOURCE_DIR}/libs/link/include
        ${CMAKE_SOURCE_DIR}/libs/link/modules/asio-standalone/asio/include
        /opt/homebrew/include
)

# 5. Linking
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads
        rtmidi
        "-framework AppKit"
        "-framework Foundation"
        "-framework Cocoa"
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework CoreServices"
)

# Optional: Print architecture info at the end of the build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND lipo -info $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Verifying binary architectures..."
)

# 6. Tests
# (Disabled as config_loader was removed)
# add_executable(test_config tests/test_config.cpp src/config_loader.cpp)
# target_include_directories(test_config PRIVATE src)
