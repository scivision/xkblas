#ifndef __BLAS_H__
# define __BLAS_H__

# include <stdlib.h>

typedef enum {CblasRowMajor=101, CblasColMajor=102} CBLAS_ORDER;
typedef enum {CblasNoTrans=111, CblasTrans=112, CblasConjTrans=113, CblasConjNoTrans=114} CBLAS_TRANSPOSE;
typedef enum {CblasUpper=121, CblasLower=122} CBLAS_UPLO;
typedef enum {CblasNonUnit=131, CblasUnit=132} CBLAS_DIAG;
typedef enum {CblasLeft=141, CblasRight=142} CBLAS_SIDE;
typedef CBLAS_ORDER CBLAS_LAYOUT;

static inline int xkblas_blas2cblas_trans( const char* trans )
{
  switch (trans[0]) {
    case 'n':
    case 'N': return CblasNoTrans;
    case 't':
    case 'T': return CblasTrans;
    case 'c':
    case 'C': return CblasConjTrans;
    default:
      abort();
  }
}

static inline int xkblas_blas2cblas_side( const char* side )
{
  switch (side[0]) {
    case 'l':
    case 'L': return CblasLeft;
    case 'r':
    case 'R': return CblasRight;
    default:
      abort();
  }
}

static inline int xkblas_blas2cblas_fill( const char* uplo )
{
  switch (uplo[0]) {
    case 'l':
    case 'L': return CblasLower;
    case 'u':
    case 'U': return CblasUpper;
    default:
      abort();
  }
}

static inline int xkblas_blas2cblas_diag( const char* diag )
{
  switch (diag[0]) {
    case 'n':
    case 'N': return CblasNonUnit;
    case 'u':
    case 'U': return CblasUnit;
    default:
      abort();
  }
}

# define XKBLAS_NUM_OF_TILES(N, TILE_SIZE) (((N)+(TILE_SIZE)-1)/(TILE_SIZE))
# define XKBLAS_MATRIX_TILE(A, LD, TI, TJ, SI, SJ) (A+((TJ*SJ) + (LD)*(TI*SI)))
# define XKBLAS_MATRIX_TILE_COORDINATE(A, LD, BX, BY, X0, Y0, X1, Y1)   \
    X0 = (size_t)A%LD;                                                  \
    Y0 = (size_t)A/LD;                                                  \
    X1 = X0 + BX;                                                       \
    Y1 = Y0 + BY;

#endif /* __BLAS_H__ */
