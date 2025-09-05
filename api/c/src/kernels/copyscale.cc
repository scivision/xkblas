/* ************************************************************************** */
/*                                                                            */
/*   copyscale.cc                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/28 19:46:21 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:44:45 by Romain PEREIRA         / _______ \       */
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
xkblas_£copyscale_tile_async(
    size_t m, size_t n,
    int should_copy,
    int * IW,
    const TYPE * D, const size_t Dm, const size_t Dn, int ldd,
          TYPE * L, const size_t Lm, const size_t Ln, int ldl,
          TYPE * U, const size_t Um, const size_t Un, int ldu,
    const size_t Ltm, const size_t Ltn,
    distribution_t * d
) {
    return xkblas_get()->copyscale_tile_async<xkblas_precision_t::££>(m, n, should_copy, IW, D, Dm, Dn, ldd, L, Lm, Ln, ldl, U, Um, Un, ldu, Ltm, Ltn, d);
}

extern "C"
int
xkblas_£copyscale_async(
    int m, int n,
    int should_copy,
    int * IW,
    const TYPE * D, int ldd,
          TYPE * L, int ldl,
          TYPE * U, int ldu
) {
    return xkblas_get()->copyscale_async<xkblas_precision_t::££>(m, n, should_copy, IW, D, ldd, L, ldl, U, ldu);
}
