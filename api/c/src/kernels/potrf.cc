/* ************************************************************************** */
/*                                                                            */
/*   potrf.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/05 20:54:41 by Romain PEREIRA         / _______ \       */
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
xkblas_£potrf_tile_async(
    distribution_t * d,
    int uplo,
    int n,
    TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, size_t lda
) {
    return xkblas_get()->potrf_tile_async<xkblas_precision_t::££>(uplo, n, A, Atm, Atn, Amb, Anb, lda, d);
}

extern "C"
int
xkblas_£potrf_async(
    int uplo,
    int n,
    TYPE * A,
    int lda
) {
    return xkblas_get()->potrf_async<xkblas_precision_t::££>(uplo, n, A, lda);
}
