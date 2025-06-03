/* ************************************************************************** */
/*                                                                            */
/*   logger-aml.h                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/16 16:15:23 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:06 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
