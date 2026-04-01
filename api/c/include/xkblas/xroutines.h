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

#ifndef __£XKBLAS_ROUTINES_H__
# define __£XKBLAS_ROUTINES_H__

# include <xkrt/consts.h>

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    # define XKTYPE       £TYPE
    # define XKTYPE_REAL  £TYPE_REAL
    # define XKDEVICE     xkrt_device_unique_id_t
    # define XKDEF(RTYPE, NAME, ...)                \
        RTYPE xkblas_£##NAME        (__VA_ARGS__);  \
        RTYPE xkblas_£##NAME##_sync (__VA_ARGS__);  \
        RTYPE xkblas_£##NAME##_async(__VA_ARGS__);
    # define XKDEFI(...)
    #  include <xkblas/for-all-routines.h>

    int
    xkblas_£spmv_async(
        const XKTYPE * alpha,
        /* matrix A (in) */
        int transA,
        int index_base,
        int index_type,
        const int nrows,
        const int ncols,
        const size_t nnz,
        const int format,
        const void * csr_row_offsets,
        const void * csr_col_indices,
        const XKTYPE * csr_values,
        /* vector X (in) */
        XKTYPE * X,
        const XKTYPE * beta,
        /* vector Y (inout) */
        XKTYPE * Y
    );

    int
    xkblas_£spmv_sync(
        const XKTYPE * alpha,
        /* matrix A (in) */
        int transA,
        int index_base,
        int index_type,
        const int nrows,
        const int ncols,
        const size_t nnz,
        const int format,
        const void * csr_row_offsets,
        const void * csr_col_indices,
        const XKTYPE * csr_values,
        /* vector X (in) */
        XKTYPE * X,
        const XKTYPE * beta,
        /* vector Y (inout) */
        XKTYPE * Y
    );

    int
    xkblas_£spmv(
        const XKTYPE * alpha,
        /* matrix A (in) */
        int transA,
        int index_base,
        int index_type,
        const int nrows,
        const int ncols,
        const size_t nnz,
        const int format,
        const void * csr_row_offsets,
        const void * csr_col_indices,
        const XKTYPE * csr_values,
        /* vector X (in) */
        XKTYPE * X,
        const XKTYPE * beta,
        /* vector Y (inout) */
        XKTYPE * Y
    );

    # undef XKDEFI
    # undef XKDEF
    # undef XKTYPE
    # undef XKTYPE_REAL
    # undef XKDEVICE

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */

#endif /* __£XKBLAS_ROUTINES_H__ */
