/**
 *
 * @file zgemm.c
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon zgemm wrappers
 *
 * @version 1.0.0
 * @comment This file has been automatically generated
 *          from Plasma 2.5.0 for CHAMELEON 1.0.0
 * @author Mathieu Faverge
 * @author Emmanuel Agullo
 * @author Cedric Castagnede
 * @author Thierry Gautier
 * @date 2018-11-20
 * @precisions normal z -> s d c
 * This file was merged from pzgemm and zgemm from Chameleon by Thierry Gautier
 * for Kaapi that support natively 2D memory view.
 */
#include "common.h"
#include "ztask.h"
#include "ztask_internal.h"
#include <math.h>
#include <string.h>


/**
 ********************************************************************************
 *
 * @ingroup Complex64_t
 *
 *  CHAMELEON_zgemm - Performs one of the matrix-matrix operations
 *
 *    \f[ C = \alpha [op( A )\times op( B )] + \beta C \f],
 *
 *  where op( X ) is one of
 *
 *    op( X ) = X  or op( X ) = X' or op( X ) = conjg( X' )
 *
 *  alpha and beta are scalars, and A, B and C  are matrices, with op( A )
 *  an m by k matrix, op( B ) a k by n matrix and C an m by n matrix.
 *
 *******************************************************************************
 *
 * @param[in] transA
 *          Specifies whether the matrix A is transposed, not transposed or conjugate transposed:
 *          = CblasNoTrans:   A is not transposed;
 *          = CblasTrans:     A is transposed;
 *          = CblasConjTrans: A is conjugate transposed.
 *
 * @param[in] transB
 *          Specifies whether the matrix B is transposed, not transposed or conjugate transposed:
 *          = CblasNoTrans:   B is not transposed;
 *          = CblasTrans:     B is transposed;
 *          = CblasConjTrans: B is conjugate transposed.
 *
 * @param[in] M
 *          M specifies the number of rows of the matrix op( A ) and of the matrix C. M >= 0.
 *
 * @param[in] N
 *          N specifies the number of columns of the matrix op( B ) and of the matrix C. N >= 0.
 *
 * @param[in] K
 *          K specifies the number of columns of the matrix op( A ) and the number of rows of
 *          the matrix op( B ). K >= 0.
 *
 * @param[in] alpha
 *          alpha specifies the scalar alpha
 *
 * @param[in] A
 *          A is a LDA-by-ka matrix, where ka is K when  transA = CblasNoTrans,
 *          and is  M  otherwise.
 *
 * @param[in] LDA
 *          The leading dimension of the array A. LDA >= max(1,M).
 *
 * @param[in] B
 *          B is a LDB-by-kb matrix, where kb is N when  transB = CblasNoTrans,
 *          and is  K  otherwise.
 *
 * @param[in] LDB
 *          The leading dimension of the array B. LDB >= max(1,N).
 *
 * @param[in] beta
 *          beta specifies the scalar beta
 *
 * @param[in,out] C
 *          C is a LDC-by-N matrix.
 *          On exit, the array is overwritten by the M by N matrix ( alpha*op( A )*op( B ) + beta*C )
 *
 * @param[in] LDC
 *          The leading dimension of the array C. LDC >= max(1,M).
 *
 *******************************************************************************
 *
 * @return
 *          \retval 0 successful exit
 *
 *******************************************************************************
 *
 * @sa CHAMELEON_zgemm_Tile
 * @sa CHAMELEON_cgemm
 * @sa CHAMELEON_dgemm
 * @sa CHAMELEON_sgemm
 *
 */

int xkblas_zgemm_async(
    int transA, int transB, int M, int N, int K,
    const Complex64_t* alpha, const Complex64_t *A, int LDA,
                              const Complex64_t *B, int LDB,
    const Complex64_t* beta,        Complex64_t *C, int LDC )
{
#if 0
		printf("[XKBLAS] gemm M %d, N %d, K %d, A %p, LDA %d, B %p, LDB %d, C %p, LDC %d\n",
										M, N, K, A, LDA, B, LDB, C, LDC );
#endif
    /* Check input arguments */
    if ((transA < CblasNoTrans) || (transA > CblasConjTrans)) {
        kaapi_error("zgemm", "illegal value of transA");
        return -1;
    }
    if ((transB < CblasNoTrans) || (transB > CblasConjTrans)) {
        kaapi_error("zgemm", "illegal value of transB");
        return -2;
    }
    if (M < 0) {
        kaapi_error("zgemm",  "illegal value of M");
        return -3;
    }
    if (N < 0) {
        kaapi_error("zgemm", "illegal value of N");
        return -4;
    }
    if (K < 0) {
        kaapi_error("zgemm", "illegal value of N");
        return -5;
    }

    size_t Am, An, Bm, Bn;
    const size_t Cm = M;
    const size_t Cn = N;

    if ( transA == CblasNoTrans ) {
        Am = M; An = K;
    } else {
        Am = K; An = M;
    }
    if ( transB == CblasNoTrans ) {
        Bm = K; Bn = N;
    } else {
        Bm = N; Bn = K;
    }

    if (LDA < kaapi_max(1, Am)) {
        kaapi_error("zgemm", "illegal value of LDA");
        return -8;
    }
    if (LDB < kaapi_max(1, Bm)) {
        kaapi_error("zgemm", "illegal value of LDB");
        return -10;
    }
    if (LDC < kaapi_max(1, M)) {
        kaapi_error("zgemm", "illegal value of LDC");
        return -13;
    }

    /* Quick return */
    if (M == 0 || N == 0 ||
      ((*alpha == 0.0 || K == 0) && *beta == 1.0))
        return 0;

    xkblas_context_t * xkctxt = xkblas_context_get();
    size_t NB = xkblas_auto_tilesize(xkctxt, KERN_GEMM, M, N, K);

    // TODO : set tiling parameters
    size_t Amb = NB;
    size_t Anb = NB;
    size_t Bmb = NB;
    size_t Bnb = NB;
    size_t Cmb = NB;
    size_t Cnb = NB;

    size_t Amt = XKBLAS_NUM_OF_TILES(Am, Amb);
    size_t Ant = XKBLAS_NUM_OF_TILES(An, Anb);
    size_t Bmt = XKBLAS_NUM_OF_TILES(Bm, Bmb);
    size_t Bnt = XKBLAS_NUM_OF_TILES(Bn, Bnb);
    size_t Cmt = XKBLAS_NUM_OF_TILES(Cm, Cmb);
    size_t Cnt = XKBLAS_NUM_OF_TILES(Cn, Cnb);

    size_t bs_mm, bs_nn, bs_kn, bs_km;

    // iterator on tiles
    for (size_t tm = 0; tm < Cmt; ++tm)
    {
        size_t bs_mm = (tm == Cmt-1) ? (M-tm*Cmb) : Cmb;
        for (size_t tn = 0; tn < Cnt; tn++)
        {
            bs_nn = (tn == Cnt-1) ? (N-tn*Cnb) : Cnb;

            // A: CblasNoTrans / B: CblasNoTrans
            if (transA == CblasNoTrans)
            {
                if (transB == CblasNoTrans)
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        Complex64_t zbeta = (tk == 0) ? *beta : 1.0;
                        INSERT_TASK_zgemm_v2(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                *alpha,
                                A, tm, tk, LDA,
                                B, tk, tn, LDB,
                                zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
                // A: CblasNoTrans / B: Cham[Conj]Trans
                else
                {
                    for (size_t tk = 0; tk < Ant; ++tk)
                    {
                        bs_kn = (tk == Ant-1) ? (An-tk*Anb) : Anb;
                        Complex64_t zbeta = (tk == 0) ? *beta : 1.0;
                        INSERT_TASK_zgemm_v2(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                *alpha,
                                A, tm, tk, LDA,
                                B, tn, tk, LDB,
                                zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
            }
            // A: Cham[Conj]Trans / B: CblasNoTrans
            else
            {
                if (transB == CblasNoTrans)
                {
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        Complex64_t zbeta = (tk == 0) ? *beta : 1.0;
                        INSERT_TASK_zgemm_v2(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                *alpha,
                                A, tk, tm, LDA,
                                B, tk, tn, LDB,
                                zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
                // A: Cham[Conj]Trans / B: Cham[Conj]Trans
                else
                {
                    for (size_t tk = 0; tk < Amt; ++tk)
                    {
                        bs_km = (tk == Amt-1) ? (Am-tk*Amb) : Amb;
                        Complex64_t zbeta = (tk == 0) ? *beta : 1.0;
                        INSERT_TASK_zgemm_v2(
                                transA, transB,
                                bs_mm, bs_nn, bs_kn,
                                *alpha,
                                A, tk, tm, LDA,
                                B, tn, tk, LDB,
                                zbeta,
                                C, tm, tn, LDC
                        );
                    }
                }
            }
        }
    }
    return 0;
}


/* gemm native pointer */
static void (*dl_zgemm)(
    const char * transa, const char * transb,
    const KBLAS_INT * m, const KBLAS_INT * n, const KBLAS_INT * k,
    const Complex64_t* alpha, const Complex64_t* A, const KBLAS_INT * lda,
                              const Complex64_t * B, const KBLAS_INT * ldb,
    const Complex64_t* beta,  Complex64_t * C, const KBLAS_INT * ldc) = 0;


/* CPU driver */
extern void xkblas_zgemm_native_(
    const char * transa, const char * transb,
    const KBLAS_INT * m, const KBLAS_INT * n, const KBLAS_INT * k,
    const Complex64_t* alpha, const Complex64_t* A, const KBLAS_INT * lda,
                              const Complex64_t * B, const KBLAS_INT * ldb,
    const Complex64_t* beta,  Complex64_t * C, const KBLAS_INT * ldc)
{
  if (dl_zgemm ==0) xkblas_load_sym((void**)&dl_zgemm,SYMBLAS_NAME(zgemm));
  dl_zgemm( transa, transb,
            m, n, k,
            alpha, A, lda,
                   B, ldb,
            beta,  C, ldc);
}

extern int xkblas_zgemm_native(
  int transA, int transB, int M, int N, int K,
  const Complex64_t* alpha, const Complex64_t *A, int LDA,
  const Complex64_t *B, int LDB,
  const Complex64_t* beta,  Complex64_t *C, int LDC )
{
  char trA = cblas2blas_op(transA);
  char trB = cblas2blas_op(transB);
  const KBLAS_INT iM = M;
  const KBLAS_INT iN = N;
  const KBLAS_INT iK = K;
  const KBLAS_INT iLDA = LDA;
  const KBLAS_INT iLDB = LDB;
  const KBLAS_INT iLDC = LDC;
  xkblas_zgemm_native_( &trA, &trB, &iM, &iN, &iK, alpha, A, &iLDA, B, &iLDB, beta, C, &iLDC );
  return 0;
}
