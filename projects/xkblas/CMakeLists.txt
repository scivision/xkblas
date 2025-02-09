# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Project name
SET(XKBLAS_NAME xkblas)
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
    ${CMAKE_SOURCE_DIR}/src/auto-tile.cc
    ${CMAKE_SOURCE_DIR}/src/conf.cc
    ${CMAKE_SOURCE_DIR}/src/context.cc
    ${CMAKE_SOURCE_DIR}/src/legacy.cc
    ${CMAKE_SOURCE_DIR}/src/memory-alloc.cc
    ${CMAKE_SOURCE_DIR}/src/memory-coherent.cc
    ${CMAKE_SOURCE_DIR}/src/memory-invalidate.cc
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
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkblas xkblas-version
)

add_custom_command(
    OUTPUT
        ${XKBLAS_KERNELS_SOURCE_FILES}
        ${CUDA_SOURCE_FILES}
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/src/kernels/generate.py ${CMAKE_SOURCE_DIR}/src/kernels ${CMAKE_SOURCE_DIR}/include
    DEPENDS
        ${XKBLAS_KERNELS_SOURCE_FILES_DEPS}
)

option(STRICT "Enable strict compilation mode" OFF)

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
message(STATUS "Found XKRT: ${XKRT_INCLUDE_DIRS}")
include_directories("${XKRT_INCLUDE_DIRS}")

find_package(CUBLAS)
if (${CUBLAS_FOUND})
    include_directories("${CUBLAS_INCLUDE_DIRS}")
    message(STATUS "Found Cublas: ${CUBLAS_INCLUDE_DIRS}")
else()
    message(STATUS "Cublas not found")
endif()

##########
# XKBLAS #
##########

# XKBLAS Lib
add_library(${XKBLAS_NAME} SHARED ${SOURCE_FILES})
target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKRT}/lib/libxkrt.so)

if (${CUBLAS_FOUND})
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${CUBLAS_LIBRARIES})
endif()

# Cuda kernels
option(ENABLE_CU "Enable the compilation of custom cuda kernels" ON)
if (ENABLE_CU)
    find_package(CUDA)
    set_source_files_properties(CUDA_SOURCES, PROPERTIES LANGUAGE CUDA)
    cuda_add_library(${XKBLAS_NAME}_kernels_cuda SHARED ${CUDA_SOURCE_FILES})
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_kernels_cuda)
    install(TARGETS ${XKBLAS_NAME}_kernels_cuda LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKBLAS_NAME} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

if (STRICT)
    SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${XKBLAS_NAME} PRIVATE ${STRICT_OPTIONS})
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
