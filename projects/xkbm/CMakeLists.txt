# CMake Minimum version
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKBM xkbm)

# Project name
project(${XKBM})

###############
# DEPENDENCES #
###############

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/include/xkbm/xkbm-version-always-run.h
    ${CMAKE_SOURCE_DIR}/src/main.cc
    ${CMAKE_SOURCE_DIR}/src/benchmark.cc
    ${CMAKE_SOURCE_DIR}/src/allocator.cc
    ${CMAKE_SOURCE_DIR}/src/cuda/benchmarks.cc
    ${CMAKE_SOURCE_DIR}/src/libnuma/benchmarks.cc
    ${CMAKE_SOURCE_DIR}/src/hwloc/benchmarks.cc
    ${CMAKE_SOURCE_DIR}/src/opencl/benchmarks.cc
    ${CMAKE_SOURCE_DIR}/src/ze/benchmarks.cc
    ${CMAKE_SOURCE_DIR}/src/hip/benchmarks.cc
    ${CMAKE_SOURCE_DIR}/src/xkrt/benchmarks.cc
)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# XKBM version
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/include/xkbm/xkbm-version-always-run.h # set invalid name so its always generated
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkbm xkbm-version
)

########
# XKBM #
########

# XKBM Lib
add_executable(${XKBM} ${SOURCE_FILES})
target_link_libraries(${XKBM} PRIVATE ${AML_LIBRARIES} ${EXCIT_LIBRARIES} ${HWLOC_LIBRARIES})

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKBM} DESTINATION bin)

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")
target_link_libraries(${XKBM} PRIVATE ${XKRT_LIBRARIES})

# HWLOC
find_package(HWLOC REQUIRED)
include_directories(${HWLOC_INCLUDE_DIRS})
target_link_libraries(${XKBM} PRIVATE ${HWLOC_LIBRARIES})

# CUDA
option(USE_CUDA "Whether to build CUDA tests." ON)
if (USE_CUDA)
    find_package(CUDA_DRIVER REQUIRED)
    include_directories("${CUDA_DRIVER_INCLUDE_DIRS}")
    target_link_libraries(${XKBM} PRIVATE ${CUDA_DRIVER_LIBRARIES})
endif()

# Level Zero
option(USE_ZE "Whether to build Intel's Level Zero tests." ON)
if (USE_ZE)
    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")
    target_link_libraries(${XKBM} PRIVATE ${ZE_LIBRARIES})
endif()
