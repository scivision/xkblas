/* ************************************************************************** */
/*                                                                            */
/*   oneapi-mkl-helper.h                                                      */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/18 20:32:24 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
