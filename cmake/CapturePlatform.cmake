# Extension point, not a claim of additional supported platforms. Future ports
# provide these interfaces without changing media/timeline/queue algorithms.
if(APPLE)
    set(capture_platform_sources
        src/platform/macos/LocalTCPServer.cpp src/platform/macos/NetworkSetup.cpp)
else()
    message(FATAL_ERROR "Capture platform networking adapter is not implemented for ${CMAKE_SYSTEM_NAME}; see docs/portability.md. Only arm64 macOS 26 is currently supported.")
endif()
add_library(capture-platform STATIC ${capture_platform_sources})
target_compile_features(capture-platform PUBLIC cxx_std_17)
target_include_directories(capture-platform PUBLIC "${PROJECT_SOURCE_DIR}/Native/src")
# The transport uses FFmpeg error constants, not its media/network implementation.
# Headers are shared by both same-source profiles; do not link runtime FFmpeg
# into fixture helpers, which deliberately use a different static profile.
target_include_directories(capture-platform PRIVATE "${CAPTURE_FFMPEG_ROOT}/include")
target_link_libraries(capture-platform PRIVATE chiaki-lib)
