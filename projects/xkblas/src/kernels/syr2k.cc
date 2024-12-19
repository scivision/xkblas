/* ************************************************************************** */
/*                                                                            */
/*   syr2k.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:20:11 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <kaapi/logger/logger.h>

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

void
register_£syr2k_format(void)
{
    LOGGER_IMPL("Not implemented");
}
