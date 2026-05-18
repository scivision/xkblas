
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/../blas
${CMAKE_CURRENT_SOURCE_DIR} ${XKblas_SOURCE_DIR}/kaapi ${XKblas_SOURCE_DIR}/blas
${BLAS_INCLUDE_DIRS} ${HIP_INCLUDE_DIRS} ${HIPBLAS_INCLUDE_DIRS})

list(APPEND GENERATED_SOURCES
  stesting_gemm.c stesting_trsm.c stesting_trmm.c stesting_symm.c stesting_syrk.c stesting_syr2k.c splgsy.c stesting_auxiliary.h stesting_auxiliary.c
  dtesting_gemm.c dtesting_trsm.c dtesting_trmm.c dtesting_symm.c dtesting_syrk.c dtesting_syr2k.c dplgsy.c dtesting_auxiliary.h dtesting_auxiliary.c
  ctesting_gemm.c ctesting_trsm.c ctesting_trmm.c ctesting_hemm.c ctesting_symm.c ctesting_syrk.c ctesting_syr2k.c ctesting_herk.c ctesting_her2k.c cplghe.c cplgsy.c ctesting_auxiliary.h ctesting_auxiliary.c
)

if(XKBLAS_BUILD_COPYSCALE)
	list(APPEND GENERATED_SOURCES
  	stesting_copyscale.c dtesting_copyscale.c ctesting_copyscale.c
  	stesting_mumps_like.c dtesting_mumps_like.c ctesting_mumps_like.c
	)
endif()


foreach(file IN LISTS GENERATED_SOURCES)
  string(REGEX REPLACE "^[sdc](.*)$" "z\\1" source "${file}")
  string(REGEX REPLACE "^([sdc]).*$" "\\1" precision "${file}")
  add_custom_command(
      OUTPUT  ${file}
      COMMAND ${XKblas_SOURCE_DIR}/gen_precision_one.sh ${precision} ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${XKblas_SOURCE_DIR}/gen_precision_one.sh
  )
  #message(STATUS "Gen precision ${precision} from ${source}")
  #message(STATUS "Include dir for BLAS: ${BLAS_INCLUDE_DIRS}")
endforeach()


#--- Testing_z
set(SOURCES_TESTING_Z
  ztesting_gemm.c
  ztesting_trsm.c
  ztesting_trmm.c
  ztesting_hemm.c
  ztesting_symm.c
  ztesting_syrk.c
  ztesting_syr2k.c
  ztesting_herk.c
  ztesting_her2k.c
  zplghe.c
  zplgsy.c
  ztesting_auxiliary.h ztesting_auxiliary.c
  )

if(XKBLAS_BUILD_COPYSCALE)
				list(APPEND SOURCES_TESTING_Z ztesting_copyscale.c ztesting_mumps_like.c)
endif()

add_executable(ztesting ${SOURCES_TESTING_Z})
target_compile_options(ztesting PUBLIC -UPRECISION_d -UPRECISION_s -UPRECISION_c)
target_compile_definitions(ztesting PUBLIC PRECISION_z=1)
target_link_libraries(ztesting xkblas kaapi ${BLAS_LIBRARIES}
"$<$<BOOL:${KAAPI_USE_CUDA_RT}>:CUDA::cublas>"
"$<$<BOOL:${OpenMP_C_FOUND}>:OpenMP::OpenMP_C>"
m)
install(TARGETS ztesting RUNTIME DESTINATION testing)

#--- Testing_c
set(SOURCES_TESTING_C
  ctesting_gemm.c
  ctesting_trsm.c
  ctesting_trmm.c
  ctesting_hemm.c
  ctesting_symm.c
  ctesting_syrk.c
  ctesting_syr2k.c
  ctesting_herk.c
  ctesting_her2k.c
  cplghe.c
  cplgsy.c
  ctesting_auxiliary.h ctesting_auxiliary.c
  )

if(XKBLAS_BUILD_COPYSCALE)
				list(APPEND SOURCES_TESTING_C ctesting_copyscale.c ctesting_mumps_like.c)
endif()

add_executable(ctesting ${SOURCES_TESTING_C})
target_compile_options(ctesting PUBLIC -UPRECISION_d -UPRECISION_s -UPRECISION_z)
target_compile_definitions(ctesting PUBLIC PRECISION_c=1)
target_link_libraries(ctesting xkblas kaapi ${BLAS_LIBRARIES}
"$<$<BOOL:${KAAPI_USE_CUDA_RT}>:CUDA::cublas>"
"$<$<BOOL:${OpenMP_C_FOUND}>:OpenMP::OpenMP_C>"
m)
install(TARGETS ctesting RUNTIME DESTINATION testing)

#--- Testing_d
set(SOURCES_TESTING_D
  dtesting_gemm.c
  dtesting_trsm.c
  dtesting_trmm.c
  dtesting_symm.c
  dtesting_syrk.c
  dtesting_syr2k.c
  dplgsy.c
  dtesting_auxiliary.h dtesting_auxiliary.c
  )

if(XKBLAS_BUILD_COPYSCALE)
				list(APPEND SOURCES_TESTING_D dtesting_copyscale.c dtesting_mumps_like.c)
endif()

add_executable(dtesting ${SOURCES_TESTING_D})
target_compile_options(dtesting PUBLIC -UPRECISION_s -UPRECISION_z -UPRECISION_c)
target_compile_definitions(dtesting PUBLIC PRECISION_d=1)
target_link_libraries(dtesting xkblas kaapi ${BLAS_LIBRARIES}
"$<$<BOOL:${KAAPI_USE_CUDA_RT}>:CUDA::cublas>"
"$<$<BOOL:${KAAPI_USE_HIP}>:${HIP_LIBRARIES}>"
"$<$<BOOL:${OpenMP_C_FOUND}>:OpenMP::OpenMP_C>"
m)
install(TARGETS dtesting RUNTIME DESTINATION testing)


#--- Testing_s
set(SOURCES_TESTING_S
  stesting_gemm.c
  stesting_trsm.c
  stesting_trmm.c
  stesting_symm.c
  stesting_syrk.c
  stesting_syr2k.c
  splgsy.c
  stesting_auxiliary.h stesting_auxiliary.c
  )

if(XKBLAS_BUILD_COPYSCALE)
				list(APPEND SOURCES_TESTING_S stesting_copyscale.c stesting_mumps_like.c)
endif()

add_executable(stesting ${SOURCES_TESTING_S})
target_compile_options(stesting PUBLIC -UPRECISION_d -UPRECISION_z -UPRECISION_c)
target_compile_definitions(stesting PUBLIC PRECISION_s=1)
target_link_libraries(stesting xkblas kaapi ${BLAS_LIBRARIES}
"$<$<BOOL:${KAAPI_USE_CUDA_RT}>:CUDA::cublas>"
"$<$<BOOL:${OpenMP_C_FOUND}>:OpenMP::OpenMP_C>"
m)
install(TARGETS stesting RUNTIME DESTINATION testing)

# - Do not install run_tests in the installation directory
configure_file(run_tests run_tests COPYONLY)

add_custom_target( testing DEPENDS stesting dtesting ctesting ztesting ${CMAKE_CURRENT_BINARY_DIR}/run_tests )

add_test(NAME ${PROJECT_NAME}_testing
COMMAND ${CMAKE_CURRENT_BINARY_DIR}/run_tests
WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
