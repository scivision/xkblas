/* ************************************************************************** */
/*                                                                            */
/*   blas-version.h                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/20 23:57:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __BLAS_VERSION_H__
# define __BLAS_VERSION_H__

#if USE_OPENBLAS
# include <lapacke.h>
#endif

# if 0
#  define PRECISION_s 1
#  define TYPE float
#  define BLAS_INT long long int
#  define native_gemm(...)          cblas_sgemm(__VA_ARGS__)
#  define native_axpy(...)          cblas_saxpy(__VA_ARGS__)
#  define native_syrk(...)          cblas_ssyrk(__VA_ARGS__)
#  define native_trsm(...)          cblas_strsm(__VA_ARGS__)
#  define native_lange_work(...)    LAPACKE_slange_work(__VA_ARGS__)
#  define native_lantr_work(...)    LAPACKE_slantr_work(__VA_ARGS__)
#  define native_larnv_work(...)    LAPACKE_slarnv_work(__VA_ARGS__)
# else
#  define PRECISION_d 1
#  define TYPE double
#  define BLAS_INT long long int
#  define native_gemm(...)          cblas_dgemm(__VA_ARGS__)
#  define native_axpy(...)          cblas_daxpy(__VA_ARGS__)
#  define native_syrk(...)          cblas_dsyrk(__VA_ARGS__)
#  define native_trsm(...)          cblas_dtrsm(__VA_ARGS__)
#  define native_lange_work(...)    LAPACKE_dlange_work(__VA_ARGS__)
#  define native_lantr_work(...)    LAPACKE_dlantr_work(__VA_ARGS__)
#  define native_larnv_work(...)    LAPACKE_dlarnv_work(__VA_ARGS__)
#  define native_larnv(...)         LAPACKE_dlarnv(__VA_ARGS__)
# endif

// 1: uniform (0,1), 2: normal (0,1), 3: uniform (-1,1)
// Seed for the random number generator (4 integers)
# define FILL(PTR, N)                               \
    do {                                            \
        lapack_int seed[4] = { 1, 2, 3, 4 };        \
        native_larnv_work(1, seed, N, PTR);         \
    } while (0)
#endif
