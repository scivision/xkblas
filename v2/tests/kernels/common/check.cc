/* retrived from xkblas original tests */

# include <math.h>
# include <sys/param.h>
# include "common/blas.h"

extern "C" {
int sgemm_(char *transa, char *transb,
  const BLAS_INT *m, const BLAS_INT *n, const BLAS_INT *k,
  const TYPE *alpha,
  const TYPE *a, const BLAS_INT *lda,
  const TYPE *b, const BLAS_INT *ldb,
  const TYPE *beta,
        TYPE *c, const BLAS_INT *ldc);
}

static void
dump_matrix(
    const char * label,
    const TYPE * M,
    const BLAS_INT ld
) {
    if (ld <= 32)
    {
        printf("---- %s ----\n", label);
        for (int i = 0 ; i < ld ; ++i)
            for (int j = 0 ; j < ld ; ++j)
                printf("%4.4f%c", M[i*ld+j], (j == ld-1) ? '\n' : ' ');

    }
}

/**
 *  Verify the solution of the gemm 'CImpl' result.
 *      1) copy C to CRef
 *      2) perform gemm(A, B, CRef)
 *      3) compare CRef and CImpl
 */
int
gemm_cmp(
    CBLAS_TRANSPOSE transA, CBLAS_TRANSPOSE transB,
    const BLAS_INT m, const BLAS_INT n, const BLAS_INT k,
    const TYPE alpha,
    const TYPE * A, const BLAS_INT lda,
    const TYPE * B, const BLAS_INT ldb,
    const TYPE beta,
    const TYPE * C, TYPE * CRef, const TYPE * CImpl, const BLAS_INT ldc
) {
    TYPE * work = (TYPE *) malloc(MAX(k,MAX(m, n)) * sizeof(TYPE));

    const int Am = (transA == CblasNoTrans) ? m : k;
    const int An = (transA == CblasNoTrans) ? k : m;
    const int Bm = (transB == CblasNoTrans) ? k : n;
    const int Bn = (transB == CblasNoTrans) ? n : k;

    double Anorm     = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I', Am, An, A,    lda, work);
    double Bnorm     = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I', Bm, Bn, B,    ldb, work);
    double CNorm     = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I',  m, n, C,     ldc, work);
    double CImplNorm = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I',  m, n, CImpl, ldc, work);

    /* run native */
    printf("Running native...\n");
    {
        uint64_t t0 = get_nanotime();
        # if 1
        char ta = cblas2blas_op(transA);
        char tb = cblas2blas_op(transB);
        sgemm_(&ta, &tb, &m, &n, &k, &alpha, A, &lda, B, &ldb, &beta, CRef, &ldc);
        # else
        cblas_sgemm(CblasColMajor, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, CRef, ldc);
        # endif
        uint64_t tf = get_nanotime();
        printf("Native took %lf s.\n", (tf - t0) / (double)1e9);
    }

    printf("alpha=%lf, beta=%lf\n", alpha, beta);
    dump_matrix("A",     A,     lda);
    dump_matrix("B",     B,     ldb);
    dump_matrix("C",     C,     ldc);
    dump_matrix("CRef",  CRef,  ldc);
    dump_matrix("CImpl", CImpl, ldc);

    // TODO : change slange, saxpy, slamch, etc... with defines based on type

    double CRefNorm  = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I',  m, n, CRef,  ldc, work);

    TYPE beta_const = (TYPE) -1.0;
    cblas_saxpy(ldc * n, beta_const, CImpl, 1, CRef, 1);

    double Rnorm = LAPACKE_slange_work(LAPACK_COL_MAJOR, 'I', m, n, CRef, ldc, work);

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
