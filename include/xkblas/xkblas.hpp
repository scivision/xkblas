/* ************************************************************************** */
/*                                                                            */
/*   xkblas.hpp                                                   .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/27 16:05:48 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKBLAS_HPP__
# define __XKBLAS_HPP__

# include <xkblas/conf.h>
# include <xkblas/kernel.hpp>

# include <xkrt/runtime.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdint.h>

typedef enum    xkblas_state_t : uint8_t
{
    XKBLAS_CONTEXT_DEINITIALIZED = 0,
    XKBLAS_CONTEXT_INITIALIZED,
}               xkblas_state_t;

# define TYPE  xkblas_precision_type_t<P>
# define TYPED template <xkblas_precision_t P>

/* xkblas instance */
typedef struct  xkblas_t
{
    /* the xkrt runtime */
    xkrt::runtime_t runtime;

    /* state */
    struct {
        spinlock_t spinlock;
        volatile std::atomic<xkblas_state_t> current;
    } state;

    /* conf */
    xkblas_conf_t conf;

    //////////////////
    // Task formats //
    //////////////////

    /* task formats */
    struct {
        # define DEFINE(K) xkrt::task_format_id_t K[XKBLAS_PRECISION_MAX];
        XKBLAS_FORALL_KERNELS(DEFINE);
        # undef DEFINE
    } formats;

    // Utilities to set / get task formats
    # define XKBLAS_TASK_FORMAT_GET(P, K) this->formats.K[P]

    # define DEFINE(K) TYPED void task_format_create_##K(xkrt::task_format_t * format);
    XKBLAS_FORALL_KERNELS(DEFINE);
    # undef DEFINE

    ////////////////
    // Management //
    ////////////////

    void init(void);
    void deinit(void);

    ////////////
    // Memory //
    ////////////

    // TODO : coherent async etc

    /////////////
    // Kernels //
    /////////////

    // TODO: add documentation on this

    // LEVEL 1 - TODO

    TYPED
    int axpby_async(int n, const TYPE alpha, const TYPE * x, const TYPE beta, TYPE * y);

    TYPED
    int axpy_async(int n, const TYPE alpha, const TYPE * x, TYPE * y);

    TYPED
    int dot_async(const TYPE * x, const TYPE * y, TYPE * result);

    TYPED
    int divcopy_async();    // TODO

    TYPED
    int fill(int n, TYPE * x, const TYPE v);

    TYPED
    int nrm2_async(int n, const TYPE * x, float * result);

    TYPED
    int scalcopy_async();    // TODO

    TYPED
    int scale_async(int n, const TYPE s, const TYPE * x);

    // LEVEL 1 - single tile

    TYPED
    int axpy_tile_async(
        int n,
        const TYPE alpha,
        const TYPE * x,
              TYPE * y,
        const size_t tn,
        const size_t bs,
        xkrt::distribution_t * d
    );

    // LEVEL 2

    TYPED
    int copyscale_async(
        int m, int n,
        int should_copy,
        int * IW,
        const TYPE * D, int ldd,
              TYPE * L, int ldl,
              TYPE * U, int ldu
    );

    // LEVEL 2  - single tile
    TYPED
    int copyscale_tile_async(
        const size_t m, const size_t n,
        int should_copy,
        int * IW,
        const TYPE * D, const size_t Dm, const size_t Dn, int ldd,
              TYPE * L, const size_t Lm, const size_t Ln, int ldl,
              TYPE * U, const size_t Um, const size_t Un, int ldu,
        const size_t Ltm, const size_t Ltn,
        xkrt::distribution_t * d
    );

    // LEVEL 3

    TYPED
    int gemm_async(
        int transA, int transB,
        int m, int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    TYPED
    int gemmt_async(
        int uplo,
        int transA, int transB,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    TYPED
    int symm_async(
        int side, int uplo,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    TYPED
    int syr2k_async(
        int uplo, int trans,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    TYPED
    int syrk_async(
        int uplo, int trans,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    TYPED
    int trmm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
              TYPE * B, int ldb
    );

    TYPED
    int trsm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
              TYPE * B, int ldb
    );

    // LEVEL 3 TILE

    TYPED
    int gemm_tile_async(
        int transA, int transB,
        const size_t m, const size_t n, const size_t k,
        const TYPE * alpha,
        const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
        const TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
        const TYPE * beta,
              TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
        xkrt::distribution_t * d
    );

    TYPED
    int gemmt_tile_async(
        int uplo,
        int transA, int transB,
        const size_t n, const size_t k,
        const TYPE * alpha,
        const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
        const TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
        const TYPE * beta,
              TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
        xkrt::distribution_t * d
    );

    TYPED
    int syrk_tile_async(
        int uplo, int trans,
        const size_t n, const size_t k,
        const TYPE * alpha,
        const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
        const TYPE * beta,
              TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
        xkrt::distribution_t * d
    );

    TYPED
    int trsm_tile_async(
        int side, int uplo,
        int transA, int diag,
        const size_t m, const size_t n,
        const TYPE * alpha,
        const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
              TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
        xkrt::distribution_t * d
    );

    // LAPACK - TODO
    TYPED
    int geqrf_async();

    TYPED
    int orgqr_async();

    TYPED
    int ormqr();

}               xkblas_t;

// TODO : currently using a global variable to preserve previous 'xkblas_init'
// and 'xkblas_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'xkblas_t' argument that the user must keep
// track of
xkblas_t * xkblas_get(void);
xkrt::runtime_t * xkblas_xkrt_runtime_get(void);

#endif /* __XKBLAS_HPP__ */
