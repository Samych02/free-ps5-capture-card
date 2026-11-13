add_library(CaptureEngine SHARED src/Engine.cpp src/Pairing.cpp src/Discovery.cpp include/CaptureEngine.h)
target_compile_features(CaptureEngine PRIVATE cxx_std_17)
target_include_directories(CaptureEngine PUBLIC include)
target_link_libraries(CaptureEngine PRIVATE chiaki-lib)
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

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPTURE_AV REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)
add_library(capture-media STATIC src/MatroskaMuxer.cpp src/LocalTCPServer.cpp)
target_compile_features(capture-media PUBLIC cxx_std_17)
target_include_directories(capture-media PUBLIC src)
target_link_libraries(capture-media PUBLIC PkgConfig::CAPTURE_AV)
