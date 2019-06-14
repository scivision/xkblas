/*
** Copyright 2009-2013,2018,2019 INRIA
**
** Contributors :
**
** thierry.gautier@inrialpes.fr
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

#define _GNU_SOURCE
#include <dlfcn.h>
//#include <stdlib.h>
//#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "flops.h"

#define KAAPI_NO_INCLUDE_BLAS_H
#include "common.h"
#include "kaapi_impl.h"

#define LIBNAME "[xkblas]"

#define TRACE_MSG 1

#ifndef XKBLAS_BLASLIB
#error "XKBLAS_BLASLIB macro should point to the (absolute) path of the libblas to load "
#endif

#define STR_EXPAND2(tok) #tok
#define STR_EXPAND(tok) STR_EXPAND2(tok)
#define XCAT2(w,x,y,z) w##x##y##z
#define XCAT(w,x,y,z) XCAT2(w,x,y,z)
#define BLAS_NAME(prefix,name)	  XCAT(prefix,name,_,)
#define SYMBLAS_NAME(prefix,name) XCAT(prefix,name,_,sym)


/* Pointer for cblas_Xgemm to the loaded BLAS library
*/
/* gemm */
static void (*cblas_sgemm_dl)(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  const float alpha, const float *A, int lda,
                     const float *B, int ldb,
  const float beta,  float *C, int ldc ) = 0;

static void (*cblas_dgemm_dl)(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  const double alpha, const double *A, int lda,
                      const double *B, int ldb,
  const double beta,  double *C, int ldc) = 0;

static void (*cblas_cgemm_dl)(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  const void *alpha, const void *A, int lda,
                     const void *B, int ldb,
  const void *beta,        void *C, int ldc ) = 0;

static void (*cblas_zgemm_dl)(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  const void *alpha, const void *A, int lda,
                     const void *B, int ldb,
  const void *beta,        void *C, int ldc ) = 0;


/* gemmt */
static void (*cblas_sgemmt_dl)(
  CBLAS_LAYOUT order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int N, int K,
  const float alpha, const float *A, int lda,
                     const float *B, int ldb,
  const float beta,  float *C, int ldc ) = 0;

static void (*cblas_dgemmt_dl)(
  CBLAS_LAYOUT order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int N, int K,
  const double alpha, const double *A, int lda,
                      const double *B, int ldb,
  const double beta,  double *C, int ldc) = 0;

static void (*cblas_cgemmt_dl)(
  CBLAS_LAYOUT order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int N, int K,
  const void *alpha, const void *A, int lda,
                     const void *B, int ldb,
  const void *beta,        void *C, int ldc ) = 0;

static void (*cblas_zgemmt_dl)(
  CBLAS_LAYOUT order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int N, int K,
  const void *alpha, const void *A, int lda,
                     const void *B, int ldb,
  const void *beta,        void *C, int ldc ) = 0;


/* trsm */
static void (*cblas_strsm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const float alpha, const float *A, int lda, float *B, int ldb) = 0;

static void (*cblas_dtrsm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const double alpha, const double *A, int lda, double *B, int ldb) = 0;

static void (*cblas_ctrsm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const void *alpha, const void *A, int lda, void *B, int ldb) = 0;

static void (*cblas_ztrsm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const void *alpha, const void *A, int lda, void *B, int ldb)= 0;


/* trmm */
static void (*cblas_strmm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const float alpha, const float *A, int lda, float *B, int ldb) = 0;

static void (*cblas_dtrmm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const double alpha, const double *A, int lda, double *B, int ldb) = 0;

static void (*cblas_ctrmm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const void *alpha, const void *A, int lda, void *B, int ldb) = 0;

static void (*cblas_ztrmm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE TransA, CBLAS_DIAG Diag,
  int M, int N,
  const void *alpha, const void *A, int lda, void *B, int ldb)= 0;


/* symm */
static void (*cblas_ssymm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, 
  int M, int N,
  float alpha, float *A, int lda, float *B, int ldb, float beta, float *C, int ldc) =0;

static void (*cblas_dsymm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, 
  int M, int N,
  double alpha, double *A, int lda, double *B, int ldb, double beta, double *C, int ldc) =0;

static void (*cblas_csymm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, 
  int M, int N,
  void *alpha, void *A, int lda, void *B, int ldb, void *beta, void *C, int ldc) =0;

static void (*cblas_zsymm_dl)(
  CBLAS_ORDER Order, CBLAS_SIDE Side, CBLAS_UPLO Uplo, 
  int M, int N,
  void *alpha, void *A, int lda, void *B, int ldb, void *beta, void *C, int ldc) =0;


/* syrk */
static void (*cblas_ssyrk_dl)(
  CBLAS_ORDER Order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE Trans,
  int N, int K, 
  float alpha, float *A, int lda, float beta, float *C, int ldc) =0;

static void (*cblas_dsyrk_dl)(
  CBLAS_ORDER Order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE Trans,
  int N, int K, 
  double alpha, double *A, int lda, double beta, double *C, int ldc) =0;

static void (*cblas_csyrk_dl)(
  CBLAS_ORDER Order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE Trans,
  int N, int K, 
  void *alpha, void *A, int lda, void *beta, void *C, int ldc) =0;

static void (*cblas_zsyrk_dl)(
  CBLAS_ORDER Order, CBLAS_UPLO Uplo, CBLAS_TRANSPOSE Trans,
  int N, int K, 
  void *alpha, void *A, int lda, void *beta, void *C, int ldc) =0;

/* */
void Usage(void)
{
  printf("XKBLAS_NGPUS:\n");
  printf("XKBLAS_TILE_SIZE:\n");
  printf("XKBLAS_THRESHOLD:\n");
  printf("\n");
  printf("KAAPI_CUDA_KERNEL_STREAM_NUMS:\n");
  abort();
}

/* Name of kernels for specific thresholds
*/
enum {
  GEN = 0,
  ZGEMM, CGEMM, DGEMM, SGEMM,
  ZGEMMT, CGEMMT, DGEMMT, SGEMMT,
  ZTRSM, CTRSM, DTRSM, STRSM,
  ZTRMM, CTRMM, DTRMM, STRMM,
  ZSYMM, CSYMM, DSYMM, SSYMM,
  ZSYRK, CSYRK, DSYRK, SSYRK,
  LAST
};
static void* handle_blas = 0;
static long threshold = 1600;
static double threshold_kern[LAST];
static char* name_kern[LAST] = {
  "GEN",
  "ZGEMM", "CGEMM", "DGEMM", "SGEMM",
  "ZGEMMT", "CGEMMT", "DGEMMT", "SGEMMT",
  "ZTRSM", "CTRSM", "DTRSM", "STRSM",
  "ZTRMM", "CTRMM", "DTRMM", "STRMM",
  "ZSYMM", "CSYMM", "DSYMM", "SSYMM",
  "ZSYRK", "CSYRK", "DSYRK", "SSYRK"
};


/*
*/
__attribute__((constructor)) void toto_constructor(void) 
{
  printf(LIBNAME ": library loaded. Blas library '%s'\n",XKBLAS_BLASLIB);

  if (getenv("XKBLAS_HELP"))
    Usage();
  if (getenv("XKBLAS_NGPUS"))
    setenv("KAAPI_NUM_GPUS",getenv("XKBLAS_NGPUS"),1);

  /* */
  for (int i=0; i<LAST; ++i)
    threshold_kern[i] = threshold;

  if (0 != xkblas_init())
  {
    printf(LIBNAME ": cannot initialize\n");
    abort();
  }
  extern const char* get_kaapi_version(void);
  printf(LIBNAME ": version :%s\n", get_kaapi_version());
  printf(LIBNAME ": ngpus :%i\n", kaapi_default_param.ngpus);

  if ((kaapi_default_param.ngpus>0) && (getenv("XKBLAS_THRESHOLD")))
  {
    threshold=atol(getenv("XKBLAS_THRESHOLD"));
    if (threshold <0) threshold = 0;
    for (int i=0; i<LAST; ++i)
      threshold_kern[i] = threshold;
  }
  printf(LIBNAME ": threshold :%i\n", threshold);

  if ((kaapi_default_param.ngpus>0) && (getenv("XKBLAS_THRESHOLD")))
  {
    threshold=atol(getenv("XKBLAS_THRESHOLD"));
    if (threshold <0) threshold = 0;
    threshold_kern[GEN] = threshold;
  }
  printf(LIBNAME ": threshold :%i\n", threshold);
  if (kaapi_default_param.ngpus>0)
  {
    char* name = "XKBLAS_THRESHOLD_PXXYYZZ";
    for (int i=0; i<LAST; ++i)
    {
      strcpy( &name[17], name_kern[i] );
      if (getenv(name))
      {
        long th=atol(getenv(name));
        if (th <0) th = 0;
        threshold_kern[i] = th;
      }
      else
        threshold_kern[i] = threshold;
      printf(LIBNAME ": %i =%i\n", name, threshold_kern[i] );
    }
  }


  handle_blas = dlopen(XKBLAS_BLASLIB,RTLD_LAZY);
  if (handle_blas ==0)
  {
    printf(LIBNAME ": cannot load liblas '%s'\n", XKBLAS_BLASLIB);
    abort();
  }
}

static void xkblas_load_sym(void** ptr, const char* name)
{
  *ptr = dlsym( handle_blas, name );
  if (*ptr ==0)
  {
    fprintf(stderr,"*** Error: " LIBNAME " cannot load symbol '%s' from '%s'\n", name, XKBLAS_BLASLIB);
    abort();
  }
}

/*
*/
__attribute__((destructor)) void toto_destructor(void) 
{
  if (handle_blas !=0) dlclose(handle_blas);
  handle_blas = 0;
  xkblas_finalize();
}

#define DATA_MAT(m,n) ((double)(m)*(double)(n))

#define DATA__GEMM(m,n,k) ((DATA_MAT((m),(n))+DATA_MAT((m),(k))+DATA_MAT((k),(n))))
#define DATA_ZGEMM(m,n,k) (1.0*sizeof(double complex)*DATA__GEMM((m),(n),(k)))
#define DATA_CGEMM(m,n,k) (1.0*sizeof(float complex)*DATA__GEMM((m),(n),(k)))
#define DATA_DGEMM(m,n,k) (1.0*sizeof(double)*DATA__GEMM((m),(n),(k)))
#define DATA_SGEMM(m,n,k) (1.0*sizeof(float)*DATA__GEMM((m),(n),(k)))

#define DATA__GEMM(m,n,k) ((DATA_MAT((m),(n))+DATA_MAT((m),(k))+DATA_MAT((k),(n))))
#define DATA_ZGEMM(m,n,k) (1.0*sizeof(double complex)*DATA__GEMM((m),(n),(k)))
#define DATA_CGEMM(m,n,k) (1.0*sizeof(float complex)*DATA__GEMM((m),(n),(k)))
#define DATA_DGEMM(m,n,k) (1.0*sizeof(double)*DATA__GEMM((m),(n),(k)))
#define DATA_SGEMM(m,n,k) (1.0*sizeof(float)*DATA__GEMM((m),(n),(k)))

#define FLOPS_ZGEMMT(m,n,k) (0.5 * FLOPS_ZGEMM((m), (n), (k)))
#define FLOPS_CGEMMT(m,n,k) (0.5 * FLOPS_CGEMM((m), (n), (k)))
#define FLOPS_DGEMMT(m,n,k) (0.5 * FLOPS_DGEMM((m), (n), (k)))
#define FLOPS_SGEMMT(m,n,k) (0.5 * FLOPS_SGEMM((m), (n), (k)))

#define DATA__GEMMT(m,n,k) ((0.5*DATA_MAT((m),(n))+DATA_MAT((m),(k))+DATA_MAT((k),(n))))
#define DATA_ZGEMMT(m,n,k) (1.0*sizeof(double complex)*DATA__GEMMT((m),(n),(k)))
#define DATA_CGEMMT(m,n,k) (1.0*sizeof(float complex)*DATA__GEMMT((m),(n),(k)))
#define DATA_DGEMMT(m,n,k) (1.0*sizeof(double)*DATA__GEMMT((m),(n),(k)))
#define DATA_SGEMMT(m,n,k) (1.0*sizeof(float)*DATA__GEMMT((m),(n),(k)))

#define DATA__TRSM(s,m,n) ((s) == CblasLeft ? (0.5*DATA_MAT((m),(m))+2*DATA_MAT((m),(n))) : (0.5*DATA_MAT((n),(n))+2*DATA_MAT((n),(m))))
#define DATA_ZTRSM(s,m,n) (1.0*sizeof(double complex)*DATA__TRSM((s),(m),(n)))
#define DATA_CTRSM(s,m,n) (1.0*sizeof(float complex)*DATA__TRSM((s),(m),(n)))
#define DATA_DTRSM(s,m,n) (1.0*sizeof(double)*DATA__TRSM((s),(m),(n)))
#define DATA_STRSM(s,m,n) (1.0*sizeof(float)*DATA__TRSM((s),(m),(n)))

#define DATA_ZTRMM DATA_ZTRSM
#define DATA_CTRMM DATA_CTRSM
#define DATA_DTRMM DATA_DTRSM
#define DATA_STRMM DATA_STRSM


#define DATA_ZSYMM(s,m,n) ((s) == CblasLeft ? DATA_ZGEMM(m,m,n) : DATA_ZGEMM(m,n,n))
#define DATA_CSYMM(s,m,n) ((s) == CblasLeft ? DATA_CGEMM(m,m,n) : DATA_CGEMM(m,n,n))
#define DATA_DSYMM(s,m,n) ((s) == CblasLeft ? DATA_DGEMM(m,m,n) : DATA_DGEMM(m,n,n))
#define DATA_SSYMM(s,m,n) ((s) == CblasLeft ? DATA_SGEMM(m,m,n) : DATA_SGEMM(m,n,n))


#define DATA__SYRK(n,k) (0.5*DATA_MAT((n),(n))+DATA_MAT((n),(k)))
#define DATA_ZSYRK(n,k) (1.0*sizeof(double complex)*DATA__SYRK((n),(k)))
#define DATA_CSYRK(n,k) (1.0*sizeof(float complex)*DATA__SYRK((n),(k)))
#define DATA_DSYRK(n,k) (1.0*sizeof(double)*DATA__SYRK((n),(k)))
#define DATA_SSYRK(n,k) (1.0*sizeof(float)*DATA__SYRK((n),(k)))


/* ======================================================================================== */
/*
*/
extern void BLAS_NAME(z,gemm)(
    const char * transa, const char * transb,
    const int * m, const int * n, const int * k,
    const double complex* alpha, const double complex* A, const int * lda,
    const double complex * B, const int * ldb, const double complex * beta,
    double complex * C, const int * ldc)
{
  if (*m == 0 || *n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_ZGEMM(*m,*n,*k)/DATA_ZGEMM(*m,*n,*k) < threshold_kern[ZGEMM])
  {
    if (cblas_zgemm_dl ==0) xkblas_load_sym((void**)&cblas_zgemm_dl,"cblas_zgemm");
    cblas_zgemm_dl(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *m, *n, *k,
                alpha, A, *lda,
                       B, *ldb,
                beta,  C, *ldc);
  }
  else {
    xkblas_zgemm_async(xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb), *m, *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(0,0,*m, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/* cblas variant
*/
void cblas_zgemm(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  const void *alpha, const void *A, int lda,
                     const void *B, int ldb,
  const void *beta,        void *C, int ldc )
{
  if (M == 0 || N == 0 ||
    ((*(Complex64_t*)alpha == 0.0 || K == 0) && *(Complex64_t*)beta == 1.0))
    return;

  if (FLOPS_ZGEMM(M,N,K)/DATA_ZGEMM(M,N,K) < threshold_kern[ZGEMM])
  {
    if (cblas_zgemm_dl ==0) xkblas_load_sym((void**)&cblas_zgemm_dl,"cblas_zgemm");
    cblas_zgemm_dl(order, transa, transb,
                M, N, K,
                alpha, A, lda,
                       B, ldb,
                beta,  C, ldc);
  }
  else {
    /* assume order : colmajor, else not take into account */
    kaapi_assert(order == CblasColMajor);
    xkblas_zgemm_async(transa, transb, M, N, K,
      (Complex64_t*)alpha, (Complex64_t*)A, lda,
                           (Complex64_t*)B, ldb,
      (Complex64_t*)beta,  (Complex64_t*)C, ldc);
    xkblas_memory_coherent_async(0, 0, M, N, C, ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/*
*/
extern void BLAS_NAME(c,gemm)(
    const char * transa, const char * transb,
    const int * m, const int * n, const int * k,
    const float complex* alpha, const float complex* A, const int * lda,
    const float complex * B, const int * ldb, const float complex * beta,
    float complex * C, const int * ldc)
{
  if (*m == 0 || *n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_CGEMM(*m,*n,*k)/DATA_CGEMM(*m,*n,*k) < threshold_kern[CGEMM])
  {
    if (cblas_cgemm_dl ==0) xkblas_load_sym((void**)&cblas_cgemm_dl,"cblas_cgemm");
    cblas_cgemm_dl(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *m, *n, *k,
                alpha, A, *lda,
                       B, *ldb,
                beta,  C, *ldc);
  }
  else {
    xkblas_cgemm_async(xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb), *m, *n, *k,
      (Complex32_t*)alpha, (Complex32_t*)A, *lda,
                           (Complex32_t*)B, *ldb,
      (Complex32_t*)beta,  (Complex32_t*)C, *ldc);
    xkblas_memory_coherent_async(0,0,*m, *n, C, *ldc, sizeof(Complex32_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/* cblas variant
*/
void cblas_cgemm(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  const Complex32_t *alpha, const Complex32_t *A, int lda,
                            const Complex32_t *B, int ldb,
  const Complex32_t *beta,  Complex32_t *C, int ldc )
{
  if (M == 0 || N == 0 ||
    ((*(Complex32_t*)alpha == 0.0 || K == 0) && *(Complex32_t*)beta == 1.0))
    return;

  if (FLOPS_CGEMM(M,N,K)/DATA_CGEMM(M,N,K) < threshold_kern[CGEMM])
  {
    if (cblas_cgemm_dl ==0) xkblas_load_sym((void**)&cblas_cgemm_dl,"cblas_cgemm");
    cblas_cgemm_dl(order, transa, transb,
                M, N, K,
                alpha, A, lda,
                       B, ldb,
                beta,  C, ldc);
  }
  else {
    /* assume order : colmajor, else not take into account */
    kaapi_assert(order == CblasColMajor);
    xkblas_cgemm_async(transa, transb, M, N, K,
      (Complex32_t*)alpha, (Complex32_t*)A, lda,
                           (Complex32_t*)B, ldb,
      (Complex32_t*)beta,  (Complex32_t*)C, ldc);
    xkblas_memory_coherent_async(0, 0, M, N, C, ldc, sizeof(Complex32_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/*
*/
extern void BLAS_NAME(d,gemm)(
    const char * transa, const char * transb,
    const int * m, const int * n, const int * k,
    const double* alpha, const double* A, const int * lda,
    const double * B, const int * ldb, const double * beta,
    double * C, const int * ldc)
{
  if (*m == 0 || *n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_DGEMM(*m,*n,*k)/DATA_DGEMM(*m,*n,*k) < threshold_kern[DGEMM])
  {
    if (cblas_dgemm_dl ==0) xkblas_load_sym((void**)&cblas_dgemm_dl,"cblas_dgemm");
    cblas_dgemm_dl(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *m, *n, *k,
                (double)*alpha, (double *)A, *lda,
                                (double *)B, *ldb,
                (double)*beta,  (double *)C, *ldc);
  }
  else {
    xkblas_dgemm_async(xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb), *m, *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(double));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/* cblas variant
*/
void cblas_dgemm(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  double alpha, const double *A, int lda,
                const double *B, int ldb,
  double beta,  double *C, int ldc )
{
  if (M == 0 || N == 0 ||
    ((alpha == 0.0 || K == 0) && beta == 1.0))
    return;

  if (FLOPS_DGEMM(M,N,K)/DATA_DGEMM(M,N,K) < threshold_kern[DGEMM])
  {
    if (cblas_dgemm_dl ==0) xkblas_load_sym((void**)&cblas_dgemm_dl,"cblas_dgemm");
    cblas_dgemm_dl(order, transa, transb,
                M, N, K,
                alpha, (double*)A, lda,
                       (double*)B, ldb,
                beta,  (double*)C, ldc);
  }
  else {
    /* assume order : colmajor, else not take into account */
    kaapi_assert(order == CblasColMajor);
    xkblas_dgemm_async(transa, transb, M, N, K,
      &alpha, A, lda,
             B, ldb,
      &beta,  C, ldc);
    xkblas_memory_coherent_async(0, 0, M, N, C, ldc, sizeof(double));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/*
*/
extern void BLAS_NAME(s,gemm)(
    const char * transa, const char * transb,
    const int * m, const int * n, const int * k,
    const float* alpha, const float* A, const int * lda,
    const float * B, const int * ldb, const float * beta,
    float * C, const int * ldc)
{
  if (*m == 0 || *n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_SGEMM(*m,*n,*k)/DATA_SGEMM(*m,*n,*k) < threshold_kern[SGEMM])
  {
    if (cblas_sgemm_dl ==0) xkblas_load_sym((void**)&cblas_sgemm_dl,"cblas_sgemm");
    cblas_sgemm_dl(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *m, *n, *k,
                (float)*alpha, (float *)A, *lda,
                               (float *)B, *ldb,
                (float)*beta,  (float *)C, *ldc);
  }
  else {
    xkblas_sgemm_async(xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb), *m, *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(float));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/* cblas variant
*/
void cblas_sgemm(
  CBLAS_LAYOUT order, CBLAS_TRANSPOSE transa, CBLAS_TRANSPOSE transb,
  int M, int N, int K,
  float alpha, const float *A, int lda,
               const float *B, int ldb,
  float beta,  float *C, int ldc )
{
  if (M == 0 || N == 0 ||
    ((alpha == 0.0 || K == 0) && beta == 1.0))
    return;

  if (FLOPS_SGEMM(M,N,K)/DATA_SGEMM(M,N,K) < threshold_kern[SGEMM])
  {
    if (cblas_sgemm_dl ==0) xkblas_load_sym((void**)&cblas_sgemm_dl,"cblas_sgemm");
    cblas_sgemm_dl(order, transa, transb,
                M, N, K,
                alpha, (float*)A, lda,
                       (float*)B, ldb,
                beta,  (float*)C, ldc);
  }
  else {
    /* assume order : colmajor, else not take into account */
    kaapi_assert(order == CblasColMajor);
    xkblas_sgemm_async(transa, transb, M, N, K,
      &alpha, A, lda,
             B, ldb,
      &beta,  C, ldc);
    xkblas_memory_coherent_async(0, 0, M, N, C, ldc, sizeof(float));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/* ======================================================================================== */
/*
*/
extern void BLAS_NAME(z,gemmt)(
    const char* uplo, const char * transa, const char * transb,
    const int * n, const int * k,
    const double complex* alpha, const double complex* A, const int * lda,
    const double complex * B, const int * ldb, const double complex * beta,
    double complex * C, const int * ldc)
{
  if (*n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_ZGEMMT(*n,*n,*k)/DATA_ZGEMMT(*n,*n,*k) < threshold_kern[ZGEMMT])
  {
#if defined(KAAPI_BLAS_USE_MKL)
    if (cblas_zgemmt_dl ==0) xkblas_load_sym((void**)&cblas_zgemmt_dl,"cblas_zgemmt");
    cblas_zgemmt_dl(CblasColMajor,
                xkblas_blas2cblas_fill(uplo),
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *k,
                (double complex *)alpha, (double complex *)A, *lda,
                (double complex *)B, *ldb,
                (double complex *)beta,  (double complex *)C, *ldc);
#else
    if (cblas_zgemm_dl ==0) xkblas_load_sym((void**)&cblas_zgemm_dl,"cblas_zgemm");
    cblas_zgemm(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *n, *k,
                (double complex *)alpha, (double complex *)A, *lda,
                (double complex *)B, *ldb,
                (double complex *)beta,  (double complex *)C, *ldc);
#endif
  }
  else {
    xkblas_zgemmt_async(xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
      *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo),0, *n, *n, C, *ldc, sizeof(double complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/*
*/
extern void BLAS_NAME(c,gemmt)(
    const char* uplo, const char * transa, const char * transb,
    const int * n, const int * k,
    const float complex* alpha, const float complex* A, const int * lda,
    const float complex * B, const int * ldb, const float complex * beta,
    float complex * C, const int * ldc)
{
  if (*n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_CGEMMT(*n,*n,*k)/DATA_CGEMMT(*n,*n,*k) < threshold_kern[CGEMMT])
  {
#if defined(KAAPI_BLAS_USE_MKL)
    if (cblas_cgemmt_dl ==0) xkblas_load_sym((void**)&cblas_cgemmt_dl,"cblas_cgemmt");
    cblas_cgemmt_dl(CblasColMajor,
                xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *k,
                (float complex *)alpha, (float complex *)A, *lda,
                (float complex *)B, *ldb,
                (float complex *)beta,  (float complex *)C, *ldc);
#else
    if (cblas_cgemm_dl ==0) xkblas_load_sym((void**)&cblas_cgemm_dl,"cblas_cgemm");
    cblas_cgemm_dl(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *n, *k,
                (float complex *)alpha, (float complex *)A, *lda,
                (float complex *)B, *ldb,
                (float complex *)beta,  (float complex *)C, *ldc);
#endif
  }
  else {
    xkblas_cgemmt_async(xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
      *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo), 0, *n, *n, C, *ldc, sizeof(float complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/*
*/
extern void BLAS_NAME(d,gemmt)(
    const char* uplo, const char * transa, const char * transb,
    const int * n, const int * k,
    const double* alpha, const double* A, const int * lda,
    const double * B, const int * ldb, const double * beta,
    double * C, const int * ldc)
{
  if (*n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_DGEMMT(*n,*n,*k)/DATA_DGEMMT(*n,*n,*k) < threshold_kern[DGEMMT])
  {
#if defined(KAAPI_BLAS_USE_MKL)
    if (cblas_dgemmt_dl ==0) xkblas_load_sym((void**)&cblas_dgemmt_dl,"cblas_dgemmt");
    cblas_dgemmt_dl(CblasColMajor,
                xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *k,
                (double )*alpha, (double *)A, *lda,
                (double *)B, *ldb,
                (double )*beta,  (double *)C, *ldc);
#else
    if (cblas_dgemm_dl ==0) xkblas_load_sym((void**)&cblas_dgemm_dl,"cblas_dgemm");
    cblas_dgemm_dl(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *n, *k,
                (double )*alpha, (double *)A, *lda,
                (double *)B, *ldb,
                (double )*beta,  (double *)C, *ldc);
#endif
  }
  else {
    xkblas_dgemmt_async(xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
      *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo), 0, *n, *n, C, *ldc, sizeof(double));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/*
*/
extern void BLAS_NAME(s,gemmt)(
    const char* uplo, const char * transa, const char * transb,
    const int * n, const int * k,
    const float* alpha, const float* A, const int * lda,
    const float * B, const int * ldb, const float * beta,
    float * C, const int * ldc)
{
  if (*n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_SGEMMT(*n,*n,*k)/DATA_SGEMMT(*n,*n,*k) < threshold_kern[SGEMMT])
  {
#if defined(KAAPI_BLAS_USE_MKL)
    if (cblas_sgemmt_dl ==0) xkblas_load_sym((void**)&cblas_sgemmt_dl,"cblas_sgemmt");
    cblas_sgemmt_dl(CblasColMajor,
                xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *k,
                (float )*alpha, (float *)A, *lda,
                (float *)B, *ldb,
                (float )*beta,  (float *)C, *ldc);
#else
    if (cblas_sgemm_dl ==0) xkblas_load_sym((void**)&cblas_sgemm_dl,"cblas_sgemm");
    cblas_sgemm_dl(CblasColMajor,
                xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
                *n, *n, *k,
                (float )*alpha, (float *)A, *lda,
                (float *)B, *ldb,
                (float )*beta,  (float *)C, *ldc);
#endif
  }
  else {
    xkblas_sgemmt_async(xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
      *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo), 0, *n, *n, C, *ldc, sizeof(float));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/* ======================================================================================== */

/*
*/
extern void BLAS_NAME(z,trsm)(
    const char * side, const char * uplo, const char* transa, const char* diag,
    const int * m, const int * n,
    const double complex * alpha,
    const double complex * A, const int * lda,
    double complex * B, const int * ldb )
{
  if (FLOPS_ZTRSM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_ZTRSM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[ZTRSM])
  {
    if (cblas_ztrsm_dl ==0) xkblas_load_sym((void**)&cblas_ztrsm_dl,"cblas_ztrsm");
    cblas_ztrsm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
  }
  else {
    xkblas_ztrsm_async(xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(double complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}


/*
*/
extern void BLAS_NAME(c,trsm)(
    const char * side, const char * uplo, const char* transa, const char* diag,
    const int * m, const int * n,
    const float complex * alpha,
    const float complex * A, const int * lda,
    float complex * B, const int * ldb )
{
  if (FLOPS_CTRSM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_CTRSM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[CTRSM])
  {
    if (cblas_ctrsm_dl ==0) xkblas_load_sym((void**)&cblas_ctrsm_dl,"cblas_ctrsm");
    cblas_ctrsm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
  }
  else {
    xkblas_ctrsm_async(xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(float complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}


/*
*/
extern void BLAS_NAME(d,trsm)(
    const char * side, const char * uplo, const char* transa, const char* diag,
    const int * m, const int * n,
    const double * alpha,
    const double * A, const int * lda,
    double * B, const int * ldb )
{
  if (FLOPS_DTRSM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_DTRSM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[DTRSM])
  {
    if (cblas_dtrsm_dl ==0) xkblas_load_sym((void**)&cblas_dtrsm_dl,"cblas_dtrsm");
    cblas_dtrsm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
              B, *ldb
    );
  }
  else {
    xkblas_dtrsm_async(xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(double));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}


/*
*/
extern void BLAS_NAME(s,trsm)(
    const char * side, const char * uplo, const char* transa, const char* diag,
    const int * m, const int * n,
    const float * alpha,
    const float * A, const int * lda,
    float * B, const int * ldb )
{
  if (FLOPS_STRSM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_STRSM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[STRSM])
  {
    if (cblas_strsm_dl ==0) xkblas_load_sym((void**)&cblas_strsm_dl,"cblas_strsm");
    cblas_strsm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
              B, *ldb
    );
  }
  else {
    xkblas_strsm_async(xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(float));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}

/* ======================================================================================== */
/*
*/
extern void BLAS_NAME(z,trmm)(
  char * side, char *uplo, char *transa, char * diag,
  int *m, int * n,
  double complex*alpha,  double complex *A, int *lda, double complex *B, int *ldb
)
{
  if (FLOPS_ZTRMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_ZTRMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[ZTRMM])
  {
    if (cblas_ztrmm_dl ==0) xkblas_load_sym((void**)&cblas_ztrmm_dl,"cblas_ztrmm");
    cblas_ztrmm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
  }
  else {
    xkblas_ztrmm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(double complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

/*
*/
extern void BLAS_NAME(c,trmm)(
  char * side, char *uplo, char *transa, char * diag,
  int *m, int * n,
  float complex *alpha,  float complex *A, int *lda, float complex *B, int *ldb
)
{
  if (FLOPS_CTRMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_CTRMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[CTRMM])
  {
    if (cblas_ctrmm_dl ==0) xkblas_load_sym((void**)&cblas_ctrmm_dl,"cblas_ctrmm");
    cblas_ctrmm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
  }
  else {
    xkblas_ctrmm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(float complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

/*
*/
extern void BLAS_NAME(d,trmm)(
  char * side, char *uplo, char *transa, char * diag,
  int *m, int * n,
  double *alpha,  double *A, int *lda, double *B, int *ldb
)
{
  if (FLOPS_DTRMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_DTRMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[DTRMM])
  {
    if (cblas_dtrmm_dl ==0) xkblas_load_sym((void**)&cblas_dtrmm_dl,"cblas_dtrmm");
    cblas_dtrmm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb
    );
  }
  else {
    xkblas_dtrmm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(double));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

/*
*/
extern void BLAS_NAME(s,trmm)(
  char * side, char *uplo, char *transa, char * diag,
  int *m, int * n,
  float * alpha,  float *A, int *lda, float *B, int *ldb
)
{
  if (FLOPS_STRMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_STRMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[STRMM])
  {
    if (cblas_strmm_dl ==0) xkblas_load_sym((void**)&cblas_strmm_dl,"cblas_strmm");
    cblas_strmm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb
    );
  }
  else {
    xkblas_strmm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(float));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}


/* ======================================================================================== */
/*
*/
extern void BLAS_NAME(z,symm)(
  char * side, char * uplo,
  int * m, int * n,
  double complex* alpha, double complex* A, int *lda,
                         double complex* B, int *ldb,
  double complex* beta,  double complex* C, int *ldc
)
{
  if (FLOPS_ZSYMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_ZSYMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[ZSYMM])
  {
    if (cblas_zsymm_dl ==0) xkblas_load_sym((void**)&cblas_zsymm_dl,"cblas_zsymm");
    cblas_zsymm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc
    );
  }
  else {
    xkblas_zsymm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(double complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

extern void BLAS_NAME(c,symm)(
  char * side, char * uplo,
  int * m, int * n,
  float complex* alpha, float complex* A, int *lda,
                        float complex* B, int *ldb,
  float complex* beta,  float complex* C, int *ldc
)
{
  if (FLOPS_CSYMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_CSYMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[CSYMM])
  {
    if (cblas_csymm_dl ==0) xkblas_load_sym((void**)&cblas_csymm_dl,"cblas_csymm");
    cblas_csymm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc
    );
  }
  else {
    xkblas_csymm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(float complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

extern void BLAS_NAME(d,symm)(
  char * side, char * uplo,
  int * m, int * n,
  double* alpha, double* A, int *lda,
                 double* B, int *ldb,
  double* beta,  double* C, int *ldc
)
{
  if (FLOPS_DSYMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_DSYMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[DSYMM])
  {
    if (cblas_dsymm_dl ==0) xkblas_load_sym((void**)&cblas_dsymm_dl,"cblas_dsymm");
    cblas_dsymm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb,
      *beta,  C, *ldc
    );
  }
  else {
    xkblas_dsymm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(double));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

extern void BLAS_NAME(s,symm)(
  char * side, char * uplo,
  int * m, int * n,
  float* alpha, float* A, int *lda,
                float* B, int *ldb,
  float* beta,  float* C, int *ldc
)
{
  if (FLOPS_SSYMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_SSYMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[SSYMM])
  {
    if (cblas_ssymm_dl ==0) xkblas_load_sym((void**)&cblas_ssymm_dl,"cblas_ssymm");
    cblas_ssymm_dl( CblasColMajor,
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb,
      *beta,  C, *ldc
    );
  }
  else {
    xkblas_ssymm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      *alpha, A, *lda,
             B, *ldb,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(float));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}



/* ======================================================================================== */
/*
*/
extern void BLAS_NAME(z,syrk)(
  char * uplo, char * transa,
  int *n, int *k,
  double complex *alpha, double complex *A, int* lda,
  double complex *beta,  double complex *C, int* ldc)
{
  if (FLOPS_ZSYRK(*n,*k)/ DATA_ZSYRK(*n,*k) < threshold_kern[ZSYRK])
  {
    if (cblas_zsyrk_dl ==0) xkblas_load_sym((void**)&cblas_zsyrk_dl,"cblas_zsyrk");
    cblas_zsyrk_dl( CblasColMajor,
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      alpha, A, *lda,
      beta,  C, *ldc
    );
  }
  else {
    xkblas_zsyrk_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      *alpha, A, *lda,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *n, *n, C, *ldc, sizeof(double complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

extern void BLAS_NAME(c,syrk)(
  char * uplo, char * transa,
  int *n, int *k,
  float complex*alpha, float complex*A, int* lda,
  float complex*beta,  float complex*C, int* ldc)
{
  if (FLOPS_CSYRK(*n,*k)/ DATA_CSYRK(*n,*k) < threshold_kern[CSYRK])
  {
    if (cblas_csyrk_dl ==0) xkblas_load_sym((void**)&cblas_csyrk_dl,"cblas_csyrk");
    cblas_csyrk_dl( CblasColMajor,
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      alpha, A, *lda,
      beta,  C, *ldc
    );
  }
  else {
    xkblas_csyrk_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      *alpha, A, *lda,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *n, *n, C, *ldc, sizeof(float complex));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

extern void BLAS_NAME(d,syrk)(
  char * uplo, char * transa,
  int *n, int *k,
  double *alpha, double *A, int* lda,
  double *beta,  double *C, int* ldc)
{
  if (FLOPS_DSYRK(*n,*k)/ DATA_DSYRK(*n,*k) < threshold_kern[DSYRK])
  {
    if (cblas_dsyrk_dl ==0) xkblas_load_sym((void**)&cblas_dsyrk_dl,"cblas_dsyrk");
    cblas_dsyrk_dl( CblasColMajor,
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      *alpha, A, *lda,
      *beta,  C, *ldc
    );
  }
  else {
    xkblas_dsyrk_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      *alpha, A, *lda,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *n, *n, C, *ldc, sizeof(double));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

extern void BLAS_NAME(s,syrk)(
  char * uplo, char * transa,
  int *n, int *k,
  float *alpha, float *A, int* lda,
  float *beta,  float *C, int* ldc)
{
  if (FLOPS_SSYRK(*n,*k)/ DATA_SSYRK(*n,*k) < threshold_kern[SSYRK])
  {
    if (cblas_ssyrk_dl ==0) xkblas_load_sym((void**)&cblas_ssyrk_dl,"cblas_ssyrk");
    cblas_ssyrk_dl( CblasColMajor,
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      *alpha, A, *lda,
      *beta,  C, *ldc
    );
  }
  else {
    xkblas_ssyrk_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      *alpha, A, *lda,
      *beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *n, *n, C, *ldc, sizeof(float));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}
