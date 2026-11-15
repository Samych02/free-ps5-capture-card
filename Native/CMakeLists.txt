add_library(CaptureEngine SHARED src/Engine.cpp src/Pairing.cpp src/Discovery.cpp include/CaptureEngine.h)
target_compile_definitions(CaptureEngine PRIVATE CAPTURE_ENGINE_BUILD=1)
target_compile_features(CaptureEngine PRIVATE cxx_std_17)
target_include_directories(CaptureEngine PUBLIC include)
target_link_libraries(CaptureEngine PRIVATE chiaki-lib)
add_library(capture-encoded STATIC src/EncodedMedia.cpp src/Timeline.cpp src/PacketQueue.cpp src/AnnexB.cpp src/AudioFormat.cpp)
target_compile_features(capture-encoded PUBLIC cxx_std_17)
target_include_directories(capture-encoded PUBLIC src)
target_link_libraries(capture-encoded PUBLIC chiaki-lib)
target_link_libraries(CaptureEngine PRIVATE capture-encoded)
if(APPLE)
    set_target_properties(CaptureEngine PROPERTIES
        FRAMEWORK TRUE FRAMEWORK_VERSION A
        MACOSX_FRAMEWORK_IDENTIFIER org.freeps5capturecard.engine
        PUBLIC_HEADER include/CaptureEngine.h
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/frameworks"
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_NAME_DIR TRUE
        CXX_VISIBILITY_PRESET hidden)
    add_custom_command(TARGET CaptureEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:CaptureEngine>/Modules"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/module.modulemap"
            "$<TARGET_BUNDLE_DIR:CaptureEngine>/Modules/module.modulemap")
else()
    set_target_properties(CaptureEngine PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/FFmpegDependency.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/CapturePlatform.cmake")
target_link_libraries(CaptureEngine PRIVATE capture-platform)
function(capture_add_media_libraries suffix ffmpeg)
    add_library(capture-media${suffix} STATIC src/MatroskaMuxer.cpp)
    target_compile_features(capture-media${suffix} PUBLIC cxx_std_17)
    target_include_directories(capture-media${suffix} PUBLIC src)
    target_link_libraries(capture-media${suffix} PUBLIC ${ffmpeg} capture-platform)
    add_library(capture-live${suffix} STATIC src/LiveFormat.cpp src/SPSMetadata.cpp src/Session.cpp src/Pipeline.cpp src/Diagnostics.cpp)
    target_include_directories(capture-live${suffix} PUBLIC include src)
    target_compile_features(capture-live${suffix} PUBLIC cxx_std_17)
    target_link_libraries(capture-live${suffix} PUBLIC capture-encoded capture-media${suffix})
endfunction()
capture_add_media_libraries("" capture-ffmpeg)
if(BUILD_TESTING)
    # Same production sources, isolated fixture-only FFmpeg input capabilities.
    capture_add_media_libraries(-fixtures capture-ffmpeg-fixtures)
endif()
target_link_libraries(CaptureEngine PRIVATE capture-live)
