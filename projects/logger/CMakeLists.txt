# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
SET(LOGGER_NAME logger)

# Project name
project(${LOGGER_NAME})

SET(SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/logger.cc)

# LOGGER Lib
add_library(${LOGGER_NAME} SHARED ${SOURCE_FILES})

# Include directory
include_directories(include/)

# sync lib deps
option(SYNC "The path to the sync library" OFF)
if (SYNC)
    message(STATUS "Using sync lib at '${SYNC}'")
    include_directories(${SYNC}/include)
else()
    message(FATAL_ERROR "Please set '-DSYNC=path/to/sync'")
endif()

# Installation
install(TARGETS ${LOGGER_NAME} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")
