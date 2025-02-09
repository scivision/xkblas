# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(tests)

set(CMAKE_BUILD_TYPE Debug)

add_executable(tests tests.cc)
target_link_libraries(tests ${XKBLAS_NAME})
if (ENABLE_CU)
    target_link_libraries(tests ${XKBLAS_NAME}_kernels_cuda)
endif()

add_subdirectory(kernels/)
