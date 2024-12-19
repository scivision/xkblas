/* ************************************************************************** */
/*                                                                            */
/*   symm.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:20:02 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <ptr/logger/logger.h>

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

void
register_£symm_format(void)
{
    LOGGER_IMPL("Not implemented");
}

