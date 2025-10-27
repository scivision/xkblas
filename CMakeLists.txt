# CMake Minimum version
cmake_minimum_required(VERSION 3.17)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

###########
# OPTIONS #
###########

include(BooleanUtils)

xkoption(STRICT       "Enable strict compilation mode" OFF)
xkoption(USE_TESTS    "Compile tests (require a CBLAS installation)" OFF)
xkoption(USE_CBLAS    "Enable the use of CPU Blas library. See https://cmake.org/cmake/help/latest/module/FindBLAS.html" OFF)
xkoption(USE_CLBLAST  "Enable clblast" OFF)
xkoption(USE_CUBLAS   "Enable cublas" OFF)
xkoption(USE_CUSPARSE "Enable cusparse" OFF)
xkoption(USE_CUDA     "Enable the compilation of custom cuda kernels" OFF)
xkoption(USE_HIP      "Enable hip" OFF)
xkoption(USE_HIPBLAS  "Enable hipblas" OFF)
xkoption(USE_SYCL     "Use Sycl" OFF)
xkoption(USE_ZE       "Use Level Zero" OFF)

###############
# C++ version #
###############
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions
add_compile_options(-Wall -Wextra -Wreturn-type)

# Project name
SET(XKBLAS_NAME xkblas)
project(${XKBLAS_NAME})

###############
# C++ version #
###############
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Optional: disables compiler-specific extensions

SET(XKBLAS_ROUTINES

    # level 1
    axpby axpy copy divcopy dot fill nrm2 scalcopy scal

    # level 2
    copyscale gemv

    # level 3
    gemm gemmt herk symm syr2k syrk trmm trsm

    # lapacke
    potrf

    # sparse
    spmv
)
SET(XKBLAS_ROUTINES_PRECISIONS s c d z)

# custom kernels
SET(XKBLAS_CUDA_KERNELS copyscale)
SET(XKBLAS_HIP_KERNELS  copyscale)

# Generated C source files for bindings
SET(XKBLAS_ROUTINES_SOURCE_FILES)
SET(XKBLAS_ROUTINES_C_SOURCE_FILES)
SET(XKBLAS_CUDA_KERNELS_SOURCE_FILES)
SET(XKBLAS_HIP_KERNELS_SOURCE_FILES)

# ensure C api directory exists
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/api/c/include/xkblas)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/api/c/src/routines)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src/kernels)
configure_file(${CMAKE_SOURCE_DIR}/api/c/include/xkblas/xkblas.h ${CMAKE_BINARY_DIR}/api/c/include/xkblas/xkblas.h COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/api/c/include/xkblas/routines.h ${CMAKE_BINARY_DIR}/api/c/include/xkblas/routines.h COPYONLY)
add_custom_target(link_target ALL COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/api/c/include/xkblas/xkblas.h ${CMAKE_BINARY_DIR}/api/c/include/xkblas.h)

# for each precision
foreach(precision IN LISTS XKBLAS_ROUTINES_PRECISIONS)

    # for each routine
    foreach(routine IN LISTS XKBLAS_ROUTINES)

        # skip single and double precision for hermitian operators
        if((precision STREQUAL "s" OR precision STREQUAL "d") AND routine MATCHES "^h.*")
            continue()
        endif()

        # generate c bindings
        list(APPEND XKBLAS_ROUTINES_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/routines/${routine}.cc)
        list(APPEND XKBLAS_ROUTINES_C_SOURCE_FILES ${CMAKE_BINARY_DIR}/api/c/src/routines/${precision}${routine}.cc)
        add_custom_command(
            OUTPUT
                ${CMAKE_BINARY_DIR}/api/c/src/routines/${precision}${routine}.cc
            COMMAND
                python3 ${CMAKE_SOURCE_DIR}/generators/generate-cc.py ${CMAKE_SOURCE_DIR}/api/c/src/routines ${CMAKE_BINARY_DIR}/api/c/src/routines ${routine} ${precision}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/api/c/src/routines/${routine}.cc
        )
    endforeach()

    # header files for each precision
    list(APPEND XKBLAS_ROUTINES_C_SOURCE_FILES ${CMAKE_BINARY_DIR}/api/c/include/xkblas/${precision}routines.h)
    add_custom_command(
        OUTPUT
            ${CMAKE_BINARY_DIR}/api/c/include/xkblas/${precision}routines.h
        COMMAND
            python3 ${CMAKE_SOURCE_DIR}/generators/generate-h.py ${CMAKE_SOURCE_DIR}/api/c/include/xkblas/xroutines.h ${CMAKE_BINARY_DIR}/api/c/include/xkblas/${precision}routines.h ${precision}
        DEPENDS
            ${CMAKE_SOURCE_DIR}/api/c/include/xkblas/xroutines.h
    )

    # custom cuda kernels
    foreach(kernel IN LISTS XKBLAS_CUDA_KERNELS)
        list(APPEND XKBLAS_CUDA_KERNELS_SOURCE_FILES ${CMAKE_BINARY_DIR}/src/kernels/${precision}${kernel}.cu)
        add_custom_command(
            OUTPUT
                ${CMAKE_BINARY_DIR}/src/kernels/${precision}${kernel}.cu
            COMMAND
                python3 ${CMAKE_SOURCE_DIR}/generators/generate-cu.py ${CMAKE_SOURCE_DIR}/src/kernels ${CMAKE_BINARY_DIR}/src/kernels ${kernel} ${precision}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/src/kernels/${kernel}.cu
        )
    endforeach()

    # custom hip kernels
    foreach(kernel IN LISTS XKBLAS_HIP_KERNELS)
        list(APPEND XKBLAS_HIP_KERNELS_SOURCE_FILES ${CMAKE_BINARY_DIR}/src/kernels/${precision}${kernel}.hip.cpp)
        add_custom_command(
            OUTPUT
                ${CMAKE_BINARY_DIR}/src/kernels/${precision}${kernel}.hip.cpp
            COMMAND
                python3 ${CMAKE_SOURCE_DIR}/generators/generate-hip.py ${CMAKE_SOURCE_DIR}/src/kernels ${CMAKE_BINARY_DIR}/src/kernels ${kernel} ${precision}
            DEPENDS
                ${CMAKE_SOURCE_DIR}/src/kernels/${kernel}.hip.cpp
        )
    endforeach()


endforeach()

SET(SOURCE_FILES
    # XKBlas core
    ${CMAKE_SOURCE_DIR}/src/auto-tile.cc
    ${CMAKE_SOURCE_DIR}/src/conf.cc
    ${CMAKE_SOURCE_DIR}/src/xkblas.cc
    ${XKBLAS_ROUTINES_SOURCE_FILES}
)

# C bindings to its own custom lib that link with xkblas-core
SET(C_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/api/c/src/host-async.cc
    ${CMAKE_SOURCE_DIR}/api/c/src/legacy.cc
    ${CMAKE_SOURCE_DIR}/api/c/src/memory/alloc.cc
    ${CMAKE_SOURCE_DIR}/api/c/src/memory/coherent.cc
    ${CMAKE_SOURCE_DIR}/api/c/src/memory/invalidate.cc
    ${CMAKE_SOURCE_DIR}/api/c/src/memory/preallocate.cc
    ${CMAKE_SOURCE_DIR}/api/c/src/memory/register.cc
    ${CMAKE_SOURCE_DIR}/api/c/src/memory/replicate.cc
    ${XKBLAS_ROUTINES_C_SOURCE_FILES}
)

#########
# RPATH #
#########
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

##########
# XKBLAS #
##########

# XKBLAS Lib
add_library(${XKBLAS_NAME}       SHARED ${SOURCE_FILES} ${C_SOURCE_FILES})
add_library(${XKBLAS_NAME}_cblas SHARED ${CMAKE_SOURCE_DIR}/api/cblas/wrappers.cc) # cblas wrappers

# Include directory
include_directories(${CMAKE_SOURCE_DIR}/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/)
include_directories(${CMAKE_BINARY_DIR}/include/)
include_directories(${CMAKE_BINARY_DIR}/api/c/include/)

# Support header file generated
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/xkblas")
configure_file(
    ${CMAKE_SOURCE_DIR}/include/xkblas/support.h.in
    ${CMAKE_BINARY_DIR}/include/xkblas/support.h
)

################
# INSTALLATION #
################

install(
    FILES "${CMAKE_BINARY_DIR}/include/xkblas/support.h"
    DESTINATION include/xkblas
)

install(TARGETS ${XKBLAS_NAME}       LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(TARGETS ${XKBLAS_NAME}_cblas LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")
install(DIRECTORY ${CMAKE_BINARY_DIR}/api/c/include/ DESTINATION include FILES_MATCHING PATTERN "*.h*")

if (STRICT)
    # SET(STRICT_OPTIONS -Wfatal-errors -Werror=conversion -ftrapv -Wno-sign-conversion)
    target_compile_options(${XKBLAS_NAME}       PRIVATE ${STRICT_OPTIONS})
    target_compile_options(${XKBLAS_NAME}_cblas PRIVATE ${STRICT_OPTIONS})
endif()

###############
# DEPENDENCES #
###############

# XK Runtime
find_package(XKRT REQUIRED)
target_link_libraries(${XKBLAS_NAME}       PRIVATE XKRT::xkrt)
target_link_libraries(${XKBLAS_NAME}_cblas PRIVATE XKRT::xkrt ${XKBLAS_NAME})

################
# DEPENDENCIES #
################

if (USE_CUBLAS)
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(${XKBLAS_NAME} PRIVATE CUDA::cudart CUDA::cuda_driver CUDA::cublas)
endif()

if (USE_CUSPARSE)
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(${XKBLAS_NAME} PRIVATE CUDA::cudart CUDA::cuda_driver CUDA::cusparse)
endif()

# Cuda custom kernels
if (USE_CUDA)
    enable_language(CUDA)
    set_source_files_properties(${XKBLAS_CUDA_KERNELS_SOURCE_FILES} PROPERTIES LANGUAGE CUDA)
    add_library(${XKBLAS_NAME}_kernels_cuda SHARED ${XKBLAS_CUDA_KERNELS_SOURCE_FILES})
    set_target_properties(${XKBLAS_NAME}_kernels_cuda PROPERTIES CUDA_ARCHITECTURES "50;72")
    install(TARGETS ${XKBLAS_NAME}_kernels_cuda LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_kernels_cuda)
endif()

if (USE_HIPBLAS)
    find_package(HIP REQUIRED)
    find_package(HIPBLAS REQUIRED)
    include_directories("${HIP_INCLUDE_DIRS}")
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${HIP_LIBRARIES} ${HIPBLAS_LIBRARIES})
endif()

# HIP custom kernels
if (USE_HIP)
    enable_language(HIP)
    set_source_files_properties(${XKBLAS_HIP_KERNELS_SOURCE_FILES} PROPERTIES LANGUAGE HIP)
    add_library(${XKBLAS_NAME}_kernels_hip SHARED ${XKBLAS_HIP_KERNELS_SOURCE_FILES})
    # set_target_properties(${XKBLAS_NAME}_kernels_cuda PROPERTIES CUDA_ARCHITECTURES "50;72")
    install(TARGETS ${XKBLAS_NAME}_kernels_hip LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${XKBLAS_NAME}_kernels_hip)
endif()

if (USE_CLBLAST)
    find_package(CLBLAST REQUIRED)
    add_compile_definitions(XKBLAS_SUPPORT_CLBLAST=1)
    include_directories("${CLBLAST_INCLUDE_DIRS}")
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${CLBLAST_LIBRARIES})
endif()

if (USE_ZE)
    find_package(ZE REQUIRED)
    include_directories("${ZE_INCLUDE_DIRS}")
endif()

if (USE_SYCL)
    target_link_libraries( ${XKBLAS_NAME} PRIVATE ${SYCL_LIBRARIES} -fsycl)
    target_compile_options(${XKBLAS_NAME} PRIVATE -fsycl)
endif()

if (USE_CBLAS)
    if (USE_MKL)
        message(STATUS "MKLROOT=$ENV{MKLROOT}")
        if(DEFINED ENV{MKLROOT})
            message(STATUS "MKLROOT=[$ENV{MKLROOT}]")
        else()
            message(FATAL_ERROR "`USE_MKL` is set but `MKLROOT` is unset")
        endif()
        set(BLA_VENDOR Intel10_64lp_seq)
        add_compile_options("-DUSE_MKL=1")
        add_compile_options("-DMKL_ILP64")
        add_compile_options("-m64")
    endif()

    if (USE_CRAYBLAS)
        set(BLA_VENDOR Generic)
        add_compile_options("-DUSE_CRAYBLAS=1")
    endif()

    if (USE_OPENBLAS)
        set(BLA_VENDOR OpenBLAS)
        add_compile_options("-DUSE_OPENBLAS=1")
    endif()

    if (USE_NVHPC)
        set(BLA_VENDOR NVHPC)
        add_compile_options("-DUSE_NVHPC=1")
    endif()

    if (NOT USE_MKL AND NOT USE_OPENBLAS AND NOT USE_CRAYBLAS)
        message(FATAL_ERROR "XKblas tests requires you specify a BLAS Vendor library.")
    endif()

    find_package(BLAS)
    if (${BLAS_FOUND})
        message(STATUS "Found BLAS library.")
    else()
        add_compile_options("-DNO_CPU_BLAS=1")
        message(STATUS "Testing correctness requires a BLAS library, and none were found.")
        set(BLAS_INCLUDE_DIRS "")
        set(BLAS_LIBRARIES_DIRS "")
    endif()

    if (USE_MKL)
        find_path(BLAS_INCLUDE_DIRS mkl_lapacke.h $ENV{MKLROOT}/include)
    endif()

    if (USE_CRAYBLAS)
        find_path(BLAS_INCLUDE_DIRS lapacke.h ${CRAY_LIBSCI_PREFIX_DIR}/include)
    endif()

    if (USE_OPENBLAS)
        find_path(BLAS_INCLUDE_DIRS lapacke.h)
    endif()

    include_directories(${BLAS_INCLUDE_DIRS})
    target_link_libraries(${XKBLAS_NAME} PRIVATE ${BLAS_LIBRARIES})

endif()

################
# Sub projects #
################
if (USE_TESTS)
    add_subdirectory(tests/)
endif()
