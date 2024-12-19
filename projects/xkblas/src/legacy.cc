/* ************************************************************************** */
/*                                                                            */
/*   legacy.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:19:11 by                           \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "context.h"
# include "xkblas/xkblas.h"
# include "xkblas/kernel-type.h"

# include <ptr/ptr.h>
# include <ptr/logger/logger.h>

# include <assert.h>

extern "C"
void
xkblas_set_modemath(xkblas_mode_math_t mode)
{
    LOGGER_FATAL("Not implemented");
}

extern "C"
int
xkblas_get_ngpus(int * count)
{
    return ptr_get_ngpus(count);
}

extern "C"
int
xkblas_get_device_count(int * count)
{
    printf("RETURNED 1 lol\n");
    *count = 1;
    return 0;

    LOGGER_FATAL("Not implemented");
    return -1;
}

extern "C"
void
xkblas_set_param(size_t nb, size_t p)
{
    (void) p;
    LOGGER_IMPL("`p` unused");

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    for (int i = 0 ; i < XKBLAS_KERNEL_TYPE_MAX ; ++i)
        context->conf.kernels[i].tile = nb;
}

extern "C"
void
xkblas_finalize(void)
{
    xkblas_deinit();
}
