# Tests CMakeLists.txt for pmtr
# Builds and runs unit and integration tests
#
# NOTE: pmtr is Linux-only (uses inotify, signalfd, prctl, SO_PEERCRED)
# Tests will only build and run on Linux systems.

enable_testing()

# Strict compiler warnings for test code
# Note: Not using -Werror because the source files have existing warnings
# that would need to be fixed first (utarray.h sign comparisons, cfg.c parser)
add_compile_options(-Wall -Wextra -Wno-unused-function -Wno-sign-compare -Wno-unused-variable)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src/include)
include_directories(${CMAKE_SOURCE_DIR}/tests)

# Source files needed for tests (shared between multiple test executables)
set(PMTR_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/tok.c
    ${CMAKE_SOURCE_DIR}/src/job.c
    ${CMAKE_SOURCE_DIR}/src/net.c
    ${CMAKE_SOURCE_DIR}/src/cfg.c
    ${CMAKE_SOURCE_DIR}/tests/test_stubs.c
)

# Test executables

# Tokenizer tests
add_executable(test_tokenizer
    test_tokenizer.c
    ${CMAKE_SOURCE_DIR}/src/tok.c
)
target_include_directories(test_tokenizer PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Setter function tests
add_executable(test_setters
    test_setters.c
    ${PMTR_TEST_SOURCES}
)
target_include_directories(test_setters PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Job management tests
add_executable(test_job
    test_job.c
    ${PMTR_TEST_SOURCES}
)
target_include_directories(test_job PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Integration tests
add_executable(test_integration
    test_integration.c
    ${PMTR_TEST_SOURCES}
)
target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Edge case tests
add_executable(test_edge_cases
    test_edge_cases.c
    ${PMTR_TEST_SOURCES}
)
target_include_directories(test_edge_cases PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Network/UDP control tests
add_executable(test_net
    test_net.c
    ${PMTR_TEST_SOURCES}
)
target_include_directories(test_net PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Register tests with CTest
add_test(NAME tokenizer_tests COMMAND test_tokenizer)
add_test(NAME setter_tests COMMAND test_setters)
add_test(NAME job_tests COMMAND test_job)
add_test(NAME integration_tests COMMAND test_integration)
add_test(NAME edge_case_tests COMMAND test_edge_cases)
add_test(NAME net_tests COMMAND test_net)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_tokenizer test_setters test_job test_integration test_edge_cases test_net
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Custom target to run tests with verbose output
add_custom_target(run_tests_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_tokenizer test_setters test_job test_integration test_edge_cases test_net
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# =============================================================================
# Sanitizer builds (AddressSanitizer + UndefinedBehaviorSanitizer)
# Usage: cmake -DCMAKE_BUILD_TYPE=Sanitize ..
# =============================================================================
set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer -g")

if(CMAKE_BUILD_TYPE STREQUAL "Sanitize")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
    message(STATUS "Building with AddressSanitizer and UndefinedBehaviorSanitizer")
endif()

# =============================================================================
# Coverage builds (gcov/lcov)
# Usage: cmake -DCMAKE_BUILD_TYPE=Coverage ..
#        make run_tests
#        make coverage
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage -g -O0")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    message(STATUS "Building with code coverage instrumentation")

    find_program(LCOV lcov)
    find_program(GENHTML genhtml)

    if(LCOV AND GENHTML)
        add_custom_target(coverage
            COMMAND ${LCOV} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${GENHTML} coverage.info --output-directory coverage_report
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "=== Coverage Summary ==="
            COMMAND ${LCOV} --summary coverage.info
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "Full report: ${CMAKE_BINARY_DIR}/coverage_report/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS run_tests
            COMMENT "Generating code coverage report"
        )
    else()
        message(WARNING "lcov/genhtml not found, coverage target disabled")
    endif()
endif()

