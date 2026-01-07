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

# include <xkblas/xkblas.h>
# include <xkblas/flops.h>
# include <xkblas/cblas.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdlib.h>
# include <stdint.h>
# include <string.h>

# if 1
# define TYPE           float
# define xkblas_axpy    xkblas_saxpy
# define VALUE          42.0f
# endif
# if 0
# define TYPE           double
# define xkblas_axpy    xkblas_daxpy
# define VALUE          42.0
# endif
# if 0
# define TYPE           _Complex float
# define xkblas_axpy    xkblas_caxpy
# define VALUE          42.0f + 13.0f * I
# endif
# if 0
# define TYPE           _Complex double
# define xkblas_axpy    xkblas_zaxpy
# define VALUE          42.0 + 13 * I
# endif

int
main(void)
{
    xkblas_init();

    const int n = 1024;

    TYPE * X = (TYPE *) malloc(n * sizeof(TYPE));
    TYPE * Y = (TYPE *) malloc(n * sizeof(TYPE));

    for (int i = 0 ; i < n ; ++i)
    {
        X[i] = (TYPE) i;
        Y[i] = (TYPE) (2 * i);
    }

    const TYPE alpha = 1.0f;
    const int incx = 1;
    const int incy = 1;
    xkblas_axpy(n, &alpha, X, incx, Y, incy);

    for (int i = 0 ; i < n ; ++i)
    {
        TYPE x = (TYPE) i;
        TYPE y = (TYPE) (2 * i);
        assert(Y[i] == alpha * x + y);
    }

    xkblas_deinit();

    return 0;
}
