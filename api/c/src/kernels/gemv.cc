/* ************************************************************************** */
/*                                                                            */
/*   gemv.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/26 00:05:30 by Romain PEREIRA         / _______ \       */
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
xkblas_£gemv_tile_async(
    distribution_t * d,
    int transA,
    const size_t m, const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const size_t lda,
    const TYPE * x, const size_t incx,
    const TYPE * beta,
          TYPE * y, const size_t tm, const size_t mb, const size_t incy
) {
    return xkblas_get()->gemv_tile_async<xkblas_precision_t::££>(
        transA,
        m, n,
        alpha,
        A, lda,
        x, incx,
        beta,
        y, tm, mb, incy,
        d
    );
}

extern "C"
int
xkblas_£gemv_async(
    int transA,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * x, int incx,
    const TYPE * beta,
          TYPE * y, int incy
) {
    return xkblas_get()->gemv_async<xkblas_precision_t::££>(transA, m, n, alpha, A, lda, x, incx, beta, y, incy);
}
