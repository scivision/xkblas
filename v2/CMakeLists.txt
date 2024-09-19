# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Project name
project(xkblas)

message(STATUS "TODO: automate kernel files in the build")

SET(SOURCE_FILES
    src/xkblas-context.cc
    src/xkblas-context-task-submit.cc
    src/memory-coherent-async.cc
    src/conf/conf.cc
    src/device/device.cc
    src/device/driver.cc
    src/device/offloader.cc
    src/device/stream.cc
    src/device/stream-instruction-submit.cc
    src/device/task.cc
    src/device/task-format.cc
    src/device/thread.cc
    src/device/thread-producer.cc
    src/device/thread-worker.cc
    src/kernels/auto-tile.cc
    src/kernels/sgemm.cc
    src/kernels/cgemm.cc
    src/kernels/dgemm.cc
    src/kernels/zgemm.cc
    src/kernels/strsm.cc
    src/kernels/ctrsm.cc
    src/kernels/dtrsm.cc
    src/kernels/ztrsm.cc
    src/logger/logger.cc
)

add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/src/kernels/sgemm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/dgemm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/cgemm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/zgemm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/strsm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/dtrsm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/ctrsm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/ztrsm.cc
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/src/kernels/generate.py ${CMAKE_SOURCE_DIR}/src/kernels ${CMAKE_SOURCE_DIR}/include
    DEPENDS
        ${CMAKE_SOURCE_DIR}/src/kernels/generate.py
        ${CMAKE_SOURCE_DIR}/src/kernels/gemm.cc
        ${CMAKE_SOURCE_DIR}/src/kernels/trsm.cc
)

option(STRICT "Enable strict compilation mode" OFF)

################
# STATS SYSTEM #
################
option(USE_STATS "Whether to compile the built-in stat system." OFF)
if (USE_STATS)
    LIST(APPEND SOURCE_FILES src/stats/stats.cc)
    add_compile_definitions(USE_STATS=1)
    message(STATUS "Using built-in stats system")
endif()

###############
# DEPENDENCES #
###############

# HWLOC
option(USE_HWLOC "Whether to build the NVIDIA GPU plugin." ON)
if (USE_HWLOC)
    find_package(HWLOC)
    include_directories(${HWLOC_INCLUDE_DIRS})
    message(STATUS "Found HWLOC: ${HWLOC_INCLUDE_DIRS}")
endif()

# HOST DRIVER
option(USE_CPU "Whether to build the CPU driver." OFF)
if (USE_CPU)
    add_compile_definitions(USE_CPU=1)
endif()

# CUDA
option(USE_CUDA "Whether to build the NVIDIA GPU plugin." ON)
if (USE_CUDA)
    add_compile_definitions(USE_CUDA=1)
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
    message(STATUS "Found Cuda: ${CUDA_INCLUDE_DIRS}")
endif()

##########
# XKBLAS #
##########

# Lib name
add_library(xkblas SHARED ${SOURCE_FILES})

if (${HWLOC_FOUND})
    target_link_libraries(xkblas PRIVATE ${HWLOC_LIBRARIES})
endif()

# Host driver
add_library(xkblas_driver_cpu SHARED src/device/driver_cpu.cc)
target_link_libraries(xkblas PRIVATE xkblas_driver_cpu)

# Cuda driver
if (${CUDA_RT_FOUND})
    target_link_libraries(xkblas PRIVATE ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    add_library(xkblas_driver_cuda SHARED src/device/driver_cuda.cc)
    if (${HWLOC_FOUND})
        target_link_libraries(xkblas_driver_cuda PRIVATE ${HWLOC_LIBRARIES})
    endif()
    target_link_libraries(xkblas_driver_cuda PRIVATE ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    target_link_libraries(xkblas PRIVATE xkblas_driver_cuda)
endif()

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS xkblas LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

if (STRICT)
    target_compile_options(xkblas PRIVATE -Wfatal-errors)
    if (${CUDA_RT_FOUND})
        target_compile_options(xkblas_driver_cuda PRIVATE -Wfatal-errors)
    endif()
endif()


################
# Sub projects #
################
add_subdirectory(tests/)
