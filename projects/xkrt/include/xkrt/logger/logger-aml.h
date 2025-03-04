/* ************************************************************************** */
/*                                                                            */
/*   logger-aml.h                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/03 20:53:21 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_AML_H__
# define __LOGGER_AML_H__

# include <xkrt/logger/logger.h>
# include <aml/utils/error.h>

# define AML_SAFE_CALL(X)                                                           \
    do {                                                                            \
        int r = X;                                                                  \
        if (r != AML_SUCCESS)                                                       \
            LOGGER_FATAL("`%s` failed with `%s` (%d)", #X, aml_strerror(r), r);     \
    } while (0)

#endif /* __LOGGER_AML_H__ */
