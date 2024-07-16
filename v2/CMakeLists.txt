# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Project name
project(xkblas)

##########
# XKBLAS #
##########

# Lib name
add_library(xkblas SHARED
    src/xkblas-context.cc
    src/conf/conf.cc
    src/device/device.cc
    src/device/driver.cc
    src/device/driver_cuda.cc
    src/device/driver_host.cc
    src/kernels/zgemm.cc
    src/logger/logger.cc
    src/scheduler/task.cc
    src/scheduler/thread.cc
)

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS xkblas LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

# Flags
add_compile_options("-DXKBLAS_DEBUG=1")

###############
# DEPENDENCES #
###############

# HWLOC
option(USE_HWLOC "Whether to build the NVIDIA GPU plugin." ON)
if (USE_HWLOC)
    find_package(HWLOC)
    include_directories(${HWLOC_INCLUDE_DIRS})
    target_link_libraries(xkblas PRIVATE ${HWLOC_LIBRARIES})
    message(STATUS "Found HWLOC: ${HWLOC_INCLUDE_DIRS}")
endif()

# CUDA
option(USE_CUDA "Whether to build the NVIDIA GPU plugin." ON)
if (USE_CUDA)
    add_compile_options("-DUSE_CUDA=1")
    find_package(CUDA REQUIRED)
    find_package(CUDA_RT)
    if (NOT ${CUDA_RT_FOUND})
        message(FATAL_ERROR "Xkblas is configured with NVIDIA GPU plugin but CUDA_RT could not be found.")
    endif()
    find_package(CUBLAS)
    if (NOT ${CUBLAS_FOUND})
        message(FATAL_ERROR "Xkblas is configured with NVIDIA GPU plugin but CUBLAS could not be found.")
    endif()
    include_directories("${CUDA_INCLUDE_DIRS}")
    target_link_libraries(xkblas PRIVATE ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    message(STATUS "Found Cuda: ${CUDA_INCLUDE_DIRS}")
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
