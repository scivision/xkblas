#ifndef __MATRIX_TILE_H__
# define __MATRIX_TILE_H__

# define XKBLAS_NUM_OF_TILES(N, TILE_SIZE) (((N)+(TILE_SIZE)-1)/(TILE_SIZE))
# define XKBLAS_MATRIX_TILE(A, LD, TI, TJ, SI, SJ) (A+((TJ*SJ) + (LD)*(TI*SI)))
# define XKBLAS_MATRIX_TILE_COORDINATE(A, LD, BX, BY, X0, Y0, X1, Y1)   \
    X0 = (size_t)A%LD;                                                  \
    Y0 = (size_t)A/LD;                                                  \
    X1 = X0 + BX;                                                       \
    Y1 = Y0 + BY;

#endif /* __MATRIX_TILE_H__ */
