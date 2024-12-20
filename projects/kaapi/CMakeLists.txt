# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(KAAPI kaapi)

# Project name
project(${KAAPI})

###############
# DEPENDENCES #
###############

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/include/kaapi/kaapi-version-always-run.h
    ${CMAKE_SOURCE_DIR}/src/conf/conf.cc
    ${CMAKE_SOURCE_DIR}/src/device/device.cc
    ${CMAKE_SOURCE_DIR}/src/device/driver.cc
    ${CMAKE_SOURCE_DIR}/src/device/offloader.cc
    ${CMAKE_SOURCE_DIR}/src/device/stream-instruction-submit.cc
    ${CMAKE_SOURCE_DIR}/src/device/stream.cc
    ${CMAKE_SOURCE_DIR}/src/device/task-format.cc
    ${CMAKE_SOURCE_DIR}/src/device/task.cc
    ${CMAKE_SOURCE_DIR}/src/device/thread-producer.cc
    ${CMAKE_SOURCE_DIR}/src/device/thread-worker.cc
    ${CMAKE_SOURCE_DIR}/src/device/thread.cc
    ${CMAKE_SOURCE_DIR}/src/logger/logger.cc
    ${CMAKE_SOURCE_DIR}/src/device/memory-coherent.cc
    ${CMAKE_SOURCE_DIR}/src/device/memory-invalidate.cc
    ${CMAKE_SOURCE_DIR}/src/device/memory-register.cc
    ${CMAKE_SOURCE_DIR}/src/runtime-task-submit.cc
    ${CMAKE_SOURCE_DIR}/src/runtime.cc
)

# KAAPI version
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/include/kaapi/kaapi-version-always-run.h # set invalid name so its always generated
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/kaapi kaapi-version
)

option(STRICT "Enable strict compilation mode" OFF)

# uncomment to disable kinterval tree heavy debug
option(ENABLE_HEAVY_DEBUG "Heavy assertion tests in the khp-tree structure, setting this variable to 'ON' will considerably slowdown task insertion, but can help debugging the khp-trees" OFF)
if (ENABLE_HEAVY_DEBUG)
    add_compile_definitions(KHP_TREE_ENABLE_COHERENCY_CHECKS=1)
endif()

################
# STATS SYSTEM #
################
option(USE_STATS "Whether to compile the built-in stat system." ON)
if (USE_STATS)
    LIST(APPEND SOURCE_FILES src/stats/stats.cc)
    add_compile_definitions(USE_STATS=1)
    message(STATUS "Using built-in stats system")
endif()

###############
# DEPENDENCES #
###############

# HWLOC
option(USE_HWLOC "Whether to build the NVIDIA GPU driver." ON)
if (USE_HWLOC)
    find_package(HWLOC REQUIRED)
    include_directories(${HWLOC_INCLUDE_DIRS})
    message(STATUS "Found HWLOC: ${HWLOC_INCLUDE_DIRS}")
endif()

# HOST DRIVER
option(USE_CPU "Whether to build the CPU driver." OFF)
if (USE_CPU)
    add_compile_definitions(USE_CPU=1)
    add_library(${KAAPI}_driver_cpu SHARED src/device/driver_cpu.cc)
    target_link_libraries(${KAAPI} PRIVATE ${KAAPI}_driver_cpu)
    install(TARGETS ${KAAPI}_driver_cpu LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# CUDA
option(USE_CUDA "Whether to build the NVIDIA GPU driver." ON)
if (USE_CUDA)
    add_compile_definitions(USE_CUDA=1)
    find_package(CUDA REQUIRED)
    find_package(CUDA_RT)
    if (NOT ${CUDA_RT_FOUND})
        message(FATAL_ERROR "Xkblas could not find CUDA_RT.")
    endif()
    include_directories("${CUDA_INCLUDE_DIRS}")
    message(STATUS "Found Cuda: ${CUDA_INCLUDE_DIRS}")

    find_package(CUBLAS REQUIRED)
    if (NOT ${CUBLAS_FOUND})
        message(FATAL_ERROR "Xkblas could not find CUBLAS.")
    endif()

endif()

# HIP
option(USE_HIP "Whether to build the AMD GPU driver." OFF)
if (USE_HIP)

    add_compile_definitions(USE_HIP=1)
    find_package(HIP REQUIRED)
    if (NOT ${HIP_FOUND})
        message(FATAL_ERROR "Xkblas could not find HIP.")
    endif()
    include_directories("${HIP_INCLUDE_DIRS}")
    message(STATUS "Found HIP: ${HIP_LIBRARIES}")

    find_package(HIPBLAS)
    if (NOT ${HIPBLAS_FOUND})
        message(FATAL_ERROR "Xkblas could not find HIPBLAS.")
    endif()
    message(STATUS "Found HIPBLAS: ${HIPBLAS_LIBRARIES}")

endif()

if (USE_CUDA AND USE_HIP)
    message(STATUS "Enabling both CUDA and HIP is experimental, what kind of machine are you running on ?")
endif()


##########
# KAAPI #
##########

# KAAPI Lib
add_library(${KAAPI} SHARED ${SOURCE_FILES})

if (${HWLOC_FOUND})
    target_link_libraries(${KAAPI} PRIVATE ${HWLOC_LIBRARIES})
endif()

# IF USING CUDA
if (${CUDA_RT_FOUND})

    # Cuda driver
    add_library(${KAAPI}_driver_cuda SHARED src/device/driver_cuda.cc)
    if (${HWLOC_FOUND})
        target_link_libraries(${KAAPI}_driver_cuda PRIVATE ${HWLOC_LIBRARIES})
    endif()
    target_link_libraries(${KAAPI}_driver_cuda PRIVATE ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    target_link_libraries(${KAAPI} PRIVATE ${KAAPI}_driver_cuda)
    install(TARGETS ${KAAPI}_driver_cuda LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${KAAPI} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

if (STRICT)
    SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${KAAPI} PRIVATE ${STRICT_OPTIONS})
    if (${CUDA_RT_FOUND})
        target_compile_options(${KAAPI}_driver_cuda PRIVATE ${STRICT_OPTIONS})
    endif()
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
