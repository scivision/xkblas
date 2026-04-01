/* ************************************************************************** */
/*                                                                            */
/*   potrf.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2026/01/08 16:32:34 by Romain PEREIRA         / _______ \       */
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
    int uplo,
    int n,
    TYPE * A, const int Atm, const int Atn, const int Amb, const int Anb, int lda,
    xkrt_device_unique_id_t device_unique_id
) {
    return xkblas_get()->potrf_tile_async<xkblas_precision_t::££>(uplo, n, A, Atm, Atn, Amb, Anb, lda, device_unique_id);
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

extern "C"
int
xkblas_£potrf_sync(
    int uplo,
    int n,
    TYPE * A,
    int lda
) {
    return xkblas_get()->potrf_sync<xkblas_precision_t::££>(uplo, n, A, lda);
}

extern "C"
int
xkblas_£potrf(
    int uplo,
    int n,
    TYPE * A,
    int lda
) {
    return xkblas_get()->potrf<xkblas_precision_t::££>(uplo, n, A, lda);
}
