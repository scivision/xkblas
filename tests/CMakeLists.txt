# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(tests)

set(CMAKE_BUILD_TYPE Debug)

add_executable(tests tests.c)

target_link_libraries(tests PRIVATE ${XKBLAS_NAME} XKRT::xkrt)

if (USE_CUDA)
    target_link_libraries(tests PRIVATE ${XKBLAS_NAME}_kernels_cuda ${CUDA_DRIVER_LIBRARIES})
endif()

if (USE_SYCL)
    target_link_libraries(tests PRIVATE sycl -fsycl -qmkl=parallel)
endif()

add_subdirectory(kernels/)
