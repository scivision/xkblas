# include "logger.h"

extern "C" int
xkblas_init(void)
{
    XKBLAS_INFO("Initializing Xkblas");
    return 0;
}

extern "C"
void
xkblas_sync(void)
{
    XKBLAS_INFO("Synchronizing Xkblas");
}

extern "C" void
xkblas_deinit(void)
{
    XKBLAS_INFO("Deinitializing Xkblas");
}
