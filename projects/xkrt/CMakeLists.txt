# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

SET(XKRT xkrt)

# Project name
project(${XKRT})

###############
# DEPENDENCES #
###############

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/include/xkrt/xkrt-version-always-run.h
    ${CMAKE_SOURCE_DIR}/include/xkrt/xkrt-support-always-run.h
    ${CMAKE_SOURCE_DIR}/src/conf/conf.cc
    ${CMAKE_SOURCE_DIR}/src/driver/device.cc
    ${CMAKE_SOURCE_DIR}/src/driver/driver.cc
    ${CMAKE_SOURCE_DIR}/src/driver/offloader.cc
    ${CMAKE_SOURCE_DIR}/src/driver/stream.cc
    ${CMAKE_SOURCE_DIR}/src/driver/thread-producer.cc
    ${CMAKE_SOURCE_DIR}/src/driver/thread-worker.cc
    ${CMAKE_SOURCE_DIR}/src/driver/thread.cc
    ${CMAKE_SOURCE_DIR}/src/logger/logger.cc
    ${CMAKE_SOURCE_DIR}/src/runtime.cc
    ${CMAKE_SOURCE_DIR}/src/runtime-task-submit.cc
    ${CMAKE_SOURCE_DIR}/src/runtime-sched.cc
    ${CMAKE_SOURCE_DIR}/src/memory-coherent.cc
    ${CMAKE_SOURCE_DIR}/src/memory-invalidate.cc
    ${CMAKE_SOURCE_DIR}/src/memory-register.cc
    ${CMAKE_SOURCE_DIR}/src/task/task-format.cc
)

# XKRT version
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/include/xkrt/xkrt-version-always-run.h # set invalid name so its always generated
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkrt xkrt-version
)

# XKRT support
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/include/xkrt/xkrt-support-always-run.h # set invalid name so its always generated
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/../utils/support.py ${CMAKE_SOURCE_DIR}/include/xkrt xkrt-support ${USE_HOST} ${USE_CUDA} ${USE_ZE}
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
    LIST(APPEND SOURCE_FILES src/stats.cc)
    add_compile_definitions(USE_STATS=1)
    message(STATUS "Using built-in stats system")
endif()

###############
# DEPENDENCES #
###############

# HWLOC
find_package(HWLOC REQUIRED)
include_directories(${HWLOC_INCLUDE_DIRS})
message(STATUS "Found HWLOC: ${HWLOC_INCLUDE_DIRS}")

# HOST DRIVER
option(USE_HOST "Whether to build the HOST driver." OFF)
if (USE_HOST)
    add_compile_definitions(USE_HOST=1)
    add_library(${XKRT}_driver_cpu SHARED src/driver/driver_cpu.cc)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_cpu)
    install(TARGETS ${XKRT}_driver_cpu LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# CUDA
option(USE_CUDA "Whether to build the CUDA driver." ON)
if (USE_CUDA)
    add_compile_definitions(USE_CUDA=1)

    find_package(CUDA_RT REQUIRED)
    include_directories("${CUDA_RT_INCLUDE_DIRS}")
    message(STATUS "Found Cuda: ${CUDA_RT_INCLUDE_DIRS}")

    find_package(CUBLAS REQUIRED)
    include_directories("${CUBLAS_INCLUDE_DIRS}")
    message(STATUS "Found Cublas: ${CUBLAS_INCLUDE_DIRS}")

endif()

# Level Zero
option(USE_ZE "Whether to build the Level Zero driver." ON)
if (USE_ZE)
    add_compile_definitions(USE_ZE=1)

    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")
    message(STATUS "Found Cuda: ${ZE_INCLUDE_DIRS}")

    #    find_package(CLBLAS REQUIRED)
    #    include_directories("${CLBLAS_INCLUDE_DIRS}")
    #    message(STATUS "Found clblas: ${CLBLAS_INCLUDE_DIRS}")

endif()

##########
# XKRT #
##########

# XKRT Lib
add_library(${XKRT} SHARED ${SOURCE_FILES})

if (${HWLOC_FOUND})
    target_link_libraries(${XKRT} PRIVATE ${HWLOC_LIBRARIES})
endif()

# IF USING CUDA
if (${CUDA_RT_FOUND})

    # Cuda driver
    add_library(${XKRT}_driver_cuda SHARED src/driver/driver_cuda.cc)
    target_link_libraries(${XKRT}_driver_cuda PRIVATE ${HWLOC_LIBRARIES} ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_cuda)
    install(TARGETS ${XKRT}_driver_cuda LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# IF USING LEVEL ZERO
if (${ZE_FOUND})
    # Level zero driver
    add_library(${XKRT}_driver_ze SHARED src/driver/driver_ze.cc)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_ze)
    install(TARGETS ${XKRT}_driver_ze LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKRT} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

if (STRICT)
    SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${XKRT} PRIVATE ${STRICT_OPTIONS})
    if (${CUDA_RT_FOUND})
        target_compile_options(${XKRT}_driver_cuda PRIVATE ${STRICT_OPTIONS})
    endif()
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
