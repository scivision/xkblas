/* ************************************************************************** */
/*                                                                            */
/*   ze_blas.h                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:57:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# ifndef __ZE_sBLAS_H__
#  define __ZE_sBLAS_H__

# include <xkrt/driver/driver-ze.h>

void
ze_blas_sgemm(
    xkrt_stream_ze_t * stream,
    int transA, int transB,
    int m, int n, int k,
    const float * alpha,
    const float * A, int lda,
    const float * B, int ldb,
    const float * beta,
          float * C, int ldc
);

# endif /* __ZE_sBLAS_H__ */
