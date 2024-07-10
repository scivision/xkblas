# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(xkblas)

# Lib name
add_library(xkblas SHARED
    src/xkblas.cc
    src/conf/conf.cc
    src/device/devices.cc
    src/device/driver/driver.cc
    src/kernels/zgemm.cc
    src/logger/logger.cc
    src/scheduler/thread.cc
)

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS xkblas LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

# Sub projects
add_subdirectory(tests/)
