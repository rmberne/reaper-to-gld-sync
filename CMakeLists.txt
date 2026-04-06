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

# 3. Define our executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/mixer.cpp
    src/mixer.hpp
    src/midi_manager.cpp
    src/midi_manager.hpp
    src/config_loader.cpp
    src/config_loader.hpp
    src/pulse_manager.cpp
    src/pulse_manager.hpp
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
add_executable(test_config tests/test_config.cpp src/config_loader.cpp)
target_include_directories(test_config PRIVATE src)
