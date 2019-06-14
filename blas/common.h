/*
** Copyright 2009-2013,2018,2019 INRIA
**
** Contributors :
**
** thierry.gautier@inrialpes.fr
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

#ifndef _kaapi_benchmark_common_h_
#define _kaapi_benchmark_common_h_

#if defined(__STDC_NO_COMPLEX__)
# error "Compiler support for complex number is required."
#else
# include <complex.h>
typedef float complex Complex32_t;
typedef double complex Complex64_t;
typedef double CFloat64_t;
#endif

#ifndef KAAPI_NO_INCLUDE_BLAS_H
#if defined(KAAPI_BLAS_USE_OPENBLAS)
#  include <cblas.h>
#  include <lapacke.h>
#elif defined(KAAPI_BLAS_USE_MKL)
#  define lapack_complex_float Complex32_t
#  define lapack_complex_double Complex64_t
#  include <mkl.h>
#  include <mkl_types.h>
#  include <mkl_cblas.h>
#  include <mkl_lapacke.h>
#else
#  error "Blas library undefined"
#endif
#define KAAPI_NO_DEFAULT_BLAS_ENUM
#endif

#include "xkblas.h"
#if KAAPI_USE_CUDA
#include <cublas_v2.h>
#endif
#include "kaapi.h"

/* List of kernel implemented in the runtime */
typedef enum {
  KERN_VOID,
  KERN_GEMM,
  KERN_GEMMT,
  KERN_TRMM,
  KERN_TRSM,
  KERN_SYMM,
  KERN_SYRK,
  KERN_SYR2K,
  KERN_HEMM,
  KERN_HERK,
  KERN_HER2K
} xkblas_kernel_t;

/* The blas thread
*/
extern kaapi_thread_t* _kaapi_blas_thread;

/* Initial data list
*/
extern kaapi_handle_t* _xkblas_list_sync0;

/* Internal matrix data structure
   From application pointer to a matrix, xkblas keep track of the internal matrix descriptor.
   The matrix descriptor contains meta information about each tile (kaapi_handle_t) and it's
   prefereed location in ldid.
   Both 2D arrays ldid and handle have dimension mtxnt and are stored in RowMajor format.
*/
struct xkblas_matrix_descr {
  void*            addr;   // matrix address
  size_t           eltsize;//
  size_t           ld;     // leading dimension
  size_t           M;      // matrix dimension, #row M
  size_t           N;      // matrix dimension, #col N
  size_t           mb;     // tile size (M)
  size_t           nb;     // tile size (N)
  size_t           mt;     // number of rows of tile
  size_t           nt;     // number of columns of tile
  uint16_t*        ldid;   // array (rowmajor) of mapping attribute or 0
  kaapi_handle_t*  handle; // array (rowmajor) of size MBxNB of handle
  size_t           capacity;// mt*nt size for handle allocation
  uint64_t         gen;    // generation number of the entry
  struct xkblas_matrix_descr* next; // to free
};
typedef struct xkblas_matrix_descr xkblas_matrix_descr_t;

#define xkblas_num_of_tiles(M,NB) (((M)+(NB)-1)/(NB))

extern int xkblas_matrix_descr_isinit(
  xkblas_matrix_descr_t* Ah
);

extern kaapi_handle_t* xkblas_get_handle(
  xkblas_matrix_descr_t* Ah,
  size_t m,
  size_t n);

/* Return ldid info for the tile A(i,j) i,j are tile indexes.
 */
extern uint16_t xkblas_get_ld(
  xkblas_matrix_descr_t* Mh,
  size_t i, size_t j
);

/* Automap block cyclic mapping for matrix of size M,N
*/
extern int xkblas_auto_map(
  xkblas_kernel_t kernel, xkblas_matrix_descr_t* Ah
);


/* Auto compute tile size
   M,N,K are the input problem sizes.
   Some kernel does not required 3 sizes and K is ignored
*/
extern size_t xkblas_auto_nb(
  xkblas_kernel_t kernel, size_t M, size_t N, size_t K
);


#if KAAPI_USE_CUDA
static inline cublasOperation_t cblas2cublas_op( int trans )
{
  switch (trans)
  {
    case CblasNoTrans: return CUBLAS_OP_N;
    case CblasTrans:  return CUBLAS_OP_T;
    case CblasConjTrans: return CUBLAS_OP_C;
  }
  abort();
}

static inline cublasOperation_t cblas2cublas_side( int side )
{
  switch (side)
  {
    case CblasLeft: return CUBLAS_SIDE_LEFT;
    case CblasRight: return CUBLAS_SIDE_RIGHT;
  }
  abort();
}

static inline cublasOperation_t cblas2cublas_uplo( int uplo )
{
  switch (uplo)
  {
    case CblasUpper: return CUBLAS_FILL_MODE_UPPER;
    case CblasLower: return CUBLAS_FILL_MODE_LOWER;
  }
  abort();
}

static inline cublasOperation_t cblas2cublas_diag( int diag )
{
  switch (diag)
  {
    case CblasNonUnit: return CUBLAS_DIAG_NON_UNIT;
    case CblasUnit: return CUBLAS_DIAG_UNIT;
  }
  abort();
}

#endif

/*
*/
static inline char* cblas2blas_fill( int uplo )
{
  switch (uplo) {
    case CblasLower: return "L";
    case CblasUpper: return "U";
    default:
      abort();
  }
}


static inline size_t kaapi_max( size_t a, size_t b)
{ return a<b ? b: a; }
static inline size_t kaapi_min( size_t a, size_t b)
{ return a>b ? b: a; }

static void kaapi_error( const char* s1, const char* s2)
{
  fprintf(stderr, "In %s: %s\n", s1, s2);
  abort();
}


/*
*/
extern __thread kaapi_thread_t* _xkblas_self_thread;
static inline kaapi_thread_t* xkblas_self_thread(void)
{ return _xkblas_self_thread; }


static inline void print_dmatrix(int M, int N, double*  A, int ldA)
{
  printf("%p\nmatrix( c( ", (void*)A);
  int i, j;
  for ( i = 0; i < M; i++)
  {
    for (  j = 0; j < N; j++)
    {
      char sep = ',';
      if ((i == M-1) && (j == N-1)) sep =' ';
      printf("%.15f%c", A[i+ldA*j],sep);
    }
    printf("  ");
  }
  printf(" ), nrow=%i, ncol=%i, byrow=TRUE);\n", M, N);
}

static inline void print_smatrix(int M, int N, float*  A, int ldA)
{
  printf("matrix( c( ");
  int i, j;
  for ( i = 0; i < M; i++)
  {
    for (  j = 0; j < N; j++)
    {
      char sep = ',';
      if ((i == M-1) && (j == N-1)) sep =' ';
      printf("%3f%c", A[i+ldA*j],sep);
    }
    printf("  ");
  }
  printf(" ), nrow=%i, ncol=%i, byrow=TRUE);\n", M, N);
}


/* Task write back to initiate write back transfer to local host main memory
   of cached data
*/
extern void kaapi_blas_create_taskwriteback(
  size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize
);

#endif /* _kaapi_benchmark_common_h_ */
