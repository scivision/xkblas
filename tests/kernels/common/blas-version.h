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

#ifndef __BLAS_VERSION_H__
# define __BLAS_VERSION_H__

#if USE_OPENBLAS
# include <lapacke.h>
#endif

# if 0
#  define PRECISION_s 1
#  define PRECISION xkblas_precision_t::S
#  define BLAS_INT long long int
#  define native_gemm(...)          cblas_sgemm(__VA_ARGS__)
#  define native_axpy(...)          cblas_saxpy(__VA_ARGS__)
#  define native_syrk(...)          cblas_ssyrk(__VA_ARGS__)
#  define native_trsm(...)          cblas_strsm(__VA_ARGS__)
#  define native_lange_work(...)    LAPACKE_slange_work(__VA_ARGS__)
#  define native_lantr_work(...)    LAPACKE_slantr_work(__VA_ARGS__)
#  define native_larnv_work(...)    LAPACKE_slarnv_work(__VA_ARGS__)
# else
#  define PRECISION_d 1
#  define PRECISION xkblas_precision_t::D
#  define BLAS_INT long long int
#  define native_gemm(...)          cblas_dgemm(__VA_ARGS__)
#  define native_axpy(...)          cblas_daxpy(__VA_ARGS__)
#  define native_syrk(...)          cblas_dsyrk(__VA_ARGS__)
#  define native_trsm(...)          cblas_dtrsm(__VA_ARGS__)
#  define native_lange_work(...)    LAPACKE_dlange_work(__VA_ARGS__)
#  define native_lantr_work(...)    LAPACKE_dlantr_work(__VA_ARGS__)
#  define native_larnv_work(...)    LAPACKE_dlarnv_work(__VA_ARGS__)
#  define native_larnv(...)         LAPACKE_dlarnv(__VA_ARGS__)
# endif

// 1: uniform (0,1), 2: normal (0,1), 3: uniform (-1,1)
// Seed for the random number generator (4 integers)
# define FILL(PTR, N)                               \
    do {                                            \
        lapack_int seed[4] = { 1, 2, 3, 4 };        \
        native_larnv_work(1, seed, N, PTR);         \
    } while (0)
#endif
