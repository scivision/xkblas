
if (KAAPI_USE_CUDA_DRIVER OR KAAPI_USE_CUDA_RT)
  configure_file(common.gen common.h COPYONLY)
else()
  add_custom_command (
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/common.h
    COMMAND hipify-perl ${CMAKE_CURRENT_SOURCE_DIR}/common.gen > ${CMAKE_CURRENT_BINARY_DIR}/common.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/common.gen
  )
endif()


if (KAAPI_USE_HIP)
  add_compile_definitions(HIPBLAS_V1)
endif()

#message(STATUS "----------- INCLUDE DIR: BLAS_INCLUDE_DIRS:" ${BLAS_INCLUDE_DIRS})
#message(STATUS "----------- INCLUDE DIR: HIPBLAS_INCLUDE_DIRS:" ${HIPBLAS_INCLUDE_DIRS})
#message(STATUS "----------- INCLUDE DIR: HIP_INCLUDE_DIRS:" ${HIP_INCLUDE_DIRS})

# Bootstrap
set(BLAS_FUNCTION_LIST gemm gemmt trsm trmm syrk syr2k symm herk her2k hemm)
if (XKBLAS_BUILD_COPYSCALE)
	list(APPEND BLAS_FUNCTION_LIST copyscale)
endif()

# internal_register.h
# Current implementation generates explicit call to register function for each known task.
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/internal_register.h
		COMMAND ${XKblas_SOURCE_DIR}/gen_internal_register.sh ${BLAS_FUNCTION_LIST}
    DEPENDS ${XKblas_SOURCE_DIR}/gen_internal_register.sh ${CMAKE_CURRENT_SOURCE_DIR}/task_format.h
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/ztask_internal.h
  COMMAND ${XKblas_SOURCE_DIR}/gen_precision_one.sh z ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${XKblas_SOURCE_DIR}/gen_precision_one.sh
)
add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/ctask_internal.h
  COMMAND ${XKblas_SOURCE_DIR}/gen_precision_one.sh c ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${XKblas_SOURCE_DIR}/gen_precision_one.sh
)
add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/dtask_internal.h
  COMMAND ${XKblas_SOURCE_DIR}/gen_precision_one.sh d ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${XKblas_SOURCE_DIR}/gen_precision_one.sh
)
add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/stask_internal.h
  COMMAND ${XKblas_SOURCE_DIR}/gen_precision_one.sh s ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ztask_internal.h ${XKblas_SOURCE_DIR}/gen_precision_one.sh
)


list(APPEND SOURCES_S
  sgemm.c sgemmt.c strsm.c strmm.c ssymm.c ssyrk.c ssyr2k.c sxkblas.h sxkblas_f77.h
  stask_gemm.c stask_symm.c stask_gemmt.c stask_trsm.c stask_trmm.c stask_syrk.c stask_syr2k.c stask.h  stask_internal.h
)

list(APPEND SOURCES_D
  dgemm.c dgemmt.c dtrsm.c dtrmm.c dsymm.c dsyrk.c dsyr2k.c dxkblas.h dxkblas_f77.h
  dtask_gemm.c dtask_symm.c dtask_gemmt.c dtask_trsm.c dtask_trmm.c dtask_syrk.c dtask_syr2k.c dtask.h dtask_internal.h
)

list(APPEND SOURCES_C
  cgemm.c cgemmt.c ctrsm.c ctrmm.c csymm.c csyrk.c csyr2k.c chemm.c cherk.c cher2k.c cxkblas.h cxkblas_f77.h
  ctask_gemm.c ctask_symm.c ctask_gemmt.c ctask_trsm.c ctask_trmm.c ctask_syrk.c ctask_syr2k.c ctask_hemm.c ctask_herk.c ctask_her2k.c ctask.h ctask_internal.h
)

list(APPEND SOURCES_Z
  zgemm.c zgemmt.c ztrsm.c ztrmm.c zsymm.c zsyrk.c zsyr2k.c zhemm.c zherk.c zher2k.c zswap.c zxkblas.h zxkblas_f77.h
  ztask_gemm.c ztask_symm.c ztask_gemmt.c ztask_trsm.c ztask_trmm.c ztask_syrk.c ztask_syr2k.c ztask_hemm.c ztask_herk.c ztask_her2k.c ztask_swap.c ztask.h ztask_internal.h
)

if (XKBLAS_BUILD_COPYSCALE)
	add_compile_definitions(XKBLAS_BUILD_COPYSCALE=1)
	list(APPEND SOURCES_S scopyscale.c stask_copyscale.c scopyscale.cu)
	list(APPEND SOURCES_D dcopyscale.c dtask_copyscale.c dcopyscale.cu)
	list(APPEND SOURCES_C ccopyscale.c ctask_copyscale.c ccopyscale.cu)
	list(APPEND SOURCES_Z zcopyscale.c ztask_copyscale.c zcopyscale.cu)
endif()

list(APPEND GENERATED_SOURCES ${SOURCES_S} ${SOURCES_D} ${SOURCES_C})


# Generation of precision code
foreach(file IN LISTS GENERATED_SOURCES)
  string(REGEX REPLACE "^[sdc](.*)$" "z\\1" source "${file}")
  string(REGEX REPLACE "^([sdc]).*$" "\\1" precision "${file}")
  add_custom_command(
      OUTPUT  ${file}
      COMMAND ${XKblas_SOURCE_DIR}/gen_precision_one.sh ${precision} ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${XKblas_SOURCE_DIR}/gen_precision_one.sh
  )
  #message(STATUS "Gen file ${file} == precision ${precision} from ${source}")
endforeach()


# Append special file Xtask_internal.h
list(APPEND GENERATED_SOURCES
    ztask_internal.h
    ctask_internal.h
    dtask_internal.h
    stask_internal.h
)

#Append source directory to source file
foreach(file IN LISTS SOURCES_Z)
  list(APPEND ASOURCES_Z "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
endforeach()
set(SOURCES_Z ${ASOURCES_Z})
#message(STATUS "Generated Source Z:" ${SOURCES_Z})

#Append directory to generated file
foreach(file IN LISTS GENERATED_SOURCES)
  #message(STATUS "Change paths to ${file}")
  list(APPEND AGENERATED_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/${file}")
endforeach()

set(GENERATED_SOURCES ${AGENERATED_SOURCES})
#message(STATUS "Generated Source:" ${GENERATED_SOURCES})


#--- XKBLAS

if (XKBLAS_BUILD_COPYSCALE)
set(CUDA_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/zcopyscale.cu
	${CMAKE_CURRENT_BINARY_DIR}/scopyscale.cu
	${CMAKE_CURRENT_BINARY_DIR}/dcopyscale.cu
	${CMAKE_CURRENT_BINARY_DIR}/ccopyscale.cu
)
endif()

add_custom_target(task_internal_headers DEPENDS
	${CMAKE_CURRENT_BINARY_DIR}/stask_internal.h
	${CMAKE_CURRENT_BINARY_DIR}/dtask_internal.h
	${CMAKE_CURRENT_BINARY_DIR}/ctask_internal.h
	${CMAKE_CURRENT_BINARY_DIR}/ztask_internal.h
)

if (KAAPI_USE_HIP)
  message(STATUS "Compilation with HIBLAS in : ${HIPBLAS_INCLUDE_DIR}")
  list(APPEND HIPIFY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/xkblas.c ${CMAKE_CURRENT_SOURCE_DIR}/dbg_blas.c
    ${SOURCES_Z}
    ${GENERATED_SOURCES}
  )
  set(HEADER_FILE ${HIPIFY_FILE})
  list(FILTER HIPIFY_FILE INCLUDE REGEX "(.)*.c$")
  list(FILTER HEADER_FILE EXCLUDE REGEX "(.)*.c$")
#  message(FATAL_ERROR "HEADER_FILE: ${HEADER_FILE}")
  foreach(file IN LISTS HIPIFY_FILE)
     string(REPLACE ".c" ".hip.c" file_hip ${file})
     get_filename_component(file_hip ${file_hip} NAME )
     string(REGEX REPLACE "^([sdc]).*$" "\\1" precision "${file}")
     add_custom_command (
       OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file_hip}
       #COMMAND hipify-perl ${file}|sed -e 's+hipComplex+hipblasComplex+g' -e 's+hipDoubleComplex+hipblasDoubleComplex+g' -e 's+cublasChemm+hipblasChemm+g' -e 's+cublasCher2k+hipblasCher2k+g' -e 's+cublasCherk+hipblasCherk+g' -e 's+cublasCsymm+hipblasCsymm+g' -e 's+cublasCsyr2k+hipblasCsyr2k+g' -e 's+cublasCsyrk+hipblasCsyrk+g' -e 's+cublasCtrmm+hipblasCtrmm+g' -e 's+cublasCtrsm+hipblasCtrsm+g' -e 's+cublasDsymm+hipblasDsymm+g' -e 's+cublasDsyr2k+hipblasDsyr2k+g' -e 's+cublasDsyrk+hipblasDsyrk+g' -e 's+cublasDtrmm+hipblasDtrmm+g' -e 's+cublasSsymm+hipblasSsymm+g' -e 's+cublasSsyr2k+hipblasSsyr2k+g' -e 's+cublasSsyrk+hipblasSsyrk+g' -e 's+cublasStrmm+hipblasStrmm+g' -e 's+cublasZhemm+hipblasZhemm+g' -e 's+cublasZher2k+hipblasZher2k+g' -e 's+cublasZherk+hipblasZherk+g' -e 's+cublasZsymm+hipblasZsymm+g' -e 's+cublasZsyr2k+hipblasZsyr2k+g' -e 's+cublasZsyrk+hipblasZsyrk+g' -e 's+cublasZtrmm+hipblasZtrmm+g' -e 's+cublasZtrsm+hipblasZtrsm+g' > ${file_hip}
       COMMAND hipify-perl ${file}|sed -e 's+hipComplex+hipComplex+g' -e 's+hipDoubleComplex+hipDoubleComplex+g' -e 's+cublasChemm+hipblasChemm+g' -e 's+cublasCher2k+hipblasCher2k+g' -e 's+cublasCherk+hipblasCherk+g' -e 's+cublasCsymm+hipblasCsymm+g' -e 's+cublasCsyr2k+hipblasCsyr2k+g' -e 's+cublasCsyrk+hipblasCsyrk+g' -e 's+cublasCtrmm+hipblasCtrmm+g' -e 's+cublasCtrsm+hipblasCtrsm+g' -e 's+cublasDsymm+hipblasDsymm+g' -e 's+cublasDsyr2k+hipblasDsyr2k+g' -e 's+cublasDsyrk+hipblasDsyrk+g' -e 's+cublasDtrmm+hipblasDtrmm+g' -e 's+cublasSsymm+hipblasSsymm+g' -e 's+cublasSsyr2k+hipblasSsyr2k+g' -e 's+cublasSsyrk+hipblasSsyrk+g' -e 's+cublasStrmm+hipblasStrmm+g' -e 's+cublasZhemm+hipblasZhemm+g' -e 's+cublasZher2k+hipblasZher2k+g' -e 's+cublasZherk+hipblasZherk+g' -e 's+cublasZsymm+hipblasZsymm+g' -e 's+cublasZsyr2k+hipblasZsyr2k+g' -e 's+cublasZsyrk+hipblasZsyrk+g' -e 's+cublasZtrmm+hipblasZtrmm+g' -e 's+cublasZtrsm+hipblasZtrsm+g' > ${file_hip}
       DEPENDS ${file}
       DEPENDS zxkblas.h cxkblas.h dxkblas.h sxkblas.h ctask.h dtask.h stask.h
     )
     #message(STATUS "Hipyfied: ${file} -> ${CMAKE_CURRENT_BINARY_DIR}/${file_hip}")
     list(APPEND HIPYFIED_FILE ${CMAKE_CURRENT_BINARY_DIR}/${file_hip})
  endforeach()

	# HIP compilation for copyscale
	#set(hip_object "${CMAKE_CURRENT_BINARY_DIR}/hip_object.o")
	set(HIP_OBJECTS)
	set(HIP_SOURCES)
	foreach(source_file ${CUDA_SOURCES})
		get_filename_component(base_name ${source_file} NAME_WE)
		set(hip_file "${CMAKE_CURRENT_BINARY_DIR}/${base_name}_hip.cpp")
		set(obj_file "${CMAKE_CURRENT_BINARY_DIR}/${base_name}_hip.o")

		list(APPEND HIP_SOURCES ${hip_file})
		list(APPEND HIP_OBJECTS ${obj_file})

		add_custom_command(
			OUTPUT ${hip_file}
			COMMAND hipify-perl ${source_file} > ${hip_file}
			DEPENDS ${source_file}
			COMMENT "Converting CUDA sources to HIP sources"
		)

		add_custom_command(
			OUTPUT ${obj_file}
			COMMAND hipcc -c -fPIC ${hip_file} -o ${obj_file} -I ${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${hip_file}
			COMMENT "Build HIP source"
		)
	endforeach()

	#add_custom_command(
	#		OUTPUT ${hip_object}
	#		COMMAND	hipcc -c ${HIP_SOURCES} -o ${hip_object}
	#		DEPENDS ${HIP_SOURCES}
	#		COMMENT "Build object file from HIP sources"
	#)

  add_library(xkblas SHARED internal_register.h common.h xkblas.h ${HEADER_FILE} ${HIPYFIED_FILE}
					${HIP_OBJECTS})
	#				${hip_object})
  add_dependencies(xkblas task_internal_headers)
else()

  if (KAAPI_USE_CUDA_RT AND XKBLAS_BUILD_COPYSCALE)
    set(CUDA_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/zcopyscale.cu
      ${CMAKE_CURRENT_BINARY_DIR}/scopyscale.cu
      ${CMAKE_CURRENT_BINARY_DIR}/dcopyscale.cu
      ${CMAKE_CURRENT_BINARY_DIR}/ccopyscale.cu
    )

    add_library(xkblas_cuda_objects OBJECT ${CUDA_SOURCES})
    target_compile_options(xkblas_cuda_objects PRIVATE
      $<$<COMPILE_LANGUAGE:CUDA>:--ptxas-options=-v>
    )
  endif()

	add_library(xkblas SHARED internal_register.h common.h xkblas.h xkblas.c dbg_blas.c
		${SOURCES_Z}
		${GENERATED_SOURCES}
    $<$<TARGET_EXISTS:xkblas_cuda_objects>:xkblas_cuda_objects>
	)
	set_source_files_properties(${SOURCES_Z} ${GENERATED_SOURCES} PROPERTIES COMPILE_FLAGS "-fPIC" )
	add_dependencies(xkblas task_internal_headers)
endif()

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set_target_properties(xkblas PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

target_link_libraries(xkblas PRIVATE $<$<BOOL:${OpenMP_C_FOUND}>:OpenMP::OpenMP_C>)

target_link_libraries(xkblas PUBLIC kaapi)
target_link_libraries(xkblas PRIVATE
"$<$<BOOL:${KAAPI_USE_CUDA_RT}>:CUDA::cublas>"
"$<$<BOOL:${KAAPI_USE_HIP}>:${HIPBLAS_LIBRARIES};${ROCBLAS_LIBRARIES}>"
stdc++
)
set_target_properties(xkblas PROPERTIES LINKER_LANGUAGE C)

target_include_directories(xkblas PUBLIC
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR};${BLAS_INCLUDE_DIRS};${HIP_INCLUDE_DIRS};${HIPBLAS_INCLUDE_DIRS}>"
$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS xkblas)
install(FILES xkblas.h ${CMAKE_CURRENT_BINARY_DIR}/sxkblas.h ${CMAKE_CURRENT_BINARY_DIR}/dxkblas.h ${CMAKE_CURRENT_BINARY_DIR}/cxkblas.h zxkblas.h
TYPE INCLUDE)
