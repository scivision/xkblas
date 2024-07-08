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
 * @author Thierry Gautier
 * @date 2018-11-20
 * @precisions normal z -> c d s
 *
 */
#include "common.h"
#include "xkblas.h"
#include "ztask.h"
#include "ztask_internal.h"

#define HEAVY_DEBUG 0
#if HEAVY_DEBUG && PRECISION_d
#warning "Heavy debug enabled. Execution time is highly slow down."
#include <math.h>
#endif

#define STR_EXPAND(tok) #tok

#define ROWDIM(v,m,n) ((v) == CblasNoTrans ? m : n)
#define COLDIM(v,m,n) ((v) == CblasNoTrans ? n : m)

/* Here is assumed the pointer to the struct argument is 'arg'.
   Must be passed as a macro parameter.
 */
 
/* Macros to defined the task format data structure for GEMM computation.
   GEMM == C <- alpha*A*B + beta C.
   A, B are read access data. C is either write accessed if beta==0 or read-write access data.
   A, B and C are 2D matrices with given view.
*/
#define TASK_NAME zgemm
#define STRNAME  "zgemm"
#define NAME(x) x##_##zgemm
#define PNAME(x) zgemm##_##x
#define SIZE_NPARAM 3
#define NPARAM 3
#define MODE_PARAM {KAAPI_ACCESS_MODE_R, KAAPI_ACCESS_MODE_R, arg->beta == 0.0 ? KAAPI_ACCESS_MODE_W : KAAPI_ACCESS_MODE_RW}
#define ADDR_PARAM {&arg->A, &arg->B, &arg->C}
#define VIEW_PARAM {\
    { ROWDIM(arg->transA, &arg->m, &arg->k), COLDIM(arg->transA, &arg->m, &arg->k), &arg->lda},\
    { ROWDIM(arg->transB, &arg->k, &arg->n), COLDIM(arg->transB, &arg->k, &arg->n), &arg->ldb},\
    { &arg->m, &arg->n, &arg->ldc}}
#define FORMAT_TYPE kaapi_dcplx_format
#define SIZEOF_TYPE sizeof(Complex64_t)
#define DOT_COLOR "chartreuse3"
#define TASK_FLOPS FLOPS_ZGEMM(arg->m,arg->n,arg->k)
#define TASK_DATA  DATA_ZGEMM(arg->m,arg->n,arg->k)


/**
 * The task' data structure of arguments.
 *
 */
 typedef struct {
  int transA;
  int transB;
  size_t m;
  size_t n;
  size_t k;
  Complex64_t alpha;
  kaapi_access_t A;
  size_t lda;
  kaapi_access_t B;
  size_t ldb;
  Complex64_t beta;
  kaapi_access_t C;
  size_t ldc;
#if KAAPI_DEBUG
	/* debug */
	void* A_host_ptr;
	void* B_host_ptr;
	void* C_host_ptr;
	size_t Am;
	size_t An;
	size_t Bm;
	size_t Bn;
	size_t Cm;
	size_t Cn;
#endif
  xkblas_mode_math_t mm;
} NAME(Arg);

static kaapi_format_id_t NAME(task_fmtid) = 0;

void INSERT_TASK_zgemm_v2(
    int transA, int transB,
    size_t m, size_t n, size_t k,
    Complex64_t alpha,
    const Complex64_t * A, size_t A_tm, size_t A_tn, size_t LDA,
    const Complex64_t * B, size_t B_tm, size_t B_tn, size_t LDB,
    Complex64_t beta,
    const Complex64_t * C, size_t C_tm, size_t C_tn, size_t LDC
) {
    xkblas_context_t * ctxt = xkblas_context_get();
    kaapi_thread_t * thread = ctxt->kthread;
    size_t tasksize = sizeof(NAME(Arg)) + sizeof(kaapi_task_withperfcnt_t);
    kaapi_task_t * task = kaapi_task_alloc(thread, NAME(task_fmtid), tasksize);
    NAME(Arg) * taskarg = kaapi_task_getargst((kaapi_task_withperfcnt_t*)task,NAME(Arg));

    taskarg->transA = transA;
    taskarg->transB = transB;
    taskarg->m = m;
    taskarg->n = n;
    taskarg->k = k;
    taskarg->alpha = alpha;
    taskarg->lda   = LDA;
    taskarg->ldb   = LDB;
    taskarg->beta  = beta;
    taskarg->ldc   = LDC;

#if KAAPI_DEBUG
    taskarg->A_host_ptr = taskarg->A.data;
    taskarg->B_host_ptr = taskarg->B.data;
    taskarg->C_host_ptr = taskarg->C.data;
    taskarg->Am = A_tm;
    taskarg->An = A_tn;
    taskarg->Bm = B_tm;
    taskarg->Bn = B_tn;
    taskarg->Cm = C_tm;
    taskarg->Cn = C_tn;
#endif

    taskarg->mm = xkblas_get_modemath();

    assert(0 && "Not implemented (dependences)");

    # if 0
    kaapi_update_dependencies(thread, &taskarg->A, task,
        KAAPI_ACCESS_MODE_R, xkblas_get_handle(Ah, Am, An));
    taskarg->lda = lda;
    kaapi_update_dependencies(thread, &taskarg->B, task,
        KAAPI_ACCESS_MODE_R, xkblas_get_handle(Bh, Bm, Bn));
    taskarg->ldb = ldb;
    kaapi_update_dependencies(thread, &taskarg->C, task,
        beta == 0.0 ? KAAPI_ACCESS_MODE_W : KAAPI_ACCESS_MODE_RW, xkblas_get_handle(Ch, Cm, Cn));
    # endif

#if KAAPI_USE_OCR
    /* OCR on the third parameter */
    kaapi_task_set_ld(task, KAAPI_TASK_OCR_PARAM, 2);
#else
    uint16_t ldid = xkblas_get_ld(Ch, Cm, Cn);
    kaapi_task_set_ld(task, KAAPI_TASK_LD_BOUND, ldid);
#endif
    kaapi_taskflag_set(task, KAAPI_TASK_PERFCNT);
    kaapi_task_commit( thread, task );
}

void INSERT_TASK_zgemm(
    int transA, int transB,
    size_t m, size_t n, size_t k, 
    Complex64_t alpha, xkblas_matrix_descr_t *Ah, size_t Am, size_t An, size_t lda,
                       xkblas_matrix_descr_t *Bh, size_t Bm, size_t Bn, size_t ldb,
    Complex64_t beta,  xkblas_matrix_descr_t *Ch, size_t Cm, size_t Cn, size_t ldc)
{
    kaapi_task_t* task;
    xkblas_context_t* ctxt = xkblas_context_get();
    kaapi_thread_t* thread = ctxt->kthread;
    size_t tasksize = sizeof(NAME(Arg)) + sizeof(kaapi_task_withperfcnt_t);
    task = kaapi_task_alloc( thread, NAME(task_fmtid), tasksize );
    NAME(Arg)* taskarg = kaapi_task_getargst((kaapi_task_withperfcnt_t*)task,NAME(Arg));

    taskarg->transA = transA;
    taskarg->transB = transB;
    taskarg->m = m;
    taskarg->n = n;
    taskarg->k = k;
    taskarg->alpha = alpha;
    kaapi_update_dependencies(thread, &taskarg->A, task,
        KAAPI_ACCESS_MODE_R, xkblas_get_handle(Ah, Am, An));
    taskarg->lda = lda;
    kaapi_update_dependencies(thread, &taskarg->B, task,
        KAAPI_ACCESS_MODE_R, xkblas_get_handle(Bh, Bm, Bn));
    taskarg->ldb = ldb;
    kaapi_update_dependencies(thread, &taskarg->C, task,
        beta == 0.0 ? KAAPI_ACCESS_MODE_W : KAAPI_ACCESS_MODE_RW, xkblas_get_handle(Ch, Cm, Cn));
    taskarg->beta = beta;
    taskarg->ldc = ldc;
    taskarg->mm = xkblas_get_modemath();
#if KAAPI_DEBUG
    taskarg->A_host_ptr = taskarg->A.data;
    taskarg->B_host_ptr = taskarg->B.data;
    taskarg->C_host_ptr = taskarg->C.data;
    taskarg->Am = Am;
    taskarg->An = An;
    taskarg->Bm = Bm;
    taskarg->Bn = Bn;
    taskarg->Cm = Cm;
    taskarg->Cn = Cn;
#endif
#if KAAPI_USE_OCR
    /* OCR on the third parameter */
    kaapi_task_set_ld(task, KAAPI_TASK_OCR_PARAM, 2);
#else
    uint16_t ldid = xkblas_get_ld(Ch, Cm, Cn);
    kaapi_task_set_ld(task, KAAPI_TASK_LD_BOUND, ldid);
#endif
    kaapi_taskflag_set(task, KAAPI_TASK_PERFCNT);
    kaapi_task_commit( thread, task );
}


static void NAME(task_body_cpu)( kaapi_task_t* task, kaapi_thread_t* thread )
{
  NAME(Arg)* arg = (NAME(Arg)*)kaapi_task_getargs(task);
  xkblas_zgemm_native(
      arg->transA, arg->transB,
      arg->m, arg->n, arg->k,
      &arg->alpha,
      (Complex64_t*)arg->A.data, arg->lda,
      (Complex64_t*)arg->B.data, arg->ldb,
      &arg->beta,
      (Complex64_t*)arg->C.data, arg->ldc
  );
}

#if KAAPI_USE_CUDA||KAAPI_USE_HIP
static void NAME(task_body_gpu)( kaapi_task_t* task, kaapi_thread_t* thread, void* handle )
{
  NAME(Arg)* arg = (NAME(Arg)*)kaapi_task_getargs(task);
  double flops = FLOPS_ZGEMM(arg->m,arg->n,arg->k);
  cublasStatus_t res;

#if (PRECISION_s||(CUDART_VERSION>=1100)) && (__HIP_PLATFORM_AMD__==0)
  if (arg->mm == XKBLAS_TENSOR_OP_MATH)
  {
    res = cublasSetMathMode((cublasHandle_t)handle, CUBLAS_TENSOR_OP_MATH);
    kaapi_assert(res == CUBLAS_STATUS_SUCCESS);
    /* no possible to know in advance if all the ops use the tensor core or not */
  }
  else
#endif 
  {
#if __HIP_PLATFORM_AMD__==0
    res = cublasSetMathMode((cublasHandle_t)handle, CUBLAS_DEFAULT_MATH);
    kaapi_assert(res == CUBLAS_STATUS_SUCCESS);
#endif
  }

#if 0//KAAPI_DEBUG
	printf("%x:: task:%p %s[%d,%d,%d]: A[%p/%p](%i,%i) B[%p/%p](%i,%i) C[%p/%p](%i,%i)\n", pthread_self(), task, __func__,
		arg->m, arg->n, arg->k,
		arg->A_host_ptr, arg->A.data, arg->Am, arg->An,
		arg->B_host_ptr, arg->B.data, arg->Bm, arg->Bn,
		arg->C_host_ptr, arg->C.data, arg->Cm, arg->Cn);
#endif

  res = cublasZgemm((cublasHandle_t)handle,
      cblas2cublas_op(arg->transA), cblas2cublas_op(arg->transB),
      arg->m, arg->n, arg->k,
      (const cuDoubleComplex*)&arg->alpha,
      (const cuDoubleComplex*)arg->A.data, arg->lda,
      (const cuDoubleComplex*)arg->B.data, arg->ldb,
      (const cuDoubleComplex*)&arg->beta,
      (cuDoubleComplex*)arg->C.data, arg->ldc
  );
  kaapi_assert(res == CUBLAS_STATUS_SUCCESS);
}

#endif //USE CUDA


#include "task_format.h"

