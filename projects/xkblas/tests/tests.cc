/* ************************************************************************** */
/*                                                                            */
/*   tests.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:49 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "xkblas.h"

# include <assert.h>
# include <stdlib.h>
# include <stdint.h>
# include <cblas.h>

# if 1
# include "xkblas-skernel.h"
# define TYPE               float
# define xkblas_gemm_async  xkblas_sgemm_async
# else
# include "xkblas-bkernel.h"
# define TYPE               char
# define xkblas_gemm_async  xkblas_bgemm_async
# endif

int
main(void)
{
    xkblas_init();

    int transA = CblasNoTrans;
    int transB = CblasNoTrans;
    int M = 16;
    int N = M;
    int K = M;
    int LD = K+M;
    TYPE alpha = 1.0;
    TYPE beta  = 1.0;

    uintptr_t mem = (uintptr_t) malloc(sizeof(TYPE) * (LD * LD + LD));
    uintptr_t Ap  = mem + (LD - (mem % LD)) + 0 * sizeof(TYPE) * (LD * LD);
    uintptr_t Bp  = mem + (LD - (mem % LD)) + 1 * sizeof(TYPE) * (LD * LD);
    uintptr_t Cp  = mem + (LD - (mem % LD)) + 2 * sizeof(TYPE) * (LD * LD);

    assert(Ap % LD == 0);
    assert(Bp % LD == 0);
    assert(Cp % LD == 0);

    const TYPE * A = (const TYPE *) Ap;
    const TYPE * B = (const TYPE *) Bp;
          TYPE * C = (      TYPE *) Cp;

    xkblas_gemm_async(transA, transB, M, N, K, &alpha, A, LD, B, LD, &beta, C, LD);
    xkblas_sync();

    xkblas_deinit();

    return 0;
}
