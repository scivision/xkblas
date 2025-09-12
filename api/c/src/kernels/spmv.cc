/* ************************************************************************** */
/*                                                                            */
/*   spmv.cc                                                      .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/12 20:17:53 by Romain PEREIRA         / _______ \       */
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
    const int nrows,
    const int ncols,
    const int nnz,
    const int * csr_row_offsets,
    const int * csr_col_indices,
    const TYPE * csr_values,
    /* vector X (in) */
    TYPE * X,
    const TYPE * beta,
    /* vector Y (inout) */
    TYPE * Y
) {
    return xkblas_get()->spmv_async<xkblas_precision_t::££>(alpha, transA, nrows, ncols, nnz, csr_row_offsets, csr_col_indices, csr_values, X, beta, Y);
}
