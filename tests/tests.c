/* ************************************************************************** */
/*                                                                            */
/*   tests.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/11 23:14:22 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkblas/xkblas.h>
# include <xkblas/flops.h>
# include <xkblas/cblas.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdlib.h>
# include <stdint.h>
# include <string.h>

# if 1
# include "xkblas/skernels.h"
# define TYPE               float
# define xkblas_gemm_async  xkblas_sgemm_async
# define FLOPS(M, N, K)     FLOPS_SGEMM(M, N, K)
# endif

int
main(void)
{
    xkblas_init();

    int transA = CblasNoTrans;
    int transB = CblasNoTrans;
    size_t M = 16384;
    size_t N = M;
    size_t K = M;
    size_t LD = M;
    TYPE alpha = 1.0;
    TYPE beta  = 1.0;
    size_t ts = 2048;
    xkblas_set_param(ts, 0);
    const size_t nt = M / ts;

    // host matrices
    const size_t alignon = LD * sizeof(TYPE);
    const size_t size = 3 * LD*LD*sizeof(TYPE) + alignon;

    # if 1
    const uintptr_t memp = (const uintptr_t) malloc(size);
    const uintptr_t  mem = memp + (alignon - memp % alignon);
    assert(mem % alignon == 0);
    # else
    const uintptr_t mem = (const uintptr_t) malloc(size);
    # endif
    const TYPE * A = (const TYPE *) (mem + 0*LD*LD*sizeof(TYPE));
    const TYPE * B = (const TYPE *) (mem + 1*LD*LD*sizeof(TYPE));
          TYPE * C = (      TYPE *) (mem + 2*LD*LD*sizeof(TYPE));
    void * ptr = (void *) mem;
    memset(ptr, 0, size);

    uint64_t t0 = xkblas_get_nanotime();
    {
        xkblas_memory_register_tiled_async(ptr, size, nt);
        xkblas_gemm_async(transA, transB, M, N, K, &alpha, A, LD, B, LD, &beta, C, LD);
        xkblas_memory_matrix_coherent_async(C, LD, M, N, sizeof(TYPE));
        xkblas_memory_unregister_tiled_async(ptr, size, nt);
        printf("Graph created in %.4lf s\n", (xkblas_get_nanotime() - t0)/1e9);
        xkblas_sync();
    }
    uint64_t tf = xkblas_get_nanotime();
    double dt = (tf-t0)/1e9;
    double FLOPS_S = FLOPS(M, N, K) / dt;
    printf("Graph execution took %.4lf s (%.2lf TFLOP/s)\n", dt, FLOPS_S/1e12);

    xkblas_deinit();

    return 0;
}
