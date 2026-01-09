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
# include <xkrt/logger/logger.h>

# include <assert.h>
# include <math.h>

# if 0
static int
is_power_of_two(int n)
{
    return n && (n & (n - 1)) == 0;
}
# endif

void
xkblas_routine_auto_tile(
    xkblas_routine_t kernel,
    int * args,
    int * ts
) {
    xkblas_t * context = xkblas_get();
    assert(context);

    const int ngpus = context->runtime.drivers.devices.n - 1;
    const double factor = 64.0;

    int ts_auto = 0;
    switch (kernel)
    {
        case (AXPY):
        case (COPY):
        case (DOT):
        case (FILL):
        case (GEMV):
        case (SCAL):
        {
            ts_auto = 2048;
            break ;
        }

        case (GEMM):
        case (TRSM):
        case (COPYSCALE):
        {
            int m = args[0];
            int n = args[1];

            ts_auto = (int) ceil(sqrt((double)m*(double)n / (factor * (double)ngpus)));
            # define MIN_BS 2048
            if (ts_auto < MIN_BS)
                ts_auto = MIN_BS;

            break ;
        }

        case (SPMV):
        {
            int m = args[0];
            ts_auto = m;
            break ;
        }

        default:
        {
            LOGGER_FATAL("Tile for kernel type %d is not implemented", kernel);
            break ;
        }
    }

    *ts = ts_auto;

    LOGGER_DEBUG("Return tile size = %d", *ts);
}
