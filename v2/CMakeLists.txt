# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# TODO : rename me to 'xkblas' for retro-compatibility (some interfaces may be missing)
SET(XKBLAS_NAME xkblas)

# Project name
project(${XKBLAS_NAME})

message(STATUS "TODO: automate kernel files in the build")

SET(XKBLAS_KERNELS_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ccopyscale.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/cgemm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/cgemmt.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/csymm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/csyr2k.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/csyrk.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ctrmm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ctrsm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dcopyscale.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dgemm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dgemmt.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dsymm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dsyr2k.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dsyrk.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dtrmm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dtrsm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/scopyscale.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/sgemm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/sgemmt.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ssymm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ssyr2k.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ssyrk.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/strmm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/strsm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/zcopyscale.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/zgemm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/zgemmt.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/zsymm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/zsyr2k.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/zsyrk.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ztrmm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ztrsm.cc
)

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/include/xkblas-version-always-run.h
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
    ${CMAKE_SOURCE_DIR}/src/kernels/auto-tile.cc
    ${CMAKE_SOURCE_DIR}/src/logger/logger.cc
    ${CMAKE_SOURCE_DIR}/src/memory-alloc.cc
    ${CMAKE_SOURCE_DIR}/src/memory-coherent.cc
    ${CMAKE_SOURCE_DIR}/src/memory-invalidate.cc
    ${CMAKE_SOURCE_DIR}/src/memory-register.cc
    ${CMAKE_SOURCE_DIR}/src/xkblas-context-task-submit.cc
    ${CMAKE_SOURCE_DIR}/src/xkblas-context.cc
    ${CMAKE_SOURCE_DIR}/src/xkblas-legacy.cc
    ${XKBLAS_KERNELS_SOURCE_FILES}
)

SET(CUDA_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/scopyscale.cu
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/ccopyscale.cu
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/dcopyscale.cu
    ${CMAKE_SOURCE_DIR}/src/kernels/generated/zcopyscale.cu
)

SET(XKBLAS_KERNELS_SOURCE_FILES_DEPS
    ${CMAKE_SOURCE_DIR}/src/kernels/copyscale.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/copyscale.cu
    ${CMAKE_SOURCE_DIR}/src/kernels/gemm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/gemmt.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/generate.py
    ${CMAKE_SOURCE_DIR}/src/kernels/kernel.h
    ${CMAKE_SOURCE_DIR}/src/kernels/symm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/syr2k.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/syrk.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/trmm.cc
    ${CMAKE_SOURCE_DIR}/src/kernels/trsm.cc
)

# XKBLAS version
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/include/xkblas-version-always-run.h # set invalid name so its always generated
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/include/xkblas-version.py ${CMAKE_SOURCE_DIR}/include/
)

# TODO : I don't like this... but it is what it is
#add_custom_command(
#    OUTPUT
#        ${XKBLAS_KERNELS_SOURCE_FILES}
#        ${CUDA_SOURCE_FILES}
#    COMMAND
#        python3 ${CMAKE_SOURCE_DIR}/src/kernels/generate.py ${CMAKE_SOURCE_DIR}/src/kernels ${CMAKE_SOURCE_DIR}/include
#    DEPENDS
#        ${XKBLAS_KERNEL_SOURCE_FILES_DEPS}
#)

option(STRICT "Enable strict compilation mode" OFF)

# TODO : comment to enable kinterval tree heavy debug
#add_compile_definitions(KINTERVAL_BTREE_DISABLE_COHERENCY_CHECKS=1)

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
    add_library(${XKBLAS_NAME}_driver_cpu SHARED src/device/driver_cpu.cc)
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_driver_cpu)
    install(TARGETS ${XKBLAS_NAME}_driver_cpu LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
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

# XKBLAS Lib
add_library(${XKBLAS_NAME} SHARED ${SOURCE_FILES})

if (${HWLOC_FOUND})
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${HWLOC_LIBRARIES})
endif()

# IF USING CUDA
if (${CUDA_RT_FOUND})

    # Cuda driver
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    add_library(${XKBLAS_NAME}_cuda_driver SHARED src/device/driver_cuda.cc)
    if (${HWLOC_FOUND})
        target_link_libraries(${XKBLAS_NAME}_cuda_driver PRIVATE ${HWLOC_LIBRARIES})
    endif()
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_cuda_driver)

    # Cuda kernels
    set_source_files_properties(CUDA_SOURCES, PROPERTIES LANGUAGE CUDA)
    cuda_add_library(${XKBLAS_NAME}_cuda_kernels SHARED ${CUDA_SOURCE_FILES})
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_cuda_kernels)
    install(TARGETS ${XKBLAS_NAME}_cuda_kernels LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

    # Cuda Cublas
    target_link_libraries(${XKBLAS_NAME}_cuda_driver PRIVATE ${CUDA_RT_LIBRARIES} ${CUBLAS_LIBRARIES})
    install(TARGETS ${XKBLAS_NAME}_cuda_driver LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

endif()

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKBLAS_NAME} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

if (STRICT)
    SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv)
    target_compile_options(${XKBLAS_NAME} PRIVATE ${STRICT_OPTIONS})
    if (${CUDA_RT_FOUND})
        target_compile_options(${XKBLAS_NAME}_cuda_driver PRIVATE ${STRICT_OPTIONS})
        target_compile_options(${XKBLAS_NAME}_cuda_kernels PRIVATE ${STRICT_OPTIONS})
    endif()
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
