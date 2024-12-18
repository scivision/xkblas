/* ************************************************************************** */
/*                                                                            */
/*   legacy.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/18 15:20:27 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "xkblas.h"
# include "context.h"
# include "logger/logger.h"

# include <assert.h>

extern "C"
void
xkblas_set_modemath(xkblas_mode_math_t mode)
{
    XKBLAS_FATAL("Not implemented");
}

extern "C"
int
xkblas_get_ngpus(int * count)
{
    assert(count);

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    *count = 0;
    for (int i = 0 ; i < XKBLAS_DRIVER_TYPE_MAX ; ++i)
    {
        xkblas_driver_t * driver = context->drivers.list + i;
        assert(driver);

        if (driver->f_get_ndevices_max)
            *count += driver->f_get_ndevices_max();
    }
    return 0;
}

extern "C"
int
xkblas_get_device_count(int * count)
{
    printf("RETURNED 1 lol\n");
    *count = 1;
    return 0;

    XKBLAS_FATAL("Not implemented");
    return -1;
}

extern "C"
void
xkblas_set_param(size_t nb, size_t p)
{
    (void) p;
    XKBLAS_IMPL("`p` unused");

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
