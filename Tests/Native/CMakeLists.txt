add_executable(capture-core-tests main.cpp InterfaceTests.cpp)
target_compile_features(capture-core-tests PRIVATE cxx_std_17)
target_link_libraries(capture-core-tests PRIVATE CaptureEngine munit)
add_test(NAME capture-interface COMMAND capture-core-tests /capture/interface)
set_tests_properties(capture-interface PROPERTIES TIMEOUT 15)
add_executable(capture-replay ../Support/replay.cpp)
target_link_libraries(capture-replay PRIVATE capture-media)
set_target_properties(capture-replay PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME capture-transport COMMAND "${PYTHON_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/Support/integration.py"
    --build-dir "${CMAKE_BINARY_DIR}" --case all)
set_tests_properties(capture-transport PROPERTIES TIMEOUT 120 LABELS transport)
