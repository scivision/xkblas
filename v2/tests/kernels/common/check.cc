/* retrived from xkblas original tests */

# include <math.h>
# include <sys/param.h>

# include "common/blas.h"

/**
 *  Verify the solution of the gemm 'CImpl' result.
 *      1) copy C to CRef
 *      2) perform gemm(A, B, CRef)
 *      3) compare CRef and CImpl
 */
int
gemm_cmp(
    CBLAS_TRANSPOSE transA, CBLAS_TRANSPOSE transB,
    int m, int n, int k,
    const TYPE alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE beta,
    const TYPE * C, TYPE * CRef, const TYPE * CImpl, int ldc
) {
    TYPE * work = (TYPE *) malloc(MAX(k,MAX(m, n))* sizeof(TYPE));

    int Am, An, Bm, Bn;
    if (transA == CblasNoTrans) {
        Am = m; An = k;
    } else {
        Am = k; An = m;
    }
    if (transB == CblasNoTrans) {
        Bm = k; Bn = n;
    } else {
        Bm = n; Bn = k;
    }

    /* run native */
    printf("Running native...\n");
    {
        uint64_t t0 = get_nanotime();
        native_gemm(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, CRef, ldc);
        uint64_t tf = get_nanotime();
        printf("Took %lf s.\n", (tf - t0) / (double)1e9);
    }

    double Anorm     = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I', Am, An, A,    lda, work);
    double Bnorm     = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I', Bm, Bn, B,    ldb, work);
    double CNorm     = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I',  m, n, C,     ldc, work);
    double CImplnorm = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I',  m, n, CImpl, ldc, work);
    double CRefNorm  = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I',  m, n, CRef,  ldc, work);

    TYPE beta_const = (TYPE) -1.0;
    cblas_zaxpy(ldc * n, &beta_const, CImpl, 1, CRef, 1);

    double Rnorm = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I', m, n, CRef, ldc, work);

    printf("Rnorm %e, Anorm %e, Bnorm %e, CNorm %e, CImplnorm %e, CRefNorm %e\n",
            Rnorm, Anorm, Bnorm, CNorm, CImplnorm, CRefNorm);

    double eps = LAPACKE_dlamch_work('e');

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
