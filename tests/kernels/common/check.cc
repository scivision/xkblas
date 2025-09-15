/* ************************************************************************** */
/*                                                                            */
/*   check.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/17 22:54:05 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

/* retrived from xkblas original tests */

# include <math.h>
# include <sys/param.h>
# include "common/blas.h"

// TODO : change slange, saxpy, slamch, etc... with defines based on type

TYPED
static void
dump_matrix(
    const char * label,
    const TYPE * M,
    const BLAS_INT m,
    const BLAS_INT n,
    const BLAS_INT ld
) {
    if (m > 64 || n > 64)
        return ;

    printf("---- %s ----\n", label);
    for (int j = 0 ; j < m ; ++j)
        for (int i = 0 ; i < n ; ++i)
            printf("%4.4f%c", M[i*ld+j], (i == n-1) ? '\n' : ' ');
}

TYPED
static void
dump_matrix_diff(
    const char * label,
    const TYPE * A,
    const TYPE * B,
    const BLAS_INT m,
    const BLAS_INT n,
    const BLAS_INT ld
) {
    if (m > 64 || n > 64)
        return ;

    printf("---- %s ----\n", label);
    for (int j = 0 ; j < m ; ++j)
        for (int i = 0 ; i < n ; ++i)
            printf("%4.4f%c", A[i*ld+j] - B[i*ld+j], (i == n-1) ? '\n' : ' ');
}

TYPED
static void
dump_vector(
    const char * label,
    const TYPE * X,
    const BLAS_INT n
) {
    if (n > 64)
        return ;

    printf("---- %s ----\n", label);
    for (int i = 0 ; i < n ; ++i)
        printf("%4.4f\n", X[i]);
}

TYPED
static void
dump_csr_matrix(
    const char * label,
    int m,
    int n,
    int * csr_row_offsets,
    int * csr_col_indices,
    TYPE * csr_values
) {
    if (m > 64 || n > 64)
        return ;

    printf("---- %s ----\n", label);
    for (int i = 0; i < n; ++i)
    {
        int start = csr_row_offsets[i];
        int end = csr_row_offsets[i + 1];
        int idx = start;

        for (int j = 0; j < m; ++j)
        {
            if (idx < end && csr_col_indices[idx] == j)
            {
                printf("%4.4f ", csr_values[idx]);
                ++idx;
            }
            else
            {
                printf("%4.4f ", 0.0);
            }
        }
        printf("\n");
    }
}

TYPED
int
spmv_cpu(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    const int nrows,
    const int ncols,
    const int nnz,
    const int * row_ptr,
    const int * col_idx,
    const TYPE * values,
    /* vector X (in) */
    TYPE * x,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * y,
    TYPE * y_gpu
) {
    int r = 0;

    for (int i = 0; i < nrows; i++) {
        double sum = 0.0;
        for (int jj = row_ptr[i]; jj < row_ptr[i+1]; jj++) {
            sum += values[jj] * x[col_idx[jj]];
        }
        y[i] = *alpha * sum + *beta * y[i];

        // 0.1% precision
        if (100.0 * abs(y[i] - y_gpu[i]) / MAX(abs(y[i]), abs(y_gpu[i])) > 0.1)
        {
            printf("y_cpu[%d] = %lf vs y_gpu[%d] = %lf\n", i , y[i], i, y_gpu[i]);
            return 1;
        }
    }
    return r;
}

/**
 *  Verify the solution of the gemm 'CImpl' result.
 *      1) copy C to CRef
 *      2) perform gemm(A, B, CRef)
 *      3) compare CRef and CImpl
 */
TYPED
int
gemm_cmp(
    CBLAS_TRANSPOSE transA, CBLAS_TRANSPOSE transB,
    const BLAS_INT m, const BLAS_INT n, const BLAS_INT k,
    const TYPE alpha,
    const TYPE * A, const BLAS_INT lda,
    const TYPE * B, const BLAS_INT ldb,
    const TYPE beta,
    const TYPE * C, TYPE * CRef, const TYPE * CImpl, const BLAS_INT ldc,
    int repeat
) {
    /* run native */
    printf("Running native...\n");
    {
        uint64_t t0 = get_nanotime();
        for (int i = 0 ; i < repeat ; ++i)
            native_gemm(CblasColMajor, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, CRef, ldc);
        uint64_t tf = get_nanotime();
        printf("Native took %lf s.\n", (tf - t0) / (double)1e9);
    }

    TYPE * work = (TYPE *) malloc(MAX(k,MAX(m, n)) * sizeof(TYPE));

    const int Am = (transA == CblasNoTrans) ? m : k;
    const int An = (transA == CblasNoTrans) ? k : m;
    const int Bm = (transB == CblasNoTrans) ? k : n;
    const int Bn = (transB == CblasNoTrans) ? n : k;
    const int Cm = Am;
    const int Cn = Bn;

    double Anorm     = native_lange_work(LAPACK_COL_MAJOR, 'I', Am, An, A,     lda, work);
    double Bnorm     = native_lange_work(LAPACK_COL_MAJOR, 'I', Bm, Bn, B,     ldb, work);
    double CNorm     = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, C,     ldc, work);
    double CImplNorm = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, CImpl, ldc, work);

    assert(CRef != CImpl);
    printf("alpha=%lf, beta=%lf\n", alpha, beta);
    dump_matrix<P>("A", A, Am, An, lda);
    dump_matrix<P>("B", B, Bm, Bn, ldb);
    dump_matrix<P>("C", C, Cm, Cn, ldc);
    dump_matrix<P>("CRef",  CRef,  Cm, Cn, ldc);
    dump_matrix<P>("CImpl", CImpl, Cm, Cn, ldc);
    dump_matrix_diff<P>("CRef - CImpl", CRef, CImpl, Cm, Cn, ldc);

    double CRefNorm  = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, CRef, ldc, work);
    TYPE beta_const = (TYPE) -1.0;
    native_axpy(ldc * n, beta_const, CImpl, 1, CRef, 1);
    double Rnorm = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, CRef, ldc, work);
    double eps = LAPACKE_slamch_work('e');

    printf("Rnorm %e, Anorm %e, Bnorm %e, CNorm %e, CImplNorm %e, CRefNorm %e\n",
            Rnorm, Anorm, Bnorm, CNorm, CImplNorm, CRefNorm);

    if (CNorm == CImplNorm)
        printf("!! CNorm == CImplNorm !! Have you forgoten to compute or move the data back ?\n");

    //TG Takes plasma-openmp test:
    // |R - R_ref|_p < gamma_{k+2} * |alpha| * |A|_p * |B|_p +
    //                 gamma_2 * |beta| * |C|_p
    // holds component-wise or with |.|_p as 1, inf, or Frobenius norm.
    // gamma_k = k*eps / (1 - k*eps), but we use
    // gamma_k = sqrt(k)*eps as a statistical average case.
    // Using 3*eps covers complex arithmetic.
    // See Higham, Accuracy and Stability of Numerical Algorithms, ch 2-3.

    //    result = Rnorm / ((Anorm + Bnorm + CNorm) * N * eps);
#if (PRECISION_z==1)
    double normalize = sqrt((double)k+2) * cabs(alpha) * Anorm * Bnorm + sqrt(2.0)*cabs(beta)*CNorm;
#elif (PRECISION_c==1)
    double normalize = sqrt((double)k+2) * cabsf(alpha) * Anorm * Bnorm + sqrt(2.0)*cabsf(beta)*CNorm;
#else
    double normalize = sqrt((double)k+2) * fabs(alpha) * Anorm * Bnorm + sqrt(2.0)*fabs(beta)*CNorm;
#endif

    double result = 0;
    if (!isnan(normalize) && (normalize !=0))
        result = Rnorm / normalize;

    printf("============\n");
    printf("Checking the norm of the difference against reference SGEMM \n");
    printf("-- gamma_(k+2) * |alpha| * |A|_p * |B|_p + gamma_2 * |beta| * |C|_p = %e < 3 * %e \n", result, eps);
    printf("============\n");

    int suspicious = 0;
    if (isnan(Rnorm) || isinf(Rnorm) || isnan(result) || isinf(result) || (result > 3*eps))
        suspicious = 1;

    free(work);

    return suspicious;
}

TYPED
int
syrk_cmp(
    CBLAS_UPLO uplo, CBLAS_TRANSPOSE trans,
    const BLAS_INT n, const BLAS_INT k,
    const TYPE alpha,
    const TYPE * A, const BLAS_INT lda,
    const TYPE beta,
    const TYPE * C, TYPE * CRef, const TYPE * CImpl, const BLAS_INT ldc,
    int repeat
) {
    /* run native */
    printf("Running native...\n");
    {
        uint64_t t0 = get_nanotime();
        for (int i = 0 ; i < repeat ; ++i)
            native_syrk(CblasColMajor, uplo, trans, n, k, alpha, A, lda, beta, CRef, ldc);
        uint64_t tf = get_nanotime();
        printf("Native took %lf s.\n", (tf - t0) / (double)1e9);
    }

    TYPE * work = (TYPE *) malloc(MAX(n, k) * sizeof(TYPE));

    const int Am = (trans == CblasNoTrans) ? n : k;
    const int An = (trans == CblasNoTrans) ? k : n;
    const int Cm = n;
    const int Cn = n;

    double Anorm     = native_lange_work(LAPACK_COL_MAJOR, 'I', Am, An, A,     lda, work);
    double CNorm     = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, C,     ldc, work);
    double CImplNorm = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, CImpl, ldc, work);

    printf("alpha=%lf, beta=%lf\n", alpha, beta);
    dump_matrix<P>("A",     A,     Am, An, lda);
    dump_matrix<P>("C",     C,     Cm, Cn, ldc);
    dump_matrix<P>("CRef",  CRef,  Cm, Cn, ldc);
    dump_matrix<P>("CImpl", CImpl, Cm, Cn, ldc);

    double CRefNorm  = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, CRef, ldc, work);
    TYPE beta_const = (TYPE) -1.0;
    native_axpy(ldc * n, beta_const, CImpl, 1, CRef, 1);
    double Rnorm = native_lange_work(LAPACK_COL_MAJOR, 'I', Cm, Cn, CRef, ldc, work);
    double eps = LAPACKE_slamch_work('e');

    printf("Rnorm %e, Anorm %e, CNorm %e, CImplNorm %e, CRefNorm %e\n",
            Rnorm, Anorm, CNorm, CImplNorm, CRefNorm);

    if (CNorm == CImplNorm)
        printf("!! CNorm == CImplNorm !! Have you forgoten to compute or move the data back ?\n");

    double result = Rnorm / ((Anorm + CNorm) * n * eps);

    printf("============\n");
    printf("Checking the norm of the difference against reference SSYRK \n");
    printf("-- ||Ccham - Clapack||_oo/((||A||_oo+||C||_oo).N.eps) = %e \n", result);
    printf("============\n");

    int suspicious = 0;
    if (isinf(CRefNorm) || isinf(CImplNorm) || isnan(result) || isinf(result) || (result > 10.0))
        suspicious = 1;

    free(work);

    return suspicious;
}

TYPED
int
trsm_cmp(
    CBLAS_SIDE side, CBLAS_UPLO uplo,
    CBLAS_TRANSPOSE transA, CBLAS_DIAG diag,
    const BLAS_INT m, const BLAS_INT n,
    const TYPE alpha,
    const TYPE * A, const BLAS_INT lda,
    const TYPE * B, TYPE * BRef, const TYPE * BImpl, const BLAS_INT ldb
) {
    const int Am = (side == CblasLeft) ? m : n;
    const int An = Am;
    const int Bm = m;
    const int Bn = n;

    /* run native */
    printf("Running native...\n");
    {
        uint64_t t0 = get_nanotime();
        native_trsm(CblasColMajor, side, uplo, transA, diag, m, n, alpha, A, lda, BRef, ldb);
        uint64_t tf = get_nanotime();
        printf("Native took %lf s.\n", (tf - t0) / (double)1e9);
    }

    printf("alpha=%lf\n", alpha);
    dump_matrix<P>("A",     A,     Am, An, lda);
    dump_matrix<P>("B",     B,     Bm, Bn, ldb);
    dump_matrix<P>("BRef",  BRef,  Bm, Bn, ldb);
    dump_matrix<P>("BImpl", BImpl, Bm, Bn, ldb);

    TYPE * work = (TYPE *)malloc(MAX(m, n) * sizeof(TYPE));
    assert(work);
    double Anorm        = native_lantr_work(LAPACK_COL_MAJOR, 'I', uplo, diag, Am, An, A, lda, work);
    double Bnorm        = native_lange_work(LAPACK_COL_MAJOR, 'I', m, n, BRef,  ldb, work);
    double BImplnorm    = native_lange_work(LAPACK_COL_MAJOR, 'I', m, n, BImpl, ldb, work);
    double BRefnorm     = native_lange_work(LAPACK_COL_MAJOR, 'I', m, n, BRef,  ldb, work);
    native_axpy(ldb * n, -1.0, BImpl, 1, BRef, 1);
    double Rnorm        = native_lange_work(LAPACK_COL_MAJOR, 'I', m, n, BRef,  ldb, work);

    printf("Rnorm %e, Anorm %e, Bnorm %e, BImplnorm %e, BRefnorm %e\n",
           Rnorm, Anorm, Bnorm, BImplnorm, BRefnorm);

    double eps = LAPACKE_slamch_work('e');
    double result = Rnorm / ((Anorm + BRefnorm) * MAX(m,n) * eps);

    printf("============\n");
    printf("Checking the norm of the difference against reference TRSM \n");
    printf("-- ||BImpl - BRef||_oo/((||A||_oo+||B||_oo).N.eps) = %e \n", result);

    int suspicious = 0;
    if (isinf(BRefnorm) || isinf(BImplnorm) || isnan(result) || isinf(result) || (result > 10.0))
        suspicious = 1;
    free(work);

    return suspicious;
}
