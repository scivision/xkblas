# include "logger/logger.h"

void
xkblas_drivers_init(void)
{
    XKBLAS_NOT_IMPLEMENTED_WARN("Dynamic driver loading not implemented. Only supporting built-in CUDA driver");
}
