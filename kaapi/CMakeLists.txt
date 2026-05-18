add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/git_hash.h
  COMMAND ${XKblas_SOURCE_DIR}/config/git_hash.sh ${XKblas_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/git_hash.h
  )

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hw_count.h
  COMMAND ${XKblas_SOURCE_DIR}/config/hw_count.sh ${CMAKE_CURRENT_BINARY_DIR}/hw_count.h
  )

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

find_package(Threads REQUIRED)

if (HWLOC_FOUND)
  add_compile_definitions(KAAPI_USE_HWLOC=1)
  message(STATUS "Kaapi configured with HWLOC")
endif()

if (KAAPI_USE_HIP)
  message(STATUS "Kaapi configured with HIP")
endif()

if (KAAPI_USE_ROCSMI)
  message(STATUS "Kaapi configured with ROCMSMI")
endif()

if (PAPI_FOUND)
  add_compile_definitions(KAAPI_USE_PAPI=1)
  message(STATUS "Kaapi configured with PAPI")
else()
  add_compile_definitions(KAAPI_USE_PAPI=0)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})


if (KAAPI_USE_CUDA_DRIVER)
  list(APPEND ADDITIONAL_SOURCES kaapi_plugin_cuda.c)
endif()
if (KAAPI_USE_CUDA_RT)
  list(APPEND ADDITIONAL_SOURCES kaapi_plugin_cuda.c)
endif()
if (KAAPI_USE_HIP)
  list(APPEND ADDITIONAL_SOURCES kaapi_plugin_hip.c)
endif()

if (KAAPI_USE_TRACE)
  list(APPEND ADDITIONAL_SOURCES kaapi_trace.h kaapi_perfctr.h kaapi_trace_lib.c kaapi_trace_parser.c kaapi_trace_recorder.c)
else()
endif()

if (KAAPI_USE_PERFCTR)
  list(APPEND ADDITIONAL_SOURCES kaapi_trace.h kaapi_perfctr.h kaapi_trace_lib.c kaapi_trace_recorder.c kaapi_trace_parser.c)
else()
endif()

#if (KAAPI_USE_TRACE AND KAAPI_USE_PERFCTR)
#  if (${PAPI_FOUND})
#  else()
#  endif()
#endif()
#
#if (KAAPI_USE_SLEEP)
#else()
#endif()
#

if (KAAPI_USE_ROCSMI)
  add_compile_definitions(KAAPI_USE_ROCSMI=1)
  message(STATUS "Kaapi configured with ROCSMI")
else()
  add_compile_definitions(KAAPI_USE_ROCSMI=0)
endif()

if (KAAPI_USE_HWLOCROCSMI)
  add_compile_definitions(KAAPI_USE_HWLOCROCSMI=1)
else()
  add_compile_definitions(KAAPI_USE_HWLOCROCSMI=0)
endif()

if (KAAPI_USE_LIBNUMA)
  add_compile_definitions(KAAPI_USE_LIBNUMA=1)
  message(STATUS "Kaapi configured with LIBNUMA")
else()
  add_compile_definitions(KAAPI_USE_LIBNUMA=0)
endif()

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_library(kaapi SHARED  ${CMAKE_CURRENT_BINARY_DIR}/git_hash.h kaapi.h kaapi_error.h kaapi_atomic.h kaapi_format.c kaapi_impl.c kaapi_task.c kaapi_rt.c kaapi_hashmap.c kaapi_barrier.c kaapi_memory.c kaapi_offload_stream.c kaapi_offload.c kaapi_offload_device.c kaapi_ld.c kaapi_dbg.c kaapi_plugin_host.c ${ADDITIONAL_SOURCES})
target_link_libraries(kaapi PRIVATE Threads::Threads)

if (KAAPI_USE_HWLOC)
  if (HWLOC_FOUND)
    include_directories(${HWLOC_INCLUDE_DIRS})
    message(STATUS "Found HWLOC_INCLUDE_DIRS: ${HWLOC_INCLUDE_DIRS}")
    target_link_libraries(kaapi PRIVATE ${HWLOC_LIBRARIES})
  endif()
else()
  message(STATUS "XKblas is configured without support for HWLOC")
endif()

if (KAAPI_USE_ROCSMI)
  include_directories(${ROCSMI_INCLUDE_DIRS})
  target_link_libraries(kaapi PRIVATE ${ROCSMI_LIBRARIES})
  message(STATUS "Found ROCSMI_LIBRARIES: ${ROCSMI_LIBRARIES}")
endif()

if (KAAPI_USE_LIBNUMA)
  target_include_directories(kaapi PRIVATE ${LIBNUMA_INCLUDE_DIRS})
  target_link_libraries(kaapi PRIVATE ${LIBNUMA_LIBRARIES})
  message(STATUS "Kaapi configured with LIBNUMA")
endif()

if (KAAPI_USE_CUDA_RT)
  target_link_libraries(kaapi PRIVATE CUDA::cuda_driver CUDA::cudart CUDA::cublas)
endif()

if (KAAPI_USE_CUDA_RT)
  target_link_libraries(kaapi PRIVATE CUDA::cudart CUDA::cublas)
endif()

if (KAAPI_USE_HIP)
	target_include_directories(kaapi PRIVATE ${HIP_INCLUDE_DIRS} ${HIPBLAS_INCLUDE_DIRS} ${ROCBLAS_INCLUDE_DIRS})
	#include_directories(${HIPBLAS_INCLUDE_DIRS} ${ROCBLAS_INCLUDE_DIRS})
  target_link_libraries(kaapi PRIVATE ${HIP_LIBRARIES} ${HIPBLAS_LIBRARIES})
endif()

if (ENABLE_KAAPI_UNIFIED)
  message(STATUS "Kaapi configured with support for UVM")
endif()

if (PAPI_FOUND)
  include_directories(${PAPI_INCLUDE_DIRS})
  target_link_libraries(kaapi PRIVATE ${PAPI_LIBRARIES})
endif()

install(TARGETS kaapi DESTINATION lib)
install(FILES kaapi.h kaapi_atomic.h kaapi_error.h DESTINATION include)
