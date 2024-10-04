# include "xkblas.h"
# include "logger/logger.h"

void
xkblas_set_modemath(xkblas_mode_math_t mode)
{
    XKBLAS_IMPL("Not implemented");
}

/* ??? */
uint64_t
xkblas_register_memory_async(void * ptr, uint64_t sz)
{
    XKBLAS_IMPL("Not implemented");
    return -1;
}

int
xkblas_unregister_memory(void * ptr, uint64_t sz)
{
    XKBLAS_IMPL("Not implemented");
    return -1;
}

int
xkblas_register_memory_waitall(void)
{
    XKBLAS_IMPL("Not implemented");
    return -1;
}

int
xkblas_get_ngpus(void)
{
    XKBLAS_IMPL("Not implemented");
    return -1;
}

void
xkblas_set_param(size_t nb, size_t p)
{
    XKBLAS_IMPL("Not implemented");
}

/* invalidate device memory */
[[deprecated("No replacement available yet (`caches` name may not be the most appropriate here)")]]
int
xkblas_memory_invalidate_caches(void)
{
    XKBLAS_IMPL("Not implemented");
    return -1;
}
