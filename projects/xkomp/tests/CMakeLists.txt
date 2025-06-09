cmake_minimum_required(VERSION 3.10)
include(CTest)

# Project name
project(tests)

# OpenMP
find_package(HWLOC  REQUIRED)
find_package(XKRT   REQUIRED)
find_package(OpenMP REQUIRED)

# list of tests
set(TEST_SOURCES parallel.cc parallel-for.cc target.cc task.cc)

# Loop over each test source file
foreach(TEST_FILE ${TEST_SOURCES})

    # Get the file name without the extension (e.g., "test" from "test.c")
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    # Create test
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE ${XKOMP_LIBRARIES} ${XKRT_LIBRARIES} ${HWLOC_LIBRARIES} OpenMP::OpenMP_CXX)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

endforeach()
