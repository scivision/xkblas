# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Project name
SET(XKBLAS_NAME xkblas)
project(${XKBLAS_NAME})

message(STATUS "TODO: automate kernel files in the build")

SET(XKBLAS_KERNELS copyscale gemm gemmt symm syr2k syrk trmm trsm)
SET(XKBLAS_KERNELS_PRECISIONS s c d z)
#SET(XKBLAS_KERNELS_PRECISIONS s)

# Generation of source files foreach kernels and precision
SET(XKBLAS_KERNELS_SOURCE_FILES)
foreach(precision IN LISTS XKBLAS_KERNELS_PRECISIONS)

    # .cc source fles for each precision
    foreach(kernel IN LISTS XKBLAS_KERNELS)
        list(APPEND XKBLAS_KERNELS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kernels/generated/${precision}${kernel}.cc)
        add_custom_command(
            OUTPUT
                ${CMAKE_SOURCE_DIR}/src/kernels/generated/${precision}${kernel}.cc
            COMMAND
                python3 ${CMAKE_SOURCE_DIR}/src/kernels/generators/generate-cc.py ${CMAKE_SOURCE_DIR} ${kernel} ${precision}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/src/kernels/${kernel}.cc
        )
    endforeach()

    # header files for each precision
    list(APPEND XKBLAS_KERNELS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/include/xkblas/${precision}kernels.h)
    add_custom_command(
        OUTPUT
            ${CMAKE_SOURCE_DIR}/include/xkblas/${precision}kernels.h
        COMMAND
            python3 ${CMAKE_SOURCE_DIR}/src/kernels/generators/generate-h.py ${CMAKE_SOURCE_DIR} ${precision}
        DEPENDS
            ${CMAKE_SOURCE_DIR}/src/kernels/kernels.h
    )

endforeach()

# TODO : this is always run for now, couldnt find a way to set a proper dependency
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/src/kernels/generated/kernel-task-format-register.cc
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/src/kernels/generators/generate-task-format.py ${CMAKE_SOURCE_DIR} '${XKBLAS_KERNELS}' '${XKBLAS_KERNELS_PRECISIONS}'
    DEPENDS
        always-run
)
list(APPEND XKBLAS_KERNELS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kernels/generated/kernel-task-format-register.cc)

SET(SOURCE_FILES
    always-run
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

# XKBLAS version
add_custom_command(
    OUTPUT
        always-run
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkblas xkblas-version
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
    enable_language(CUDA)
    set_source_files_properties(${CUDA_SOURCE_FILES} PROPERTIES LANGUAGE CUDA)
    add_library(${XKBLAS_NAME}_kernels_cuda SHARED ${CUDA_SOURCE_FILES})
    set_target_properties(${XKBLAS_NAME}_kernels_cuda PROPERTIES CUDA_ARCHITECTURES "50;72")
    install(TARGETS ${XKBLAS_NAME}_kernels_cuda LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_kernels_cuda)
endif()

option(ENABLE_ZE_BLAS "Enable the compilation of Level Zero BLAS" ON)
if (ENABLE_ZE_BLAS)

    # set sources
    SET(XKBLAS_ZE_BLAS_SOURCE_FILES)
    foreach(precision IN LISTS XKBLAS_KERNELS_PRECISIONS)
        list(APPEND XKBLAS_ZE_BLAS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/ze_blas/${precision}blas.cc ${CMAKE_SOURCE_DIR}/include/xkblas/ze_blas/ze_${precision}blas.h)

        add_custom_command(
            OUTPUT
                ${CMAKE_SOURCE_DIR}/src/ze_blas/${precision}blas.cc
            COMMAND
                python3 ${CMAKE_SOURCE_DIR}/src/ze_blas/generator.py ${CMAKE_SOURCE_DIR} ${precision}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/src/ze_blas/blas.cc
        )

        add_custom_command(
            OUTPUT
                ${CMAKE_SOURCE_DIR}/include/xkblas/ze_blas/ze_${precision}blas.h
            COMMAND
                python3 ${CMAKE_SOURCE_DIR}/include/xkblas/ze_blas/generator.py ${CMAKE_SOURCE_DIR} ${precision}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/include/xkblas/ze_blas/ze_blas.template
        )
    endforeach()

    add_compile_definitions(XKBLAS_SUPPORT_ZE_BLAS=1)
    add_library(${XKBLAS_NAME}_kernels_ze SHARED ${XKBLAS_ZE_BLAS_SOURCE_FILES})
    install(TARGETS ${XKBLAS_NAME}_kernels_ze LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_kernels_ze)

    # current impl relies on sycl ... :-(
    target_compile_options(${XKBLAS_NAME}_kernels_ze PRIVATE -fsycl)

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
