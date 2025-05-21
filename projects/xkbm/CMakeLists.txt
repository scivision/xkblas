# CMake Minimum version
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKBM xkbm)

# Project name
project(${XKBM})

###########
# OPTIONS #
###########

option(USE_AML "Whether to build the AML driver." OFF)
option(USE_CUDA "Whether to build the CUDA driver." OFF)
option(USE_XKRT "Whether to build the XKRT driver." OFF)
option(USE_ZE "Whether to build the Level Zero driver." OFF)
option(STRICT "Enable strict compilation mode" ON)

include(BooleanUtils)
convert_bool(USE_AML)
convert_bool(USE_CUDA)
convert_bool(USE_CUDA)
convert_bool(USE_XKRT)
convert_bool(USE_ZE)
configure_file(${CMAKE_SOURCE_DIR}/include/xkbm/support.h.in ${CMAKE_SOURCE_DIR}/include/xkbm/support.h)

# XKBM version and supports
add_custom_command(
    OUTPUT  always-run
    COMMAND python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkbm version
    COMMENT "Always generate version file before building the project"
)
add_custom_target(always_generate_files ALL DEPENDS always-run)

###############
# DEPENDENCES #
###############
SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/main.cc
    ${CMAKE_SOURCE_DIR}/src/benchmark.cc
    ${CMAKE_SOURCE_DIR}/src/allocator.cc
)

if (USE_XKRT)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/xkrt/benchmarks.cc)
endif()

if (USE_CUDA)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/cuda/benchmarks.cc)
endif()

if (USE_ZE)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/ze/benchmarks.cc)
endif()

if (USE_AML)
    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/aml/benchmarks.cc)
endif()

########
# XKBM #
########

# XKBM Lib
add_executable(${XKBM} ${SOURCE_FILES})
add_dependencies(${XKBM} always_generate_files)

# Include directory
include_directories(${CMAKE_SOURCE_DIR}/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/)

# Installation
install(TARGETS ${XKBM} DESTINATION bin)

###############
# DEPENDENCES #
###############

# XK Runtime
if (USE_XKRT)
    find_package(XKRT REQUIRED)
    include_directories("${XKRT_INCLUDE_DIRS}")
    target_link_libraries(${XKBM} PRIVATE ${XKRT_LIBRARIES})
endif()

# CUDA
if (USE_CUDA)
    find_package(CUDA_DRIVER REQUIRED)
    include_directories("${CUDA_DRIVER_INCLUDE_DIRS}")
    target_link_libraries(${XKBM} PRIVATE ${CUDA_DRIVER_LIBRARIES})
endif()

# Level Zero
if (USE_ZE)
    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")
    target_link_libraries(${XKBM} PRIVATE ${ZE_LIBRARIES})
    target_compile_options(${XKBM} PRIVATE -fsycl)
endif()

# AML
if (USE_AML)
    find_package(AML REQUIRED)
    find_package(EXCIT REQUIRED)
    include_directories("${AML_INCLUDE_DIRS}")
    include_directories("${EXCIT_INCLUDE_DIRS}")
    target_link_libraries(${XKBM} PRIVATE ${AML_LIBRARIES} ${EXCIT_LIBRARIES})
endif()
