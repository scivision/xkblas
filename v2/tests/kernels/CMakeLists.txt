# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(xkblas-kernels)

set(CMAKE_BUILD_TYPE Debug)

# Lib name
add_executable(xkblas-kernels common/main.cc xkblas/impl.cc)
target_link_libraries(xkblas-kernels xkblas)

include_directories(.)
