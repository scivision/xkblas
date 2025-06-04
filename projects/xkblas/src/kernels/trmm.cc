/* ************************************************************************** */
/*                                                                            */
/*   trmm.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/04 17:03:17 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:36:03 by Romain PEREIRA         / _______ \       */
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

/**
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon zgemm wrappers
 *
 * @version 1.0.0
 * @comment This file has been automatically generated
 *          from Plasma 2.5.0 for CHAMELEON 1.0.0
 * @author Mathieu Faverge
 * @author Emmanuel Agullo
 * @author Cedric Castagnede
 * @author Thierry Gautier
 * @date 2018-11-20
 * @precisions normal z -> s d c
 * This file was merged from Chameleon by Thierry Gautier for Kaapi that
 * support natively 2D memory view.
 */

# include <xkrt/logger/logger.h>

extern "C"
int
xkblas_£trmm_async(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    LOGGER_FATAL("Not implemented");
    return 0;
}

void
register_£trmm_format(void)
{
    LOGGER_IMPL("Not implemented");
}
