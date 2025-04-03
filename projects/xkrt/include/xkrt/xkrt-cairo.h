/* ************************************************************************** */
/*                                                                            */
/*   xkrt-cairo.h                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/04/03 16:02:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 16:04:24 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_CAIRO_H__
# define __XKRT_CAIRO_H__

# include <xkrt/xkrt-support.h>

# if XKRT_SUPPORT_CAIRO
void xkrt_cairo_memory_trees(xkrt_runtime_t * runtime);
# else
# define xkrt_cairo_memory_trees(...) LOGGER_NOT_SUPPORTED()
# endif

#endif /* __XKRT_CAIRO_H__ */
