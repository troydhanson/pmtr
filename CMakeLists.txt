cmake_minimum_required(VERSION 3.11.4)
project(pmtr)
set(CMAKE_C_STANDARD 11)

# Option to build tests (default: ON)
option(BUILD_TESTS "Build unit and integration tests" ON)

add_subdirectory(src)
add_subdirectory(docs)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

