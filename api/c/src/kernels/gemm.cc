/* ************************************************************************** */
/*                                                                            */
/*   gemm.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:44:53 by Romain PEREIRA         / _______ \       */
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
xkblas_£gemm_tile_async(
    distribution_t * d,
    int transA, int transB,
    const size_t m, const size_t n, const size_t k,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    const TYPE * beta,
          TYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc
) {
    return xkblas_get()->gemm_tile_async<xkblas_precision_t::££>(
        transA, transB,
        m, n, k,
        alpha,
        A, Atm, Atn, Amb, Anb, lda,
        B, Btm, Btn, Bmb, Bnb, ldb,
        beta,
        C, Ctm, Ctn, Cmb, Cnb, ldc,
        d
    );
}

extern "C"
int
xkblas_£gemm_async(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->gemm_async<xkblas_precision_t::££>(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}
