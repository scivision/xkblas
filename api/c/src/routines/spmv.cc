/* ************************************************************************** */
/*                                                                            */
/*   spmv.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2026/01/07 05:17:25 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkblas/xkblas.hpp>

XKRT_NAMESPACE_USE;

extern "C"
int
xkblas_£spmv_async(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    int index_type,
    const int nrows,
    const int ncols,
    const size_t nnz,
    const int format,
    const void * row,
    const void * col,
    const TYPE * values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y
) {
    if (index_type == 32)
        return xkblas_get()->spmv_async<xkblas_precision_t::££, I32>(alpha, transA, index_base, nrows, ncols, nnz, format, (int32_t *) row, (int32_t *) col, values, X, beta, Y);
    else
    {
        assert(index_type == 64);
        return xkblas_get()->spmv_async<xkblas_precision_t::££, I64>(alpha, transA, index_base, nrows, ncols, nnz, format, (int64_t *) row, (int64_t *) col, values, X, beta, Y);
    }
}

extern "C"
int
xkblas_£spmv_sync(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    int index_type,
    const int nrows,
    const int ncols,
    const size_t nnz,
    const int format,
    const void * row,
    const void * col,
    const TYPE * values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y
) {
    if (index_type == 32)
        return xkblas_get()->spmv_sync<xkblas_precision_t::££, I32>(alpha, transA, index_base, nrows, ncols, nnz, format, (int32_t *) row, (int32_t *) col, values, X, beta, Y);
    else
    {
        assert(index_type == 64);
        return xkblas_get()->spmv_sync<xkblas_precision_t::££, I64>(alpha, transA, index_base, nrows, ncols, nnz, format, (int64_t *) row, (int64_t *) col, values, X, beta, Y);
    }
}

extern "C"
int
xkblas_£spmv(
    const TYPE * alpha,
    /* matrix A (in) */
    int transA,
    int index_base,
    int index_type,
    const int nrows,
    const int ncols,
    const size_t nnz,
    const int format,
    const void * row,
    const void * col,
    const TYPE * values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y
) {
    if (index_type == 32)
        return xkblas_get()->spmv<xkblas_precision_t::££, I32>(alpha, transA, index_base, nrows, ncols, nnz, format, (int32_t *) row, (int32_t *) col, values, X, beta, Y);
    else
    {
        assert(index_type == 64);
        return xkblas_get()->spmv<xkblas_precision_t::££, I64>(alpha, transA, index_base, nrows, ncols, nnz, format, (int64_t *) row, (int64_t *) col, values, X, beta, Y);
    }
}
