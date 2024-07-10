# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Project name
project(xkblas)

# Options
option(USE_CUDA "Whether to build the NVIDIA GPU plugin" ON)
if (USE_CUDA)
    find_package(CUDA REQUIRED)
    find_package(CUDA_RT)
    if (NOT ${CUDA_RT_FOUND})
        message(FATAL_ERROR "Xkblas is configured with NVIDIA GPU plugin but CUDA_RT could not be found.")
    endif()
    find_package(CUBLAS)
    if (NOT ${CUBLAS_FOUND})
        message(FATAL_ERROR "Xkblas is configured with NVIDIA GPU plugin but CUBLAS could not be found.")
    endif()
    add_compile_options("-DUSE_CUDA=1")
    message(STATUS "Configuring with CUDA runtime plugin")
endif()

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
