/* ************************************************************************** */
/*                                                                            */
/*   syrk.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/03 15:23:28 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:45:22 by Romain PEREIRA         / _______ \       */
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
xkblas_£syrk_tile_async(
    int uplo, int trans,
    const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    distribution_t * d
) {
    return xkblas_get()->syrk_tile_async<xkblas_precision_t::££>(uplo, trans, n, k, alpha, A, Atm, Atn, Amb, Anb, lda, beta, C, Ctm, Ctn, Cmb, Cnb, ldc, d);
}

extern "C"
int
xkblas_£syrk_async(
    int uplo, int trans,
    int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->syrk_async<xkblas_precision_t::££>(uplo, trans, n, k, alpha, A, lda, beta, C, ldc);
}
