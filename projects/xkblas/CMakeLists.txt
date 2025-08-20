# CMake Minimum version
cmake_minimum_required(VERSION 3.7)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

###############
# C++ version #
###############
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions

# Project name
SET(XKBLAS_NAME xkblas)
project(${XKBLAS_NAME})

###############
# C++ version #
###############
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions

SET(XKBLAS_KERNELS copyscale gemm gemmt symm syr2k syrk trmm trsm)
SET(XKBLAS_KERNELS_PRECISIONS s c d z)
#SET(XKBLAS_KERNELS_PRECISIONS s d)

# custom kernels
SET(XKBLAS_CUDA_KERNELS copyscale)

# Generation of source files foreach kernels and precision
SET(XKBLAS_KERNELS_SOURCE_FILES)
SET(XKBLAS_CUDA_KERNELS_SOURCE_FILES)

# .cc source fles for each precision
foreach(precision IN LISTS XKBLAS_KERNELS_PRECISIONS)

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

    # custom cuda kernels
    foreach(kernel IN LISTS XKBLAS_CUDA_KERNELS)
        list(APPEND XKBLAS_CUDA_KERNELS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kernels/generated/${precision}${kernel}.cu)
        add_custom_command(
            OUTPUT
                ${CMAKE_SOURCE_DIR}/src/kernels/generated/${precision}${kernel}.cu
            COMMAND
                python3 ${CMAKE_SOURCE_DIR}/src/kernels/generators/generate-cu.py ${CMAKE_SOURCE_DIR} ${kernel} ${precision}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/src/kernels/${kernel}.cu
        )
    endforeach()

endforeach()

# TODO : this is always run for now, couldnt find a way to set a proper dependency
add_custom_command(
    OUTPUT
        ${CMAKE_SOURCE_DIR}/src/kernels/generated/kernel-task-format-register.cc
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/src/kernels/generators/generate-task-format.py ${CMAKE_SOURCE_DIR} '${XKBLAS_KERNELS}' '${XKBLAS_KERNELS_PRECISIONS}'
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/../utils/version.py ${CMAKE_SOURCE_DIR}/include/xkblas xkblas-version
)
list(APPEND XKBLAS_KERNELS_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/kernels/generated/kernel-task-format-register.cc)

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/auto-tile.cc
    ${CMAKE_SOURCE_DIR}/src/conf.cc
    ${CMAKE_SOURCE_DIR}/src/legacy.cc
    ${CMAKE_SOURCE_DIR}/src/memory-alloc.cc
    ${CMAKE_SOURCE_DIR}/src/memory-coherent.cc
    ${CMAKE_SOURCE_DIR}/src/memory-invalidate.cc
    ${CMAKE_SOURCE_DIR}/src/memory-preallocate.cc
    ${CMAKE_SOURCE_DIR}/src/memory-register.cc
    ${CMAKE_SOURCE_DIR}/src/memory-replicate.cc
    ${CMAKE_SOURCE_DIR}/src/xkblas.cc
    ${XKBLAS_KERNELS_SOURCE_FILES}
)

#########
# RPATH #
#########
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
include_directories("${XKRT_INCLUDE_DIRS}")

##########
# XKBLAS #
##########

# XKBLAS Lib
add_library(${XKBLAS_NAME} SHARED ${SOURCE_FILES})
target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKRT_LIBRARIES})

# Include directory
include_directories(include/)
include_directories(src/)

# Installation
install(TARGETS ${XKBLAS_NAME} LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

option(STRICT "Enable strict compilation mode" ON)
if (STRICT)
    # SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${XKBLAS_NAME} PRIVATE ${STRICT_OPTIONS})
endif()

################
# DEPENDENCIES #
################

option(USE_CUBLAS "Enable cublas" ON)
if (USE_CUBLAS)
		find_package(CUDA_DRIVER REQUIRED)
    find_package(CUBLAS REQUIRED)
    include_directories("${CUBLAS_INCLUDE_DIRS}")
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${CUBLAS_LIBRARIES})
endif()

# Cuda kernels
option(USE_CUDA "Enable the compilation of custom cuda kernels" ON)
if (USE_CUDA)
    enable_language(CUDA)
    set_source_files_properties(${XKBLAS_CUDA_KERNELS_SOURCE_FILES} PROPERTIES LANGUAGE CUDA)
    add_library(${XKBLAS_NAME}_kernels_cuda SHARED ${XKBLAS_CUDA_KERNELS_SOURCE_FILES})
    set_target_properties(${XKBLAS_NAME}_kernels_cuda PROPERTIES CUDA_ARCHITECTURES "50;72")
    install(TARGETS ${XKBLAS_NAME}_kernels_cuda LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_kernels_cuda)
endif()

option(USE_CLBLAST "Enable clblast" ON)
if(USE_CLBLAST)
    find_package(CLBLAST REQUIRED)
    add_compile_definitions(XKBLAS_SUPPORT_CLBLAST=1)
    include_directories("${CLBLAST_INCLUDE_DIRS}")
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${CLBLAST_LIBRARIES})
endif()

option(USE_ZE "Use Level Zero" OFF)
if (USE_ZE)
    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")
endif()

option(USE_SYCL "Use Sycl" OFF)
if (USE_SYCL)
    target_link_libraries( ${XKBLAS_NAME} PRIVATE ${SYCL_LIBRARIES} -fsycl)
    target_compile_options(${XKBLAS_NAME} PRIVATE -fsycl)
endif()

################
# Sub projects #
################
add_subdirectory(tests/)
