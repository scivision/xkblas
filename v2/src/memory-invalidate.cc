# include "xkblas-context.h"

static int
xkblas_memory_invalidate_devices(void)
{
    XKBLAS_INFO("Invalidate XKBlas devices memory");

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    context->memtree.invalidate();

    return 0;
}

extern "C"
int
xkblas_memory_invalidate_caches(void)
{
    return xkblas_memory_invalidate_devices();
}
