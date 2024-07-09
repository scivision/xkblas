# include "xkblas.h"

int
main(void)
{
    xkblas_init();
    xkblas_sync();
    xkblas_deinit();
    return 0;
}
