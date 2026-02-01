cmake_minimum_required(VERSION 3.20)
project(headcannon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HEADCANNON_BUILD_TESTS "Build unit tests" ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Library source files
set(HEADCANNON_SOURCES
    src/data/tracking_pose.cpp
    src/math/angle_utils.cpp
    src/math/deadzone_utils.cpp
    src/math/smoothing_utils.cpp
    src/protocol/opentrack_packet.cpp
    src/protocol/udp_receiver.cpp
    src/protocol/polling_udp_receiver.cpp
    src/processing/center_offset_manager.cpp
    src/processing/tracking_processor.cpp
    src/config/ini_reader.cpp
    src/memory/pattern_scanner.cpp
    src/input/hotkey_poller.cpp
)

# Create static library
add_library(headcannon STATIC ${HEADCANNON_SOURCES})

target_include_directories(headcannon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link Winsock for UDP on Windows
if(WIN32)
    target_link_libraries(headcannon PUBLIC ws2_32)
endif()

# Optional hook manager module (requires MinHook)
option(HEADCANNON_BUILD_HOOKS "Build hook manager (requires MinHook)" OFF)
if(HEADCANNON_BUILD_HOOKS)
    if(NOT TARGET minhook)
        message(FATAL_ERROR "HEADCANNON_BUILD_HOOKS requires MinHook target. Please add MinHook before including headcannon-core.")
    endif()

    add_library(headcannon_hooks STATIC src/hooks/hook_manager.cpp)
    target_include_directories(headcannon_hooks
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(headcannon_hooks PUBLIC minhook)

    install(TARGETS headcannon_hooks
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
endif()

# Tests
if(HEADCANNON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS headcannon
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
