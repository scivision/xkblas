# include "xkblas.h"
# include "xkblas-context.h"
# include "logger/logger.h"

# include <assert.h>

void
xkblas_set_modemath(xkblas_mode_math_t mode)
{
    XKBLAS_FATAL("Not implemented");
}

int
xkblas_get_ngpus(void)
{
    XKBLAS_FATAL("Not implemented");
    return -1;
}

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
