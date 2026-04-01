/* ************************************************************************** */
/*                                                                            */
/*   trsm.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/19 10:41:41 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2026/01/08 16:32:55 by Romain PEREIRA         / _______ \       */
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
    const int m, const int n,
    const TYPE * alpha,
    const TYPE * A, const int Atm, const int Atn, const int Amb, const int Anb, const int lda,
          TYPE * B, const int Btm, const int Btn, const int Bmb, const int Bnb, const int ldb,
    xkrt_device_unique_id_t device_unique_id
) {
    return xkblas_get()->trsm_tile_async<xkblas_precision_t::££>(side, uplo, transA, diag, m, n, alpha, A, Atm, Atn, Amb, Anb, lda, B, Btm, Btn, Bmb, Bnb, ldb, device_unique_id);
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

extern "C"
int
xkblas_£trsm_sync(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    return xkblas_get()->trsm_sync<xkblas_precision_t::££>(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb);
}

extern "C"
int
xkblas_£trsm(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    return xkblas_get()->trsm<xkblas_precision_t::££>(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb);
}
