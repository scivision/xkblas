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

#ifndef __IMPL_HPP__
# define __IMPL_HPP__

#ifndef TYPE
# error "Must include a type file before including " __FILE__
#endif

# include <assert.h>

class impl_t
{
    public:

        /* impl name */
        const char * name(void) const;

        /* init/deinit routines */
        void init(void);
        void deinit(void);

        uintptr_t alloc(size_t size);
        void pin_async(void * ptr, size_t size);
        void pin_wait(void);

        /* kernels (async) */
        void gemm(
            int transa, int transb,
            int m, int n, int k,
            const TYPE * alpha,
            const TYPE * A, int lda,
            const TYPE * B, int ldb,
            const TYPE * beta,
                  TYPE * C, int ldc
        );

	    void gemmt(
            int uplo, int transa, int transb,
            int m, int k,
            const TYPE * alpha,
            const TYPE * A, int lda,
            const TYPE * B, int ldb,
            const TYPE * beta,
                  TYPE * C, int ldc
	    );

        void
        trsm(
            int side, int uplo,
            CBLAS_TRANSPOSE transA, int diag,
            const BLAS_INT m, const BLAS_INT n,
            const TYPE * alpha,
            const TYPE * A, const BLAS_INT lda,
                  TYPE * B, const BLAS_INT ldb
        );

        void
        syrk(
            int uplo, int trans,
            int n, int k,
            const TYPE * alpha,
            const TYPE * A, int lda,
            const TYPE * beta,
                  TYPE * C, int ldc
        );

        void
        copyscale(
            const BLAS_INT m, const BLAS_INT n,
            bool should_copy, int * IW,
                  TYPE * D, const int ldd,
                  TYPE * L, const int ldl,
                  TYPE * U, const int ldu
        );

        void
        coherent(
            TYPE * M,
            int m, int n,
            int ld
        );

        void
        replicate(
            TYPE * M,
            int m, int n,
            int ld
        );

        void
        preallocate(
            TYPE * M,
            int m, int n,
            int ld
        );

        void
        set_tile(int ts);

        /* wait for the completion of previously sent operations */
        void wait(void);

        /* invalidate all memory replicates, reset the runtime */
        void reset(void);

}; /* impl_t */

#endif /* __IMPL_HPP__ */
