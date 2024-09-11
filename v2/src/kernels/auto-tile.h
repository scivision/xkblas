#ifndef __AUTO_TILE_H__
# define __AUTO_TILE_H__

# include "kernels/kernel-type.h"

void
xkblas_kernel_auto_tile(
    xkblas_kernel_type_t kernel,
    int * args,
    int * bs
);

#endif /* __AUTO_TILE_H__ */
