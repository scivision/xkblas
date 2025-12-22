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

# ifndef __XKBLAS_ROUTINE_DECL_H__
#  define __XKBLAS_ROUTINE_DECL_H__

# include <stdint.h>

// Define the macro list of precisions
# define XKBLAS_FORALL_PRECISIONS(F) \
    F(S)                             \
    F(C)                             \
    F(D)                             \
    F(Z)

typedef enum    xkblas_precision_t
{
    # define DEFINE_ENUM(name) name,
    XKBLAS_FORALL_PRECISIONS(DEFINE_ENUM)
    # undef DEFINE_ENUM

    XKBLAS_PRECISION_MAX

}               xkblas_precision_t;

// Define the macro list of index type
# define XKBLAS_FORALL_INDEX(F) \
    F(I32)                      \
    F(I64)

typedef enum    xkblas_index_t
{
    # define DEFINE_ENUM(name) name,
    XKBLAS_FORALL_INDEX(DEFINE_ENUM)
    # undef DEFINE_ENUM

    XKBLAS_INDEX_MAX

}               xkblas_index_t;

/* for all precisions and index types */
# define XKBLAS_FORALL_PRECISIONS_AND_INDEX(F)  \
    F(S, I32)                                   \
    F(C, I32)                                   \
    F(D, I32)                                   \
    F(Z, I32)                                   \
    F(S, I64)                                   \
    F(C, I64)                                   \
    F(D, I64)                                   \
    F(Z, I64)

// Define the macro list of kernels
#define XKBLAS_FORALL_ROUTINES(F) \
    /* LEVEL 1 */                 \
    F(AXPBY)                      \
    F(AXPY)                       \
    F(COPY)                       \
    F(DOT)                        \
    F(FILL)                       \
    F(SCALCOPY)                   \
    F(SCAL)                       \
                                  \
    /* LEVEL 2 */                 \
    F(COPYSCALE)                  \
    F(GEMV)                       \
                                  \
    /* LEVEL 3 */                 \
    F(GEMM)                       \
    F(GEMMT)                      \
    F(HERK)                       \
    F(SYMM)                       \
    F(SYR2K)                      \
    F(SYRK)                       \
    F(TRSM)                       \
    F(TRMM)                       \
                                  \
    /* LAPACKE */                 \
    F(POTRF)                      \
                                  \
    /* SPARSE */                  \
    F(SPMV)

// Now define the enum using the macro
typedef enum    xkblas_routine_t
{
    # define DEFINE_ENUM(name) name,
    XKBLAS_FORALL_ROUTINES(DEFINE_ENUM)
    # undef DEFINE_ENUM

   XKBLAS_ROUTINE_MAX

}               xkblas_routine_t;

# define XKBLAS_FORALL_PRECISIONS_AND_ROUTINES(F)    \
    /* LEVEL 1 */                                   \
    F(S,     DOT)                                   \
    F(D,     DOT)                                   \
    F(S,     FILL)                                  \
    F(C,     FILL)                                  \
    F(D,     FILL)                                  \
    F(Z,     FILL)                                  \
    F(S,     SCAL)                                  \
    F(D,     SCAL)                                  \
    F(S,     AXPY)                                  \
    F(C,     AXPY)                                  \
    F(D,     AXPY)                                  \
    F(Z,     AXPY)                                  \
    F(S,     COPY)                                  \
    F(C,     COPY)                                  \
    F(D,     COPY)                                  \
    F(Z,     COPY)                                  \
    F(S,     AXPBY)                                 \
    F(C,     AXPBY)                                 \
    F(D,     AXPBY)                                 \
    F(Z,     AXPBY)                                 \
                                                    \
    /* LEVEL 2 */                                   \
    F(S,     COPYSCALE)                             \
    F(C,     COPYSCALE)                             \
    F(D,     COPYSCALE)                             \
    F(Z,     COPYSCALE)                             \
    F(S,     GEMV)                                  \
    F(C,     GEMV)                                  \
    F(D,     GEMV)                                  \
    F(Z,     GEMV)                                  \
                                                    \
    /* LEVEL 3 */                                   \
    F(S,     GEMM)                                  \
    F(C,     GEMM)                                  \
    F(D,     GEMM)                                  \
    F(Z,     GEMM)                                  \
    F(S,     GEMMT)                                 \
    F(C,     GEMMT)                                 \
    F(D,     GEMMT)                                 \
    F(Z,     GEMMT)                                 \
    F(C,     HERK)                                  \
    F(Z,     HERK)                                  \
    F(S,     SYMM)                                  \
    F(C,     SYMM)                                  \
    F(D,     SYMM)                                  \
    F(Z,     SYMM)                                  \
    F(S,     SYR2K)                                 \
    F(C,     SYR2K)                                 \
    F(D,     SYR2K)                                 \
    F(Z,     SYR2K)                                 \
    F(S,     SYRK)                                  \
    F(C,     SYRK)                                  \
    F(D,     SYRK)                                  \
    F(Z,     SYRK)                                  \
    F(S,     TRSM)                                  \
    F(C,     TRSM)                                  \
    F(D,     TRSM)                                  \
    F(Z,     TRSM)                                  \
    F(S,     TRMM)                                  \
    F(C,     TRMM)                                  \
    F(D,     TRMM)                                  \
    F(Z,     TRMM)                                  \
                                                    \
   /* lapacke */                                    \
   F(S,      POTRF)                                 \
   F(C,      POTRF)                                 \
   F(D,      POTRF)                                 \
   F(Z,      POTRF)                                 \
                                                    \
   /* sparse */                                     \
   F(S,      SPMV)                                  \
   F(C,      SPMV)                                  \
   F(D,      SPMV)                                  \
   F(Z,      SPMV)

# endif /* __XKBLAS_ROUTINE_DECL_H__ */
