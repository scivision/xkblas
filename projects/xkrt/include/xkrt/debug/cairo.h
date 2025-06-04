/* ************************************************************************** */
/*                                                                            */
/*   cairo.h                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/26 19:40:36 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:59:26 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_CAIRO_H__
# define __XKRT_CAIRO_H__

# include <xkrt/support.h>

# if XKRT_SUPPORT_CAIRO
void xkrt_cairo_memory_trees(xkrt_runtime_t * runtime);
# else
# define xkrt_cairo_memory_trees(...) LOGGER_NOT_SUPPORTED()
# endif

#endif /* __XKRT_CAIRO_H__ */
