/* ************************************************************************** */
/*                                                                            */
/*   trsm.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/19 10:41:41 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/10/06 23:11:54 by Romain PEREIRA         / _______ \       */
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
xkblas_£trsm_tile_async(
    int side, int uplo,
    int transA, int diag,
    const size_t m, const size_t n,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
          TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    xkrt_device_global_id_t device_global_id
) {
    return xkblas_get()->trsm_tile_async<xkblas_precision_t::££>(side, uplo, transA, diag, m, n, alpha, A, Atm, Atn, Amb, Anb, lda, B, Btm, Btn, Bmb, Bnb, ldb, device_global_id);
}

// A*X = B or X*A = B
// B is (M, N)
// A is whether (M, M) if side is left, whether (N, N) if side is right
extern "C"
int
xkblas_£trsm_async(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    return xkblas_get()->trsm_async<xkblas_precision_t::££>(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb);
}
