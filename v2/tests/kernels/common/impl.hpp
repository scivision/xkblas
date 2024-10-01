#ifndef __IMPL_HPP__
# define __IMPL_HPP__

#ifndef TYPE
# error "Must include a type file before including " __FILE__
#endif

# include <assert.h>

class impl_t
{
    public:

        /* impl name */
        const char * name(void) const;

        /* init/deinit routines */
        void init(void);
        void deinit(void);

        /* kernels (async) */
        void gemm(
            int transa, int transb,
            int m, int n, int k,
            const TYPE * alpha,
            const TYPE * A, int lda,
            const TYPE * B, int ldb,
            const TYPE * beta,
                  TYPE * C, int ldc
        );

        void
        trsm(
            int side, int uplo,
            CBLAS_TRANSPOSE transA, int diag,
            const BLAS_INT m, const BLAS_INT n,
            const TYPE * alpha,
            const TYPE * A, const BLAS_INT lda,
                  TYPE * B, const BLAS_INT ldb
        );

        void
        copyscale(
            const BLAS_INT m, const BLAS_INT n,
            bool should_copy, int * IW,
            const TYPE * D, const int ldd,
                  TYPE * L, const int ldl,
                  TYPE * U, const int ldu
        );

        void
        coherent(
            TYPE * M,
            int m, int n,
            int ld
        );

        void
        set_tile(int m, int n);

        /* wait for the completion of previously sent operations */
        void wait(void);

}; /* impl_t */

#endif /* __IMPL_HPP__ */
