/* ************************************************************************** */
/*                                                                            */
/*   herk.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2026/01/08 16:32:29 by Romain PEREIRA         / _______ \       */
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

# include <xkblas/xkblas.hpp>

XKRT_NAMESPACE_USE;

extern "C"
int
xkblas_£herk_tile_async(
    int uplo, int transA,
    const int n, const int k,
    const TYPE_REAL * alpha,
    const TYPE * A, const int Atm, const int Atn, const int Amb, const int Anb, const int lda,
    const TYPE_REAL * beta,
          TYPE * C, const int Ctm, const int Ctn, const int Cmb, const int Cnb, const int ldc,
    xkrt_device_unique_id_t device_unique_id
) {
    return xkblas_get()->herk_tile_async<xkblas_precision_t::££>(
        uplo, transA,
        n, k,
        alpha,
        A, Atm, Atn, Amb, Anb, lda,
        beta,
        C, Ctm, Ctn, Cmb, Cnb, ldc,
        device_unique_id
    );
}

extern "C"
int
xkblas_£herk_async(
    int uplo, int transA,
    int n, int k,
    const TYPE_REAL * alpha,
    const TYPE * A, int lda,
    const TYPE_REAL * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->herk_async<xkblas_precision_t::££>(uplo, transA, n, k, alpha, A, lda, beta, C, ldc);
}

extern "C"
int
xkblas_£herk_sync(
    int uplo, int transA,
    int n, int k,
    const TYPE_REAL * alpha,
    const TYPE * A, int lda,
    const TYPE_REAL * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->herk_sync<xkblas_precision_t::££>(uplo, transA, n, k, alpha, A, lda, beta, C, ldc);
}

extern "C"
int
xkblas_£herk(
    int uplo, int transA,
    int n, int k,
    const TYPE_REAL * alpha,
    const TYPE * A, int lda,
    const TYPE_REAL * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->herk<xkblas_precision_t::££>(uplo, transA, n, k, alpha, A, lda, beta, C, ldc);
}
