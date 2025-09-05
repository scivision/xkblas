/* ************************************************************************** */
/*                                                                            */
/*   herk.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/05 20:29:31 by Romain PEREIRA         / _______ \       */
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
    distribution_t * d,
    int uplo, int transA,
    const size_t n, const size_t k,
    const TYPE_REAL * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE_REAL * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc
) {
    return xkblas_get()->herk_tile_async<xkblas_precision_t::££>(
        uplo, transA,
        n, k,
        alpha,
        A, Atm, Atn, Amb, Anb, lda,
        beta,
        C, Ctm, Ctn, Cmb, Cnb, ldc,
        d
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
