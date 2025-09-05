/* ************************************************************************** */
/*                                                                            */
/*   syr2k.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/04 17:03:17 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:49:12 by Romain PEREIRA         / _______ \       */
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
xkblas_£syr2k_async(
    int uplo, int trans,
    int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    LOGGER_FATAL("Not implemented");
    return 0;
}
