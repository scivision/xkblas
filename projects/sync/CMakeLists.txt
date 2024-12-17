# CMake Minimum version
cmake_minimum_required(VERSION 3.7)

# Project name
SET(SYNC_NAME sync)
project(${SYNC_NAME})

# Installation
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

# Tests
# add_subdirectory(tests/)
