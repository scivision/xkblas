# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(PTR_NAME ptr)

# Project name
project(${PTR_NAME})

###############
# DEPENDENCES #
###############

# sync lib deps
option(SYNC "The path to the sync library" OFF)
if (SYNC)
    message(STATUS "Using sync lib at '${SYNC}'")
    include_directories(${SYNC}/include)
else()
    message(FATAL_ERROR "Please set '-DSYNC=path/to/sync'")
endif()

option(LOGGER "The path to the logger library" OFF)
if (LOGGER)
    message(STATUS "Using logger lib at '${LOGGER}'")
    include_directories(${LOGGER}/include)
else()
    message(FATAL_ERROR "Please set '-DLOGGER=path/to/logger'")
endif()

#######
# PTR #
#######

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/include/ptr/ptr-version-always-run.h
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
    ${CMAKE_SOURCE_DIR}/src/memory-alloc.cc
    ${CMAKE_SOURCE_DIR}/src/memory-coherent.cc
    ${CMAKE_SOURCE_DIR}/src/memory-invalidate.cc
    ${CMAKE_SOURCE_DIR}/src/memory-register.cc
    ${CMAKE_SOURCE_DIR}/src/runtime-task-submit.cc
    ${CMAKE_SOURCE_DIR}/src/runtime.cc
)

# PTR version
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/include/ptr/ptr-version-always-run.h # set invalid name so its always generated
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/ptr ptr-version
)

option(STRICT "Enable strict compilation mode" OFF)

# uncomment to disable kinterval tree heavy debug
option(DISABLE_HEAVY_DEBUG "When compiling in BUILD_TYPE=Debug, you can disable heavy assertion tests in the cube-tree structure setting this variable to 'ON' - this is not recommanded" OFF)
if (DISABLE_HEAVY_DEBUG)
    add_compile_definitions(CUBE_TREE_DISABLE_COHERENCY_CHECKS=1)
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
    find_package(HWLOC)
    include_directories(${HWLOC_INCLUDE_DIRS})
    message(STATUS "Found HWLOC: ${HWLOC_INCLUDE_DIRS}")
endif()

# HOST DRIVER
option(USE_CPU "Whether to build the CPU driver." OFF)
if (USE_CPU)
    add_compile_definitions(USE_CPU=1)
    add_library(${PTR_NAME}_driver_cpu SHARED src/device/driver_cpu.cc)
    target_link_libraries(${PTR_NAME} PRIVATE ${PTR_NAME}_driver_cpu)
    install(TARGETS ${PTR_NAME}_driver_cpu LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
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
    find_package(CUBLAS)
    if (NOT ${CUBLAS_FOUND})
        message(FATAL_ERROR "Xkblas could not find CUBLAS.")
    endif()
    include_directories("${CUDA_INCLUDE_DIRS}")
    message(STATUS "Found Cuda: ${CUDA_INCLUDE_DIRS}")
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
# PTR #
##########

# PTR Lib
add_library(${PTR_NAME} SHARED ${SOURCE_FILES})

if (${HWLOC_FOUND})
    target_link_libraries(${PTR_NAME} PRIVATE ${HWLOC_LIBRARIES})
endif()

# IF USING CUDA
if (${CUDA_RT_FOUND})

    # Cuda driver
    target_link_libraries(${PTR_NAME} PRIVATE ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    add_library(${PTR_NAME}_cuda_driver SHARED src/device/driver_cuda.cc)
    if (${HWLOC_FOUND})
        target_link_libraries(${PTR_NAME}_cuda_driver PRIVATE ${HWLOC_LIBRARIES})
    endif()
    target_link_libraries(${PTR_NAME} PRIVATE ${PTR_NAME}_cuda_driver)

endif()

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${PTR_NAME} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

if (STRICT)
    SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${PTR_NAME} PRIVATE ${STRICT_OPTIONS})
    if (${CUDA_RT_FOUND})
        target_compile_options(${PTR_NAME}_driver_cuda PRIVATE ${STRICT_OPTIONS})
    endif()
endif()

################
# Sub projects #
################
# add_subdirectory(tests/)
