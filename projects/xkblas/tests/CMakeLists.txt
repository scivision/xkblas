# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(tests)

set(CMAKE_BUILD_TYPE Debug)

add_executable(tests tests.cc)
target_link_libraries(tests ${XKBLAS_NAME} ${XKBLAS_NAME}_cuda_kernels)

add_subdirectory(kernels/)
