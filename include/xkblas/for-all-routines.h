/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
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

// LEVEL 1 - TODO

# ifndef XKDEF
#  error "Must define macro `XKDEF`"
# endif

# ifndef XKDEFI
#  define XKDEFI XKDEF
# endif

/* y := a.x + b.y */
XKDEF(int, axpby, int n, const XKTYPE * alpha, const XKTYPE * x, const int incx, const XKTYPE * beta, XKTYPE * y, const int incy);

/* y := a.x + y */
XKDEF(int, axpy, int n, const XKTYPE * alpha, const XKTYPE * x, const int incx, XKTYPE * y, const int incy);

/* r = x.y */
XKDEF(int, dot, int n, const XKTYPE * x, const int incx, const XKTYPE * y, const int incy, XKTYPE * result);

/* y := x */
XKDEF(int, copy, size_t n, const XKTYPE * x, XKTYPE * y);

XKDEF(int, divcopy); // TODO

XKDEF(int, fill, int n, XKTYPE * x, const XKTYPE v);

XKDEF(int, nrm2, int n, const XKTYPE * x, float * result);

XKDEF(int, scalcopy); // TODO

XKDEF(int, scal, int n, const XKTYPE * alpha, XKTYPE * x, const int incx);

// LEVEL 2

XKDEF(int,
    copyscale,
    int m, int n,
    int should_copy,
    int * IW,
    const XKTYPE * D, int ldd,
          XKTYPE * L, int ldl,
          XKTYPE * U, int ldu
);

XKDEF(int,
    gemv,
    int transA,
    int m, int n,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE * x, const int incx,
    const XKTYPE * beta,
          XKTYPE * y, const int incy
);

// LEVEL 3

XKDEF(int,
    gemm,
    int transA, int transB,
    int m, int n, int k,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE * B, int ldb,
    const XKTYPE * beta,
          XKTYPE * C, int ldc
);

XKDEF(int,
    gemmt,
    int uplo,
    int transA, int transB,
    int n, int k,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE * B, int ldb,
    const XKTYPE * beta,
          XKTYPE * C, int ldc
);

XKDEF(int,
    herk,
    int uplo, int trans,
    int n, int k,
    const XKTYPE_REAL * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE_REAL * beta,
          XKTYPE * C, int ldc
);

XKDEF(int,
    symm,
    int side, int uplo,
    int m, int n,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE * B, int ldb,
    const XKTYPE * beta,
          XKTYPE * C, int ldc
);

XKDEF(int,
    syr2k,
    int uplo, int trans,
    int n, int k,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE * B, int ldb,
    const XKTYPE * beta,
          XKTYPE * C, int ldc
);

XKDEF(int,
    syrk,
    int uplo, int trans,
    int n, int k,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE * beta,
          XKTYPE * C, int ldc
);

XKDEF(int,
    trmm,
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
          XKTYPE * B, int ldb
);

/**
 *
 *  This perform a regular tiling using the TSRM tiling parameter
 *  With sub-TSRM on the diagonal, and GEMM on the other blocks
 *
 *  .-------------------.
 *  | \                 |
 *  |___\               |
 *  |    | \            |
 *  |____|___\          |
 *  |    |    | \       |
 *  |____|____|___\     |
 *  |    |    |    | \  |
 *  |____|____|____|___\|
 *
 */
XKDEF(int,
    trsm,
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
          XKTYPE * B, int ldb
);

/**
 *
 *  This perform a recursive tiling, recursing as long as m >= min_tile_size
 *  GEMMs may also be re-subdivided from the GEMM tiling parameter
 *
 *  .-------------------.
 *  | \                 |
 *  |___\               |
 *  |    | \            |
 *  |____|___\          |
 *  |         | \       |
 *  |         |___\     |
 *  |         |    | \  |
 *  |____ ____|____|___\|
 *
 */
XKDEF(int,
    trsm_rec,
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
          XKTYPE * B, int ldb,
    const int m_threshold
);

// LAPACKE
XKDEF(int, geqrf);
XKDEF(int, orgqr);
XKDEF(int, ormqr);
XKDEF(int, potrf, int uplo, int n, XKTYPE * A, int lda);

/* Y = alpha . op(A) . X + beta . Y
 * spmv of a CSR matrix with dense vectors */
XKDEFI(
    int,
    spmv,
    const XKTYPE * alpha,
    int transA,
    int index_base,
    const int nrows,
    const int ncols,
    const int nnz,
    const int format,
    const XKINDEX * row,
    const XKINDEX * col,
    const XKTYPE * values,
    XKTYPE * X,
    const XKTYPE * beta,
    XKTYPE * Y
);

//////////////////
/// SINGLE TILE //
//////////////////

// LEVEL 1 - single tile

XKDEF(int,
    axpy_tile,
    int n,
    const XKTYPE * alpha,
    const XKTYPE * x,
    const int incx,
          XKTYPE * y,
    const int incy,
    const size_t bs,
    XKDEVICE device_global_id
);

XKDEF(int,
    copy_tile,
    size_t n,
    const XKTYPE * x,
          XKTYPE * y,
    XKDEVICE device_global_id
);

XKDEF(int,
    dot_tile,
    int n,
    const XKTYPE * x, const int incx,
    const XKTYPE * y, const int incy,
    const XKTYPE * temp_r,
          XKTYPE * r,
    XKDEVICE device_global_id
);

XKDEF(int,
    scal_tile,
    int n,
    const XKTYPE * alpha,
    XKTYPE * x,
    const int incx,
    XKDEVICE device_global_id
);

// LEVEL 2  - single tile
XKDEF(int,
    copyscale_tile,
    const size_t m, const size_t n,
    int should_copy,
    int * IW,
    const XKTYPE * D, const size_t Dm, const size_t Dn, int ldd,
          XKTYPE * L, const size_t Lm, const size_t Ln, int ldl,
          XKTYPE * U, const size_t Um, const size_t Un, int ldu,
    XKDEVICE device_global_id
);

XKDEF(int,
    gemv_tile,
    int transA,
    const size_t m, const size_t n,
    const XKTYPE * alpha,
    const XKTYPE * A, int lda,
    const XKTYPE * x, const int incx,
    const XKTYPE * beta,
          XKTYPE * y, const size_t tm, const size_t mb, const int incy,
    XKDEVICE device_global_id
);

// LEVEL 3 TILE

XKDEF(int,
    gemm_tile,
    int transA, int transB,
    const size_t m, const size_t n, const size_t k,
    const XKTYPE * alpha,
    const XKTYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const XKTYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    const XKTYPE * beta,
          XKTYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    XKDEVICE device_global_id
);

XKDEF(int,
    gemmt_tile,
    int uplo,
    int transA, int transB,
    const size_t n, const size_t k,
    const XKTYPE * alpha,
    const XKTYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const XKTYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    const XKTYPE * beta,
          XKTYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    XKDEVICE device_global_id
);

XKDEF(int,
    herk_tile,
    int uplo, int trans,
    const size_t n, const size_t k,
    const XKTYPE_REAL * alpha,
    const XKTYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const XKTYPE_REAL * beta,
          XKTYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    XKDEVICE device_global_id
);

XKDEF(int,
    syrk_tile,
    int uplo, int trans,
    const size_t n, const size_t k,
    const XKTYPE * alpha,
    const XKTYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    const XKTYPE * beta,
          XKTYPE * C, const size_t Ctm, const size_t Ctn, const size_t Cmb, const size_t Cnb, const size_t ldc,
    XKDEVICE device_global_id
);

XKDEF(int,
    trsm_tile,
    int side, int uplo,
    int transA, int diag,
    const size_t m, const size_t n,
    const XKTYPE * alpha,
    const XKTYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
          XKTYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    XKDEVICE device_global_id
);

// LAPACKE TILE

XKDEF(int,
    potrf_tile,
    int uplo,
    int n,
    XKTYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
    XKDEVICE device_global_id
);

// SPARSE

XKDEFI(
    int,
    spmv_tile,
    const XKTYPE * alpha,
    int transA,
    int index_base,
    const int nrows,
    const int ncols,
    const int nnz,
    const int format,
    const XKINDEX * row,
    const XKINDEX * col,
    const XKTYPE * values,
    XKTYPE * X,
    const XKTYPE * beta,
    XKTYPE * Y,
    XKDEVICE device_global_id
);
