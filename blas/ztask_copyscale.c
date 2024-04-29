/**
 *
 * @file codelet_zgemm.c
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2016 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon zgemm StarPU codelet
 *
 * @version 1.0.0
 * @comment This file has been automatically generated
 *          from Plasma 2.5.0 for CHAMELEON 1.0.0
 * @author Hatem Ltaief
 * @author Jakub Kurzak
 * @author Mathieu Faverge
 * @author Emmanuel Agullo
 * @author Cedric Castagnede
 * @author Philippe Virouleau
 * @author Thierry Gautier, add zgemmt based on zgemm
 * @author Pierre-Etienne Polet
 * @date 2024-01-24
 * @precisions normal z -> c d s
 *
 */
#include "common.h"
#include "xkblas.h"
#include "ztask.h"
#include "ztask_internal.h"

#define ROWDIM(v,m,n) ((v) == CblasNoTrans ? m : n)
#define COLDIM(v,m,n) ((v) == CblasNoTrans ? n : m)

#define STR_EXPAND(tok) #tok
#define TASK_NAME zcopyscale
#define STRNAME  "zcopyscale"
#define NAME(x) x##_##zcopyscale
#define PNAME(x) zcopyscale##_##x
//#define SIZE_NPARAM 4
//#define NPARAM 4
//#define MODE_PARAM {KAAPI_ACCESS_MODE_R,KAAPI_ACCESS_MODE_R,KAAPI_ACCESS_MODE_RW,KAAPI_ACCESS_MODE_W}
//#define ADDR_PARAM {&arg->IW, &arg->D, &arg->L, &arg->U}
#define SIZE_NPARAM 3
#define NPARAM 3
#define MODE_PARAM {KAAPI_ACCESS_MODE_R,KAAPI_ACCESS_MODE_RW,KAAPI_ACCESS_MODE_W}
#define ADDR_PARAM {&arg->D, &arg->L, &arg->U}
#define VIEW_PARAM {\
    { &arg->n, &arg->n, &arg->ldd },\
    { &arg->n, &arg->m, &arg->ldl },\
    { &arg->m, &arg->n, &arg->ldu}}
//    { &arg->n, &__one, &arg->n }, IW
#define FORMAT_TYPE kaapi_dcplx_format
#define SIZEOF_TYPE sizeof(Complex64_t)
#define DOT_COLOR "peachpuff2"
#define TASK_FLOPS FLOPS_ZCOPYSCALE(arg->n,arg->m)
#define TASK_DATA  DATA_ZCOPYSCALE(arg->n,arg->m)
// TODO redefine task FLOP and TASK DATA

/**
 *
 * @ingroup CORE_Complex64_t
 *
 */
 typedef struct {
	size_t m;
	size_t n;
	bool should_copy;
//	kaapi_access_t IW;
	kaapi_access_t D;
	size_t ldd;
	kaapi_access_t L;
	size_t ldl;
	kaapi_access_t U;
	size_t ldu;
#if defined(KAAPI_DEBUG)
	void* D_host_ptr;
	void* L_host_ptr;
	void* U_host_ptr;
	size_t Dm;
	size_t Dn;
	size_t Lm;
	size_t Ln;
	size_t Um;
	size_t Un;
#endif
} NAME(Arg);

static kaapi_format_id_t NAME(task_fmtid) = 0;

void INSERT_TASK_zcopyscale(
	size_t m, size_t n, bool should_copy,
//	xkblas_matrix_descr_t *IWh, size_t IWm, size_t IWn,
	xkblas_matrix_descr_t *Dh, size_t Dm, size_t Dn, size_t ldd,
	xkblas_matrix_descr_t *Lh, size_t Lm, size_t Ln, size_t ldl,
	xkblas_matrix_descr_t *Uh, size_t Um, size_t Un, size_t ldu )
{
		// Common for all tasks
	kaapi_task_t* task;
	xkblas_context_t* ctxt = xkblas_context_get();
	kaapi_thread_t* thread = ctxt->kthread;
	size_t tasksize = sizeof(NAME(Arg)) + sizeof(kaapi_task_withperfcnt_t);
	task = kaapi_task_alloc( thread, NAME(task_fmtid), tasksize );
	NAME(Arg)* taskarg = kaapi_task_getargst((kaapi_task_withperfcnt_t*)task,NAME(Arg));

		// Init args
	taskarg->m = m;
	taskarg->n = n;
	taskarg->should_copy = should_copy;
	//printf("Dh %p, Dm %d/%d, Dn %d/%d %p\n", Dh, Dm, Dh->mt, Dn, Dh->nt, &taskarg->D);
	kaapi_update_dependencies( thread, &taskarg->D, task,
		KAAPI_ACCESS_MODE_R, xkblas_get_handle(Dh, Dm, Dn) );	
	taskarg->ldd = ldd;

	//printf("Lh %p, Lm %d/%d, Ln %d/%d %p\n", Lh, Lm, Lh->mt, Ln, Lh->nt, &taskarg->L);
	kaapi_update_dependencies( thread, &taskarg->L, task,
		KAAPI_ACCESS_MODE_RW, xkblas_get_handle(Lh, Lm, Ln) );	
	taskarg->ldl = ldl;
	
	//printf("Uh %p, Um %d/%d, Un %d/%d %p\n", Uh, Um, Uh->mt, Un, Uh->nt, &taskarg->U);
	kaapi_update_dependencies( thread, &taskarg->U, task,
		KAAPI_ACCESS_MODE_W, xkblas_get_handle(Uh, Um, Un) );	
	taskarg->ldu = ldu;

#if defined(KAAPI_DEBUG)
	taskarg->D_host_ptr = Dh->addr;
	taskarg->L_host_ptr = Lh->addr;
	taskarg->U_host_ptr = Uh->addr;
	taskarg->Dm = Dm;
	taskarg->Dn = Dn;
	taskarg->Lm = Lm;
	taskarg->Ln = Ln;
	taskarg->Um = Um;
	taskarg->Un = Un;
#endif

	//kaapi_update_dependencies( thread, &taskarg->IW, task, KAAPI_ACCESS_MODE_R, xkblas_get_handle(IWh, IWm, IWn) );	

	// ????
#if KAAPI_USE_OCR
    		// OCR on the L (because RW) -> maybe U -> R, L -> W
	kaapi_task_set_ld(task, KAAPI_TASK_OCR_PARAM, 1);
#else
	uint16_t ldid = xkblas_get_ld(Lh, Lm, Ln);
	kaapi_task_set_ld(task, KAAPI_TASK_LD_BOUND, ldid);
#endif
    
	kaapi_taskflag_set(task, KAAPI_TASK_PERFCNT);
	kaapi_task_commit( thread, task );	
}


static void NAME(task_body_cpu)( kaapi_task_t* task, kaapi_thread_t* thread )
{
  NAME(Arg)* arg = (NAME(Arg)*)kaapi_task_getargs(task);
  
  xkblas_zcopyscale_native(
      arg->m, arg->n, arg->should_copy,
      NULL,
//    arg->IW.data, 
      arg->D.data, arg->ldd,
      arg->L.data, arg->ldl,
      arg->U.data, arg->ldu
  );
}

#if KAAPI_USE_CUDA||KAAPI_USE_HIP
extern void cuda_zcopyscale( 
	cudaStream_t cuda_stream, size_t m, size_t n, bool should_copy,
        int* IW,
	const cuDoubleComplex* D, size_t ldd,
	cuDoubleComplex* L, size_t ldl,
	cuDoubleComplex* U, size_t ldu );

static void NAME(task_body_gpu)( kaapi_task_t* task, kaapi_thread_t* thread, void* handle )
{
	NAME(Arg)* arg = (NAME(Arg)*)kaapi_task_getargs(task);
#if defined(KAAPI_DEBUG)
	printf("%s: D[%p](%i,%i) L[%p](%i,%i) U[%p](%i,%i)\n", __func__,
		arg->D_host_ptr, arg->Dm, arg->Dn,
		arg->L_host_ptr, arg->Lm, arg->Ln,
		arg->U_host_ptr, arg->Um, arg->Un);
#endif
	
	cudaStream_t cuda_stream;
        cublasGetStream( (cublasHandle_t) handle, &cuda_stream ); // TODO check error
	cuda_zcopyscale( cuda_stream, arg->m, arg->n, arg->should_copy, 
			NULL, //(int*) arg->IW.data, 
			(const cuDoubleComplex*) arg->D.data, arg->ldd, 
			(cuDoubleComplex*) arg->L.data, arg->ldl, 
			(cuDoubleComplex*) arg->U.data, arg->ldu );
}

#endif


#include "task_format.h"

