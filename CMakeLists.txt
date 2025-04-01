cmake_minimum_required (VERSION 3.7)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-DKAAPI_DEBUG=1 ")
set(CMAKE_CC_FLAGS_DEBUG_INIT "-DKAAPI_DEBUG=1 ")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DKAAPI_DEBUG=1 -Wfatal-errors ")

#set(CMAKE_CXX_FLAGS_DEBUG_INIT "-DKAAPI_DEBUG=1 -D_OFFLOAD_DEBUG=1")
#set(CMAKE_CC_FLAGS_DEBUG_INIT "-DKAAPI_DEBUG=1 -D_OFFLOAD_DEBUG=1")
#set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DKAAPI_DEBUG=1 -D_OFFLOAD_DEBUG=1")

#
project (XKblas)

# More detailled information to know what we are doing
message( STATUS "C Compiler:   ${CMAKE_C_COMPILER}" )
message( STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}" )


# Global options
option(KAAPI_USE_CUDA_RT "To set if Kaapi should be compiled with support for NVIDIA GPU using Runtime API." OFF)
option(KAAPI_USE_HIP "To set if Kaapi should be compiled with support for AMD GPU using HIP API." OFF)

option(KAAPI_USE_TRACE "Use tracing facility for post mortem analysis (gantt, dot graph, ...)" OFF)
option(KAAPI_USE_PERFCTR "Add support for performance counter in the trace of execution. If PAPI is found then hardware performance counters are also available." OFF)

option(KAAPI_USE_MKL "Use MKL" OFF)
option(KAAPI_USE_OPENBLAS "Use OpenBLAS" OFF)
option(KAAPI_USE_CRAYBLAS "Use Cray LIBSCI" OFF)
option(KAAPI_USE_NVPL "Use Nvidia NVPL from NVHPC" OFF)
option(KAAPI_USE_AOCL "Use AMD AOCL as BLAS" OFF)

option(KAAPI_USE_SLEEP "Sleep thread is nothing to do" ON)
option(KAAPI_BUILD_TESTING "Compile /testing files" OFF)

option(KAAPI_USE_HWLOC "To set if Kaapi should be compiled with support for HWLOC." ON)

option(XKBLAS_BUILD_COPYSCALE "Build the copyscale function" OFF)
option(ENABLE_KAAPI_UNIFIED "Enable KAAPI_UNIFIED_MACRO" OFF)
option(ENABLE_KAAPI_NVTX "Enable NVTX profiling for the CPU threads" OFF)

#--
if(ENABLE_KAAPI_UNIFIED)
  add_definitions(-DKAAPI_UNIFIED)
  #add_definitions(-DKAAPI_UNIFIED_PARTIAL)
endif()

if(ENABLE_KAAPI_NVTX)
  add_definitions(-DKAAPI_NVTX)
endif()

#--
if (KAAPI_USE_HIP AND KAAPI_USE_CUDA_RT)
  message(FATAL_ERROR "Current implementation of Kaapi does not support HIP pluging and CUDA plugin compiled together.")
endif()

#--- OpenMP
find_package(OpenMP)
if (OPENMP_FOUND)
  #SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  #SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

#--- CUDA
if (KAAPI_USE_CUDA_RT)
  find_package(CUDA REQUIRED) # TODO deal with HIP/NO GPU
  find_package(CUDA_RT)
  if (NOT ${CUDA_RT_FOUND})
    message(FATAL_ERROR "Kaapi is configured with CUDA Runtime API but library could not be found.")
  endif()
  find_package(CUBLAS)
  if (NOT ${CUBLAS_FOUND})
    message(FATAL_ERROR "Kaapi is configured with CUDA API but CUBLAS is not found for XKblas !")
  endif()
#  find_path(BLAS_INCLUDE_DIRS cblas.h
#    /usr/include
#    /usr/local/include
#    $ENV{BLAS_HOME}/include)
endif()

#Hip
if (KAAPI_USE_HIP)
  # Cannot use find_package HIP that imply unknown package on neowise (or I do not read all the documentation)
  #
  find_package(HIP)
  if (NOT ${HIP_FOUND})
    message(FATAL_ERROR "Kaapi is configured with HIP, but hip installation could not be found!")
  endif()

  find_package(ROCBLAS)
  if (NOT ${ROCBLAS_FOUND})
    message(FATAL_ERROR "Kaapi is configured with HIP, but rocblas could not be found!")
  endif()
  find_package(HIPBLAS)
  if (NOT ${HIPBLAS_FOUND})
    message(FATAL_ERROR "Kaapi is configured with HIP, but hipblas could not be found!")
  endif()

  message(STATUS "Found HIP_INCLUDE_DIRS: ${HIP_INCLUDE_DIRS}")
  message(STATUS "Found HIP_LIBRARIES: ${HIP_LIBRARIES}")
  message(STATUS "Found HIPBLAS_INCLUDE_DIRS: ${HIPBLAS_INCLUDE_DIRS}")
  message(STATUS "Found HIPBLAS_LIBRARIES: ${HIPBLAS_LIBRARIES}")
  message(STATUS "Found ROCBLAS_INCLUDE_DIRS: ${ROCBLAS_INCLUDE_DIRS}")
  message(STATUS "Found ROCBLAS_LIBRARIES: ${ROCBLAS_LIBRARIES}")
else()
  set(HIP_FOUND FALSE)
endif()

#--- PAPI
if (KAAPI_USE_TRACE AND KAAPI_USE_PERFCTR)
  find_package(PAPI)
endif()

#--- HWLOC
if (KAAPI_USE_HWLOC)
  find_package(HWLOC)
endif()

if (${HIP_FOUND})
  find_path(ROCSMI_INCLUDE_DIRS
    NAMES rocm_smi/rocm_smi.h
    HINTS ${ROCM_PATH}/include
    HINTS ${ROCSMI_PREFIX}/include  
  )
  find_library(ROCSMI_LIBRARIES
    # Pick the static library first for easier run-time linking.
    NAMES librocm_smi64.so 
    HINTS ${ROCM_PATH}/lib
    HINTS ${ROCSMI_PREFIX}/lib  
  )
  if (ROCSMI_INCLUDE_DIRS STREQUAL "ROCSMI_INCLUDE_DIRS-NOTFOUND")
    message(STATUS "ROCSMI_INCLUDE_DIRS not defined:" ${ROCSMI_INCLUDE_DIRS})
    set(KAAPI_USE_ROCSMI FALSE)
    set(KAAPI_USE_HWLOCROCSMI FALSE)
  else()
    message(STATUS "ROCSMI_INCLUDE_DIRS defined:" ${ROCSMI_INCLUDE_DIRS})
    set(KAAPI_USE_ROCSMI TRUE)
    if (HWLOC_FOUND)
      find_path(HWLOC_RSMI_INCLUDE_DIRS
        NAMES hwloc/rsmi.h
        HINTS ${ROCSMI_PREFIX}/include
      )
      if (HWLOC_RSMI_INCLUDE_DIRS  STREQUAL "HWLOC_RSMI_INCLUDE_DIRS-NOTFOUND")
	set(KAAPI_USE_HWLOCROCSMI FALSE)
      else()
	set(KAAPI_USE_HWLOCROCSMI TRUE)
      endif()
    else()
      set(KAAPI_USE_HWLOCROCSMI FALSE)
    endif()
  endif()
  if (ROCSMI_LIBRARIES STREQUAL "ROCSMI_LIBRARIES-NOTFOUND")
     message(STATUS "ROCSMI_LIBRARIES not defined:" ${ROCSMI_LIBRARIES})
     set(KAAPI_USE_ROCSMI FALSE)
     set(KAAPI_USE_HWLOCROCSMI FALSE)
  else()
     message(STATUS "ROCSMI_LIBRARIES defined:" ${ROCSMI_LIBRARIES})
  endif()
endif()


#--- LIBNUMA
find_path(LIBNUMA_INCLUDE_DIRS
  NAMES numa.h
  HINTS /usr/include
)
find_library(LIBNUMA_LIBRARIES
  NAMES libnuma.so
  HINTS /usr/lib64
  HINTS /usr/lib
)
if (LIBNUMA_INCLUDE_DIRS STREQUAL "LIBNUMA_INCLUDE_DIRS-NOTFOUND")
  set(KAAPI_USE_LIBNUMA FALSE)
else()
  if (LIBNUMA_LIBRARIES STREQUAL "LIBNUMA_LIBRARIES-NOTFOUND")
    set(KAAPI_USE_LIBNUMA FALSE)
  else()
    set(KAAPI_USE_LIBNUMA TRUE)
  endif()
endif()


#--- BLAS
if (KAAPI_USE_MKL)
  set(BLA_VENDOR Intel10_64lp_seq)
endif()
if (KAAPI_USE_CRAYBLAS)
  set(BLA_VENDOR Generic)
endif()
if (KAAPI_USE_OPENBLAS)
  set(BLA_VENDOR OpenBLAS)
endif()
#if (KAAPI_USE_NVPL)
#  set(BLA_VENDOR NVHPC)
#endif()
if (KAAPI_USE_OPENBLAS)
  set(BLA_VENDOR AOCL)
endif()
if (NOT KAAPI_USE_MKL AND NOT KAAPI_USE_OPENBLAS AND NOT KAAPI_USE_CRAYBLAS AND NOT KAAPI_USE_NVPL AND NOT KAAPI_USE_AOCL)
  message(FATAL_ERROR "XKblas requires you specify the BLAS Vendor library.")
endif()

# Find BLAS Package
if (KAAPI_USE_NVPL)
  # NVPL is not supported by the default CMakelist.txt
  message(STATUS "Searching NVPL as BLAS library")
  find_library(BLAS_LIB libnvpl_blas_core.so)
  find_path(BLAS_INCLUDE_DIR nvpl_blas_cblas.h include/)
  if(BLAS_LIB AND BLAS_INCLUDE_DIR)
    set(BLAS_FOUND TRUE)
    set(BLAS_LIBRARIES ${BLAS_LIB})
    set(BLAS_INCLUDE_DIRS ${BLAS_INCLUDE_DIR})
  else()
    message(FATAL_ERROR "XKblas failed to find NVPL, check CMAKE_PREFIX_PATH ?")
  endif()
else()
  find_package(BLAS REQUIRED)
  if (KAAPI_USE_MKL)
    find_path(BLAS_INCLUDE_DIRS mkl_lapacke.h $ENV{MKLROOT}/include)
  endif()
  if (KAAPI_USE_CRAYBLAS)
    find_path(BLAS_INCLUDE_DIRS lapacke.h ${CRAY_LIBSCI_PREFIX_DIR}/include)
  endif()
  if (KAAPI_USE_AOCL)
    find_path(BLAS_INCLUDE_DIRS lapacke.h)
  endif()
endif()
if (NOT ${BLAS_FOUND})
  message(FATAL_ERROR "XKblas requires a BLAS library.")
endif()
message("BLAS_INCLUDE_DIRS : ${BLAS_INCLUDE_DIRS}")
message("BLAS_LIBRARIES    : ${BLAS_LIBRARIES}")

add_compile_options(${BLAS_LINKER_FLAGS})

#--- Add common command line options for all subdirectories
if (KAAPI_USE_CUDA_RT)
  add_compile_options("-DKAAPI_USE_CUDA=1")
  add_compile_options("-DKAAPI_USE_CUDA_RUNTIME_API=1")
  message(STATUS "Configuring Kaapi with CUDA runtime plugin")
endif()
if (KAAPI_USE_HIP)
  add_compile_options("-DKAAPI_USE_HIP=1")
  add_compile_options("-D__HIP_PLATFORM_AMD__=1")
  add_compile_options("-DROCBLAS_V3") 
  message(STATUS "Configuring Kaapi with HIP plugin")
endif()

if (KAAPI_USE_TRACE)
  add_compile_options("-DKAAPI_USE_TRACELIB=1")
  message(STATUS "Configuring Kaapi with tracing support")
else()
  add_compile_options("-DKAAPI_USE_TRACELIB=0")
endif()

if (KAAPI_USE_PERFCTR)
  add_compile_options("-DKAAPI_USE_PERFCOUNTER=1")
  message(STATUS "Configuring Kaapi with performance counter support")
else()
   add_compile_options("-DKAAPI_USE_PERFCOUNTER=0")
endif()

if (KAAPI_USE_TRACE AND KAAPI_USE_PERFCTR)
  if (${PAPI_FOUND})
    add_compile_options("-DKAAPI_USE_PAPI=1")
  else()
    add_compile_options("-DKAAPI_USE_PAPI=0")
  endif()
endif()

if (KAAPI_USE_SLEEP)
  add_compile_options("-DDKAAPI_USE_SLEEP=1")
else()
  add_compile_options("-DDKAAPI_USE_SLEEP=0")
endif()

if (KAAPI_USE_HWLOC)
  if (${HWLOC_FOUND})
    add_compile_options("-DKAAPI_USE_HWLOC=1")
  else()
    add_compile_options("-DKAAPI_USE_HWLOC=0")
  endif()
else()
  add_compile_options("-DKAAPI_USE_HWLOC=0")
endif()

if (KAAPI_USE_MKL)
  add_compile_options("-DKAAPI_BLAS_USE_MKL=1")
  add_compile_options("-DMKL_ILP64")
  add_compile_options("-m64")
  add_compile_options("-DXKBLAS_BLASLIB=\"libmkl_intel_ilp64.so\"")
endif()
if (KAAPI_USE_CRAYBLAS)
  add_compile_options("-DKAAPI_BLAS_USE_CRAYBLAS=1")
  add_compile_options("-DXKBLAS_BLASLIB=\"libsci_cray.so\"")
endif()
if (KAAPI_USE_OPENBLAS)
  add_compile_options("-DKAAPI_BLAS_USE_OPENBLAS=1")
  add_compile_options("-DXKBLAS_BLASLIB=\"libopenblas.so\"")
endif()
if (KAAPI_USE_AOCL)
  add_compile_options("-DKAAPI_BLAS_USE_AOCL=1")
  add_compile_options("-DXKBLAS_BLASLIB=\"libblis.so\"")
endif()
if (KAAPI_USE_NVPL)
  add_compile_options("-DKAAPI_BLAS_USE_NVPL=1")
  add_compile_options("-DXKBLAS_BLASLIB=\"libnvpl_blas_ilp64.so\"")
endif()
#find_package(BLAS REQUIRED)
#if (NOT ${BLAS_FOUND})
#  message(FATAL_ERROR "XKblas requires a BLAS library.")
#endif()

#--- Subdirectories
include_directories ("kaapi")

add_subdirectory(kaapi)
add_subdirectory(bin)
add_subdirectory(blas)
if (KAAPI_BUILD_TESTING)
  add_subdirectory(testing)
endif()

