/* ************************************************************************** */
/*                                                                            */
/*   symm.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/04 17:03:17 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:48:48 by Romain PEREIRA         / _______ \       */
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
xkblas_£symm_async(
    int side, int uplo,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    LOGGER_FATAL("Not implemented");
    return 0;
}
