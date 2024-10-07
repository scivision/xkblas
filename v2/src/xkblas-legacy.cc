# include "xkblas.h"
# include "xkblas-context.h"
# include "logger/logger.h"

# include <assert.h>

void
xkblas_set_modemath(xkblas_mode_math_t mode)
{
    XKBLAS_FATAL("Not implemented");
}

/* ??? */
uint64_t
xkblas_register_memory_async(void * ptr, uint64_t sz)
{
    XKBLAS_WARN("'xkblas_register_memory_async' not implemented");
    return -1;
}

int
xkblas_unregister_memory(void * ptr, uint64_t sz)
{
    XKBLAS_FATAL("Not implemented");
    return -1;
}

int
xkblas_register_memory_waitall(void)
{
    XKBLAS_WARN("`xkblas_register_memory_waitall` not implemented");
    return -1;
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
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    for (int i = 0 ; i < XKBLAS_KERNEL_TYPE_MAX ; ++i)
    {
        context->conf.kernels[i].tile[0] = nb;
        context->conf.kernels[i].tile[1] = nb;
    }
}

/* invalidate device memory */
int
xkblas_memory_invalidate_caches(void)
{
    XKBLAS_WARN("`xkblas_memory_invalidate_caches` not implemented");
    return -1;
}
