# Tests CMakeLists.txt for pmtr
# Builds and runs unit and integration tests

enable_testing()

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

# Register tests with CTest
add_test(NAME tokenizer_tests COMMAND test_tokenizer)
add_test(NAME setter_tests COMMAND test_setters)
add_test(NAME job_tests COMMAND test_job)
add_test(NAME integration_tests COMMAND test_integration)
add_test(NAME edge_case_tests COMMAND test_edge_cases)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_tokenizer test_setters test_job test_integration test_edge_cases
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Custom target to run tests with verbose output
add_custom_target(run_tests_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_tokenizer test_setters test_job test_integration test_edge_cases
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
