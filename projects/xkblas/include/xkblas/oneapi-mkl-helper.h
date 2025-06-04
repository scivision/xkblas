/* ************************************************************************** */
/*                                                                            */
/*   oneapi-mkl-helper.h                                          .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:27:01 by Romain PEREIRA         / _______ \       */
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

#ifndef __ONEAPI_MKL_HELPER_H__
# define __ONEAPI_MKL_HELPER_H__

# include "xkblas/cblas.h"
# include <oneapi/mkl.hpp>

static inline oneapi::mkl::transpose
cblas2mkl_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return oneapi::mkl::transpose::N;
        case CblasTrans:        return oneapi::mkl::transpose::T;
        case CblasConjTrans:    return oneapi::mkl::transpose::C;
    }
    LOGGER_FATAL("Unknown trans code");
    abort();
}

#endif /* __ONEAPI_MKL_HELPER_H__ */
