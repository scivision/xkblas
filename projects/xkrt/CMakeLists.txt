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
    ${CMAKE_SOURCE_DIR}/src/conf/conf.cc
    ${CMAKE_SOURCE_DIR}/src/driver/device.cc
    ${CMAKE_SOURCE_DIR}/src/driver/driver.cc
    ${CMAKE_SOURCE_DIR}/src/driver/stream.cc
    ${CMAKE_SOURCE_DIR}/src/driver/thread.cc
    ${CMAKE_SOURCE_DIR}/src/logger/logger.cc
    ${CMAKE_SOURCE_DIR}/src/logger/bits-to-str.cc
    ${CMAKE_SOURCE_DIR}/src/logger/metric.cc
    ${CMAKE_SOURCE_DIR}/src/host-thread.cc
    ${CMAKE_SOURCE_DIR}/src/runtime.cc
    ${CMAKE_SOURCE_DIR}/src/runtime-task-submit.cc
    ${CMAKE_SOURCE_DIR}/src/runtime-sched.cc
    ${CMAKE_SOURCE_DIR}/src/memory-coherent.cc
    ${CMAKE_SOURCE_DIR}/src/memory-copy.cc
    ${CMAKE_SOURCE_DIR}/src/memory-distribute.cc
    ${CMAKE_SOURCE_DIR}/src/memory-invalidate.cc
    ${CMAKE_SOURCE_DIR}/src/memory-register.cc
    ${CMAKE_SOURCE_DIR}/src/task/task-format.cc
    ${CMAKE_SOURCE_DIR}/src/thread/thread.cc
    ${CMAKE_SOURCE_DIR}/src/thread/deque.cc
)

# XKRT version
add_custom_command(
    OUTPUT  always-run
    COMMAND python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkrt xkrt-version
    COMMAND python3 ${CMAKE_SOURCE_DIR}/../utils/support.py ${CMAKE_SOURCE_DIR}/include/xkrt xkrt-support ${USE_HOST} ${USE_CUDA} ${USE_ZE} ${USE_SYCL} ${USE_CL} ${USE_STATS}
)

add_custom_target(${XKRT}_generated_files ALL DEPENDS always-run)

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
    message(STATUS "Using built-in stats system")
endif()

########
# XKRT #
########

# XKRT Lib
add_library(${XKRT} SHARED ${SOURCE_FILES})

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKRT} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

###############
# DEPENDENCES #
###############

# HWLOC
find_package(HWLOC REQUIRED)
include_directories(${HWLOC_INCLUDE_DIRS})
target_link_libraries(${XKRT} PRIVATE ${HWLOC_LIBRARIES})

# HOST DRIVER
option(USE_HOST "Whether to build the HOST driver." OFF)
if (USE_HOST)
    add_library(${XKRT}_driver_cpu SHARED src/driver/driver_cpu.cc)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_cpu)
    install(TARGETS ${XKRT}_driver_cpu LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# CUDA
option(USE_CUDA "Whether to build the CUDA driver." ON)
if (USE_CUDA)

    find_package(CUDA_DRIVER REQUIRED)
    include_directories("${CUDA_DRIVER_INCLUDE_DIRS}")

    find_package(CUDA_RT REQUIRED)
    include_directories("${CUDA_RT_INCLUDE_DIRS}")

    find_package(CUBLAS REQUIRED)
    include_directories("${CUBLAS_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_cu SHARED src/driver/driver_cu.cc)
    target_link_libraries(${XKRT}_driver_cu PRIVATE ${HWLOC_LIBRARIES} ${CUDA_DRIVER_LIBRARIES} ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_cu)
    install(TARGETS ${XKRT}_driver_cu LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# HIP
option(USE_HIP "Whether to build the HIP driver." ON)
if (USE_HIP)

    find_package(HIP REQUIRED)
    include_directories("${HIP_INCLUDE_DIRS}")

    # find_package(CUBLAS REQUIRED)
    # include_directories("${CUBLAS_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_hip SHARED src/driver/driver_hip.cc)
    target_link_libraries(${XKRT}_driver_hip PRIVATE ${HWLOC_LIBRARIES} ${HIP_LIBRARIES} ${HIPBLAS_LIBRARIES})
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_hip)
    install(TARGETS ${XKRT}_driver_hip LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# Level Zero
option(USE_ZE "Whether to build the Level Zero driver." ON)
if (USE_ZE)

    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_ze SHARED src/driver/driver_ze.cc)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_ze)
    install(TARGETS ${XKRT}_driver_ze LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# SYCL
option(USE_SYCL "Enable the compilation of SYCL and set its interop with Level Zero driver" ON)
if (USE_SYCL)
    find_package(SYCL REQUIRED)
    include_directories(${SYCL_INCLUDE_DIRS})
    target_link_libraries(${XKRT} PRIVATE ${SYCL_LIBRARIES})
endif()

# OpenCL
option(USE_CL "Whether to build the OpenCL driver." ON)
if (USE_CL)

    find_package(CL REQUIRED)
    include_directories("${CL_INCLUDE_DIRS}")

    add_library(${XKRT}_driver_cl SHARED src/driver/driver_cl.cc)
    target_link_libraries(${XKRT} PRIVATE ${XKRT}_driver_cl ${CL_LIBRARIES})
    install(TARGETS ${XKRT}_driver_cl LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

option(STRICT "Enable strict compilation mode" ON)
if (STRICT)
    SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${XKRT} PRIVATE ${STRICT_OPTIONS})
    if (${CUDA_DRIVER_FOUND})
        target_compile_options(${XKRT}_driver_cu PRIVATE ${STRICT_OPTIONS})
    endif()
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
enable_testing()
