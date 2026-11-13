include(FetchContent)
file(READ "${PROJECT_SOURCE_DIR}/Dependencies.lock.json" CAPTURE_LOCK)
string(JSON CHIAKI_REPOSITORY GET "${CAPTURE_LOCK}" chiaki repository)
string(JSON CHIAKI_REVISION GET "${CAPTURE_LOCK}" chiaki revision)
foreach(feature GUI CLI FFMPEG_DECODER PI_DECODER STEAMDECK_NATIVE SETSU SPEEX STEAM_SHORTCUT)
    set(CHIAKI_ENABLE_${feature} OFF CACHE BOOL "Disabled in capture-only core" FORCE)
endforeach()
set(CHIAKI_LIB_ENABLE_OPUS OFF CACHE BOOL "No local Opus decoding" FORCE)
set(CHIAKI_ENABLE_TESTS ${BUILD_TESTING} CACHE BOOL "Upstream core regression tests" FORCE)
string(JSON submodule_count LENGTH "${CAPTURE_LOCK}" chiaki submodules)
math(EXPR last_submodule "${submodule_count} - 1")
set(CHIAKI_SUBMODULES)
foreach(index RANGE ${last_submodule})
    string(JSON submodule GET "${CAPTURE_LOCK}" chiaki submodules ${index})
    list(APPEND CHIAKI_SUBMODULES "${submodule}")
endforeach()
FetchContent_Declare(chiaki
    GIT_REPOSITORY "${CHIAKI_REPOSITORY}"
    GIT_TAG "${CHIAKI_REVISION}"
    GIT_SUBMODULES ${CHIAKI_SUBMODULES}
    GIT_SUBMODULES_RECURSE TRUE
    PATCH_COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE=<SOURCE_DIR>" "-DROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/ApplyChiakiPatches.cmake")
FetchContent_MakeAvailable(chiaki)
