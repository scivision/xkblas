/* ************************************************************************** */
/*                                                                            */
/*   syr2k.cc                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/01/30 00:16:18 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/08/27 16:10:40 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkblas/xkblas.hpp>

XKRT_NAMESPACE_USE;

TYPED
void
xkblas_t::task_format_create_SYR2K(
    task_format_t * format
) {
}

# define DEFINE(P)  \
    template void xkblas_t::task_format_create_SYR2K<P>(task_format_t * format);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
