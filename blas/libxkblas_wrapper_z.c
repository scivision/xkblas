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
#include "libxkblas_wrapper.h"

/* Pointer for cblas_Xgemm to the loaded BLAS library
*/
/* gemm */
static void (*dl_zgemm)(
    const char * transa, const char * transb,
    const int * m, const int * n, const int * k,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              const Complex64_t * B, const int * ldb,
    const Complex64_t* beta,  Complex64_t * C, const int * ldc) = 0;


/* gemmt */
static void (*dl_zgemmt)(
    const char* uplo, const char * transa, const char * transb,
    const int * n, const int * k,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              const Complex64_t * B, const int * ldb,
    const Complex64_t* beta,  Complex64_t * C, const int * ldc) = 0;


/* trsm */
static void (*dl_ztrsm)(
    const char * side, const char *uplo, const char* transa, const char* diag,
    const int* m, const int* n,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              Complex64_t* B, const int * ldb ) = 0;

/* trmm */
static void (*dl_ztrmm)(
  const char* side, const char* uplo, const char* transa, const char* diag,
  const int* m, const int* n,
  const Complex64_t*alpha, const Complex64_t *A, const int *lda,
                           Complex64_t *B, const int* ldb ) = 0;


/* symm */
static void (*dl_zsymm)(
  const char* side, const char* uplo,
  const int* m, const int* n,
  const Complex64_t* alpha, const Complex64_t* A, const int *lda,
                            const Complex64_t* B, const int *ldb,
  const Complex64_t* beta,  Complex64_t* C, const int *ldc ) = 0;


/* syrk */
static void (*dl_zsyrk)(
  const char * uplo, const char * transa,
  const int *n, const int *k,
  const Complex64_t *alpha, const Complex64_t *A, const int* lda,
  const Complex64_t *beta,  Complex64_t *C, const int* ldc) = 0;


/* syr2k */
static void (*dl_zsyr2k)(
  const char* uplo, const char* transa,
  const int* n, const int* k,
  const Complex64_t *alpha, const Complex64_t *A, const int* lda,
                            const Complex64_t *B, const int* ldb,
  const Complex64_t *beta,  Complex64_t *C, const int* ldc) = 0;

#if defined(PRECISION_z)||defined(PRECISION_c)
/* hemm */
static void (*dl_zhemm)(
  const char* side, const char* uplo,
  const int* m, const int* n,
  const Complex64_t* alpha, const Complex64_t* A, const int *lda,
                            const Complex64_t* B, const int *ldb,
  const Complex64_t* beta,  Complex64_t* C, const int *ldc ) = 0;

/* herk */
static void (*dl_zherk)(
  char * uplo, char * transa,
  int *n, int *k,
  const CFloat64_t *alpha, const Complex64_t *A, const int* lda,
  const CFloat64_t *beta,  Complex64_t *C, const int* ldc) = 0;

/* her2k */
static void (*dl_zher2k)(
  const char* uplo, const char* transa,
  const int* n, const int* k,
  const Complex64_t* alpha, const Complex64_t* A, const int* lda,
                            const Complex64_t* B, const int* ldb,
  const CFloat64_t* beta,   Complex64_t* C, const int* ldc) = 0;
#endif



/* ================================  GEMM  =============================================== */
extern void CATSTR(_xkblas_,zgemm)(
    const char * transa, const char * transb,
    const int * m, const int * n, const int * k,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              const Complex64_t * B, const int * ldb,
    const Complex64_t* beta,  Complex64_t * C, const int * ldc)
{
  printf("In: %s\n",__func__);
  if (dl_zgemm ==0) xkblas_load_sym((void**)&dl_zgemm,SYMBLAS_NAME(zgemm));
  dl_zgemm( transa, transb,
            m, n, k,
            alpha, A, lda,
                   B, ldb,
            beta,  C, ldc);
  printf("Out: %s\n",__func__);
}

/* F77 name
*/
extern void BLAS_NAME(zgemm)(
    const char * transa, const char * transb,
    const int * m, const int * n, const int * k,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              const Complex64_t * B, const int * ldb,
    const Complex64_t* beta,  Complex64_t * C, const int * ldc)
{
  if (*m == 0 || *n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  printf("In: %s\n",__func__);
  if (FLOPS_ZGEMM(*m,*n,*k)/DATA_ZGEMM(*m,*n,*k) < threshold_kern[ZGEMM])
  {
    CATSTR(_xkblas_,zgemm)( transa, transb,
                          m, n, k,
                          alpha, A, lda,
                                 B, ldb,
                          beta,  C, ldc);
  }
  else {
    xkblas_zgemm_async(xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb), *m, *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
  printf("Out: %s\n",__func__);
}





/* ================================  GEMMT  =============================================== */
extern void CATSTR(_xkblas_,zgemmt)(
    const char* uplo, const char * transa, const char * transb,
    const int * n, const int * k,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              const Complex64_t * B, const int * ldb,
    const Complex64_t* beta,  Complex64_t * C, const int * ldc)
{
#if defined(KAAPI_BLAS_USE_MKL)
  if (dl_zgemmt ==0) xkblas_load_sym((void**)&dl_zgemmt,SYMBLAS_NAME(zgemmt));
  dl_zgemmt( uplo, transa, transb,
             n, k,
             alpha, A, lda,
                    B, ldb,
             sbeta, C, ldc);
#else
  if (dl_zgemm ==0) xkblas_load_sym((void**)&dl_zgemm,SYMBLAS_NAME(zgemm));
  dl_zgemm( transa, transb,
            n, n, k,
            alpha, A, lda,
                   B, ldb,
            beta,  C, ldc);
#endif
}

/* F77 name
*/
extern void BLAS_NAME(zgemmt)(
    const char* uplo, const char * transa, const char * transb,
    const int * n, const int * k,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              const Complex64_t * B, const int * ldb,
    const Complex64_t* beta,  Complex64_t * C, const int * ldc)
{
  if (*n == 0 ||
    ((*alpha == 0.0 || *k == 0) && *beta == 1.0))
    return;

  if (FLOPS_ZGEMMT(*n,*n,*k)/DATA_ZGEMMT(*n,*n,*k) < threshold_kern[ZGEMMT])
  {
    CATSTR(_xkblas_,zgemmt)( uplo, transa, transb,
               n, k,
               alpha, A, lda,
                      B, ldb,
               beta, C, ldc);
  }
  else {
    xkblas_zgemmt_async(xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_trans(transb),
      *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc);
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo),0, *n, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
  }
}



/* ================================  TRSM  =============================================== */
extern void CATSTR(_xkblas_,ztrsm)(
    const char * side, const char *uplo, const char* transa, const char* diag,
    const int* m, const int* n,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              Complex64_t* B, const int * ldb )
{
  if (dl_ztrsm ==0) xkblas_load_sym((void**)&dl_ztrsm,SYMBLAS_NAME(ztrsm));
  dl_ztrsm( side, uplo, transa, diag,
            m, n,
            alpha, A, lda,
                   B, ldb
  );
}

/* F77 name
*/
extern void BLAS_NAME(ztrsm)(
    const char * side, const char *uplo, const char* transa, const char* diag,
    const int* m, const int* n,
    const Complex64_t* alpha, const Complex64_t* A, const int * lda,
                              Complex64_t* B, const int * ldb )
{
  if (FLOPS_ZTRSM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_ZTRSM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[ZTRSM])
  {
    CATSTR(_xkblas_,ztrsm)( side, uplo, transa, diag,
              m, n,
              alpha, A, lda,
                     B, ldb
    );
  }
  else {
    xkblas_ztrsm_async(xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}



/* ================================  TRMM  =============================================== */
extern void CATSTR(_xkblas_,ztrmm)(
  const char * side, const char *uplo, const char *transa, const char * diag,
  const int *m, const int * n,
  const Complex64_t* alpha,  const Complex64_t *A, const int *lda,
                            Complex64_t *B, const int *ldb
)
{
  if (dl_ztrmm ==0) xkblas_load_sym((void**)&dl_ztrmm,SYMBLAS_NAME(ztrmm));
  dl_ztrmm( side, uplo, transa, diag,
            m, n,
            alpha, A, lda,
                   B, ldb
  );
}

/* F77 name
*/
extern void BLAS_NAME(ztrmm)(
  const char * side, const char *uplo, const char *transa, const char * diag,
  const int *m, const int * n,
  const Complex64_t* alpha,  const Complex64_t *A, const int *lda,
                            Complex64_t *B, const int *ldb
)
{
  if (FLOPS_ZTRMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_ZTRMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[ZTRMM])
  {
    CATSTR(_xkblas_,ztrmm)( side, uplo, transa, diag,
              m, n,
              alpha, A, lda,
                     B, ldb
    );
  }
  else {
    xkblas_ztrmm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      xkblas_blas2cblas_trans(transa), xkblas_blas2cblas_diag(diag),
      *m, *n,
      alpha, A, *lda,
             B, *ldb
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, B, *ldb, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}



/* ================================  SYMM  =============================================== */
extern void CATSTR(_xkblas_,zsymm)(
  const char * side, const char * uplo,
  const int * m, const int * n,
  const Complex64_t* alpha, const Complex64_t* A, const int *lda,
                            const Complex64_t* B, const int *ldb,
  const Complex64_t* beta,  Complex64_t* C, const int *ldc
)
{
  if (dl_zsymm ==0) xkblas_load_sym((void**)&dl_zsymm,SYMBLAS_NAME(zsymm));
  dl_zsymm( side, uplo,
            m, n,
            alpha, A, lda,
                   B, ldb,
            beta,  C, ldc
  );
}

/* F77 name
*/
extern void BLAS_NAME(zsymm)(
  const char * side, const char * uplo,
  const int * m, const int * n,
  const Complex64_t* alpha, const Complex64_t* A, const int *lda,
                            const Complex64_t* B, const int *ldb,
  const Complex64_t* beta,  Complex64_t* C, const int *ldc
)
{
  if (FLOPS_ZSYMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_ZSYMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[ZSYMM])
  {
    CATSTR(_xkblas_,zsymm)( side, uplo,
              m, n,
              alpha, A, lda,
                     B, ldb,
              beta,  C, ldc
    );
  }
  else {
    xkblas_zsymm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}



/* ================================  SYRK  =============================================== */
extern void CATSTR(_xkblas_,zsyrk)(
  const char * uplo, const char * transa,
  const int *n, const int *k,
  const Complex64_t *alpha, const Complex64_t *A, const int* lda,
  const Complex64_t *beta,  Complex64_t *C, const int* ldc)
{
  if (dl_zsyrk ==0) xkblas_load_sym((void**)&dl_zsyrk,SYMBLAS_NAME(zsyrk));
  dl_zsyrk( uplo, transa,
            n, k,
            alpha, A, lda,
            beta,  C, ldc
  );
}

/* F77 name
*/
extern void BLAS_NAME(zsyrk)(
  const char * uplo, const char * transa,
  const int *n, const int *k,
  const Complex64_t *alpha, const Complex64_t *A, const int* lda,
  const Complex64_t *beta,  Complex64_t *C, const int* ldc)
{
  if (FLOPS_ZSYRK(*n,*k)/ DATA_ZSYRK(*n,*k) < threshold_kern[ZSYRK])
  {
    CATSTR(_xkblas_,zsyrk)( uplo, transa,
              n, k,
              alpha, A, lda,
              beta,  C, ldc
    );
  }
  else {
    xkblas_zsyrk_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      alpha, A, *lda,
      beta,  C, *ldc
    );
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo), 0, *n, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}


/* ================================  SYR2K  =============================================== */
extern void CATSTR(_xkblas_,zsyr2k)(
  const char * uplo, const char * transa,
  const int *n, const int *k,
  const Complex64_t *alpha, const Complex64_t *A, const int* lda,
                            const Complex64_t *B, const int* ldb,
  const Complex64_t *beta,  Complex64_t *C, const int* ldc)
{
  if (dl_zsyr2k ==0) xkblas_load_sym((void**)&dl_zsyr2k,SYMBLAS_NAME(zsyr2k));
  dl_zsyr2k( uplo, transa,
             n, k,
             alpha, A, lda,
                    B, ldb,
             beta,  C, ldc
  );
}

/* F77 name
*/
extern void BLAS_NAME(zsyr2k)(
  const char * uplo, const char * transa,
  const int *n, const int *k,
  const Complex64_t *alpha, const Complex64_t *A, const int* lda,
                            const Complex64_t *B, const int* ldb,
  const Complex64_t *beta,  Complex64_t *C, const int* ldc)
{
  if (FLOPS_ZSYR2K(*n,*k)/ DATA_ZSYR2K(*n,*k) < threshold_kern[ZSYR2K])
  {
    CATSTR(_xkblas_,zsyr2k)( uplo, transa,
               n, k,
               alpha, A, lda,
                      B, ldb,
               beta,  C, ldc
    );
  }
  else {
    xkblas_zsyr2k_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc
    );
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo), 0, *n, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}



#if defined(PRECISION_z)||defined(PRECISION_c)
/* ================================  HEMM  =============================================== */
extern void CATSTR(_xkblas_,zhemm)(
  const char * side, const char * uplo,
  const int * m, const int * n,
  const Complex64_t* alpha, const Complex64_t* A, const int *lda,
                            const Complex64_t* B, const int *ldb,
  const Complex64_t* beta,  Complex64_t* C, const int *ldc
)
{
  if (dl_zhemm ==0) xkblas_load_sym((void**)&dl_zhemm,SYMBLAS_NAME(zsymm));
  dl_zhemm( side, uplo,
            m, n,
            alpha, A, lda,
                   B, ldb,
            beta,  C, ldc
  );
}

/* F77 name
*/
extern void BLAS_NAME(zhemm)(
  const char * side, const char * uplo,
  const int * m, const int * n,
  const Complex64_t* alpha, const Complex64_t* A, const int *lda,
                            const Complex64_t* B, const int *ldb,
  const Complex64_t* beta,  Complex64_t* C, const int *ldc
)
{
  if (FLOPS_ZHEMM(xkblas_blas2cblas_side(side),*m,*n)
     / DATA_ZHEMM(xkblas_blas2cblas_side(side),*m,*n) < threshold_kern[ZHEMM])
  {
    CATSTR(_xkblas_,zhemm)( side, uplo,
              m, n,
              alpha, A, lda,
                     B, ldb,
              beta,  C, ldc
    );
  }
  else {
    xkblas_zhemm_async(
      xkblas_blas2cblas_side(side), xkblas_blas2cblas_fill(uplo),
      *m, *n,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc
    );
    xkblas_memory_coherent_async(0, 0, *m, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}



/* ================================  HERK  =============================================== */
extern void CATSTR(_xkblas_,zherk)(
  char * uplo, char * transa,
  int *n, int *k,
  CFloat64_t *alpha, Complex64_t *A, int* lda,
  CFloat64_t *beta,  Complex64_t *C, int* ldc)
{
  if (dl_zherk ==0) xkblas_load_sym((void**)&dl_zherk,SYMBLAS_NAME(zherk));
  dl_zherk( uplo, transa,
            n, k,
            alpha, A, lda,
            beta,  C, ldc
  );
}

/* F77 name
*/
extern void BLAS_NAME(zherk)(
  char * uplo, char * transa,
  int *n, int *k,
  CFloat64_t *alpha, Complex64_t *A, int* lda,
  CFloat64_t *beta,  Complex64_t *C, int* ldc)
{
  if (FLOPS_ZHERK(*n,*k)/ DATA_ZHERK(*n,*k) < threshold_kern[ZHERK])
  {
    CATSTR(_xkblas_,zherk)( uplo, transa,
              n, k,
              alpha, A, lda,
              beta,  C, ldc
    );
  }
  else {
    xkblas_zherk_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      alpha, A, *lda,
      beta,  C, *ldc
    );
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo), 0, *n, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}



/* ================================  HER2K  =============================================== */
extern void CATSTR(_xkblas_,zher2k)(
  char * uplo, char * transa,
  int *n, int *k,
  Complex64_t *alpha, Complex64_t *A, int* lda,
                      Complex64_t *B, int* ldb,
  CFloat64_t *beta,   Complex64_t *C, int* ldc)
{
  if (dl_zher2k ==0) xkblas_load_sym((void**)&dl_zher2k,SYMBLAS_NAME(zher2k));
  dl_zher2k( uplo, transa,
             n, k,
             alpha, A, lda,
                    B, ldb,
             beta,  C, ldc
  );
}

/* F77 name
*/
extern void BLAS_NAME(zher2k)(
  char * uplo, char * transa,
  int *n, int *k,
  Complex64_t *alpha, Complex64_t *A, int* lda,
                      Complex64_t *B, int* ldb,
  CFloat64_t *beta,   Complex64_t *C, int* ldc)
{
  if (FLOPS_ZHER2K(*n,*k)/ DATA_ZHER2K(*n,*k) < threshold_kern[ZHER2K])
  {
    CATSTR(_xkblas_,zher2k)( uplo, transa,
               n, k,
               alpha, A, lda,
                      B, ldb,
               beta,  C, ldc
    );
  }
  else {
    xkblas_zher2k_async(
      xkblas_blas2cblas_fill(uplo), xkblas_blas2cblas_trans(transa),
      *n, *k,
      alpha, A, *lda,
             B, *ldb,
      beta,  C, *ldc
    );
    xkblas_memory_coherent_async(xkblas_blas2cblas_fill(uplo), 0, *n, *n, C, *ldc, sizeof(Complex64_t));
    xkblas_sync();
    xkblas_memory_invalidate_caches();
 }
}

#endif
