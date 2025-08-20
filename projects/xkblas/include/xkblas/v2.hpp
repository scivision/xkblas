/* ************************************************************************** */
/*                                                                            */
/*   v2.hpp                                                       .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/20 15:30:57 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef V2
# define V2

/* XKBLAS LEGACY INTERFACES */
# include <xkblas.h>
# include <xkblas/conf.h>

# include <xkrt/xkrt.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdint.h>

typedef enum    xkblas_state_t : uint8_t
{
    XKBLAS_CONTEXT_DEINITIALIZED = 0,
    XKBLAS_CONTEXT_INITIALIZED,
}               xkblas_state_t;

/* xkblas instance */
typedef struct  xkblas_t
{
    /* the xkrt runtime */
    xkrt_runtime_t runtime;

    /* state */
    struct {
        spinlock_t spinlock;
        volatile std::atomic<xkblas_state_t> current;
    } state;

    /* conf */
    xkblas_conf_t conf;

    /////////////
    // Kernels //
    /////////////

    // LEVEL 2

    template <typename TYPE>
    int copyscale_async(
        int m, int n,
        int should_copy, int * IW,
        const TYPE * D, int ldd,
              TYPE * L, int ldl,
              TYPE * U, int ldu
    );

    // LEVEL 3

    template <typename TYPE>
    int gemm_async(
        int transA, int transB,
        int m, int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    template <typename TYPE>
    int gemmt_async(
        int transA, int transB,
        int m, int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    template <typename TYPE>
    int symm_async(
        int side, int uplo,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    template <typename TYPE>
    int syr2k_async(
        int uplo, int trans,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * B, int ldb,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    template <typename TYPE>
    int syrk_async(
        int uplo, int trans,
        int n, int k,
        const TYPE * alpha,
        const TYPE * A, int lda,
        const TYPE * beta,
              TYPE * C, int ldc
    );

    template <typename TYPE>
    int trmm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
              TYPE * B, int ldb
    );

    template <typename TYPE>
    int trsm_async(
        int side, int uplo,
        int transA, int diag,
        int m, int n,
        const TYPE * alpha,
        const TYPE * A, int lda,
              TYPE * B, int ldb
    );

}               xkblas_t;

// TODO : currently using a global variable to preserve previous 'xkblas_init'
// and 'xkblas_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'xkblas_t' argument that the user must keep
// track of
extern "C"
xkblas_t * xkblas_get(void);

extern "C"
xkrt_runtime_t * xkblas_xkrt_runtime_get(void);

extern "C"
task_format_id_t xkblas_task_format_create(task_format_t * format);

#endif /* V2 */
