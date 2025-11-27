cmake_minimum_required(VERSION 3.11.4)
project(pmtr)
set(CMAKE_C_STANDARD 11)

# =============================================================================
# Platform check: pmtr is Linux-only
# Uses: inotify, signalfd, prctl, SO_PEERCRED, CPU_SET
# =============================================================================
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR
        "pmtr requires Linux. It uses Linux-specific APIs:\n"
        "  - inotify for file change monitoring\n"
        "  - signalfd for signal handling\n"
        "  - prctl for process naming\n"
        "  - SO_PEERCRED for peer credentials\n"
        "Cannot build on ${CMAKE_SYSTEM_NAME}."
    )
endif()

# Option to build tests (default: ON)
option(BUILD_TESTS "Build unit and integration tests" ON)

add_subdirectory(src)
add_subdirectory(docs)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

