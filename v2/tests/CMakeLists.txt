# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(tests)

set(CMAKE_BUILD_TYPE Debug)

# Lib name
add_executable(gemm gemm.cc)
target_link_libraries(gemm xkblas)

