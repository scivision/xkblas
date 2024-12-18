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
    ${CMAKE_SOURCE_DIR}/src/context.cc
    ${CMAKE_SOURCE_DIR}/src/legacy.cc
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
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/ xkblas-version
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

# ptr lib deps
option(PTR "The path to the sync library" OFF)
if (PTR)
    message(STATUS "Using PTR at '${PTR}'")
    include_directories(${PTR}/include)
else()
    message(FATAL_ERROR "Please set '-DPTR=path/to/ptr'")
endif()
include_directories("${PTR}/include")
message(STATUS "Found PTR: ${PTR}")

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
# XKBLAS #
##########

# XKBLAS Lib
add_library(${XKBLAS_NAME} SHARED ${SOURCE_FILES})

if (${HWLOC_FOUND})
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${HWLOC_LIBRARIES})
endif()

# IF USING CUDA
if (${CUDA_RT_FOUND})

    # Cuda kernels
    set_source_files_properties(CUDA_SOURCES, PROPERTIES LANGUAGE CUDA)
    cuda_add_library(${XKBLAS_NAME}_cuda_kernels SHARED ${CUDA_SOURCE_FILES})
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_cuda_kernels)
    install(TARGETS ${XKBLAS_NAME}_cuda_kernels LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

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
    if (${CUDA_RT_FOUND})
        target_compile_options(${XKBLAS_NAME}_cuda_kernels PRIVATE ${STRICT_OPTIONS})
    endif()
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
