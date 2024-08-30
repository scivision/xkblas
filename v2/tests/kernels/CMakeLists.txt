# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(xkblas-kernels)
include_directories(.)

# option(BLA_STATIC "Enable static linkage with BLAS" ON)
find_package(BLAS REQUIRED)
if(BLAS_FOUND)
    message(STATUS "BLAS found - ${BLAS_LIBRARIES}")
    add_executable(xkblas-kernels common/main.cc)
    target_link_libraries(xkblas-kernels xkblas ${BLAS_LIBRARIES})
else()
    message(WARNING "BLAS not found")
endif(BLAS_FOUND)
