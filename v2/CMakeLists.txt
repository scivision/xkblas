# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(xkblas)

# Lib name
add_library(xkblas SHARED
    src/xkblas.cc
    src/kernel/zgemm.cc
    src/logger/logger.cc
    src/scheduler/sync.cc
)

# Include directory
include_directories(include/)
include_directories(src/)
include_directories(src/logger)
include_directories(src/scheduler)
include_directories(src/sync)

# Installation
install(TARGETS xkblas LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

# Sub projects
add_subdirectory(tests/)
