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

# include <xkblas/xkblas.hpp>

XKRT_NAMESPACE_USE;

extern "C"
int
xkblas_£gemm_tile_async(
    int transA, int transB,
    const int m, const int n, const int k,
    const TYPE * alpha,
    const TYPE * A, const int Atm, const int Atn, const int Amb, const int Anb, const int lda,
    const TYPE * B, const int Btm, const int Btn, const int Bmb, const int Bnb, const int ldb,
    const TYPE * beta,
          TYPE * C, const int Ctm, const int Ctn, const int Cmb, const int Cnb, const int ldc,
    xkrt_device_global_id_t device_global_id
) {
    return xkblas_get()->gemm_tile_async<xkblas_precision_t::££>(
        transA, transB,
        m, n, k,
        alpha,
        A, Atm, Atn, Amb, Anb, lda,
        B, Btm, Btn, Bmb, Bnb, ldb,
        beta,
        C, Ctm, Ctn, Cmb, Cnb, ldc,
        device_global_id
    );
}

extern "C"
int
xkblas_£gemm_async(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->gemm_async<xkblas_precision_t::££>(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

extern "C"
int
xkblas_£gemm_sync(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->gemm_sync<xkblas_precision_t::££>(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

extern "C"
int
xkblas_£gemm(
    int transA, int transB,
    int m, int n, int k,
    const TYPE * alpha,
    const TYPE * A, int lda,
    const TYPE * B, int ldb,
    const TYPE * beta,
          TYPE * C, int ldc
) {
    return xkblas_get()->gemm<xkblas_precision_t::££>(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}
