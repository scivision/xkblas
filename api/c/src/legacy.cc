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
# include <xkblas/xkblas.h>

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>

# include <assert.h>

extern "C"
void
xkblas_set_modemath(xkblas_mode_math_t mode)
{
    LOGGER_FATAL("Not implemented");
}

extern "C"
int
xkblas_get_ngpus(void)
{
    xkblas_t * context = xkblas_get();
    assert(context);

    return context->runtime.get_ndevices_max();
}

extern "C"
int
xkblas_get_device_count(int * count)
{
    LOGGER_IMPL("RETURNED 1 lol");
    *count = 1;
    return 0;

    LOGGER_FATAL("Not implemented");
    return -1;
}

extern "C"
void
xkblas_set_param(size_t nb, size_t p)
{
    (void) p;
//    LOGGER_IMPL("`p` unused");

    xkblas_t * context = xkblas_get();
    assert(context);

    for (int i = 0 ; i < XKBLAS_KERNEL_MAX ; ++i)
        context->conf.kernels[i].tile = nb;
}

extern "C"
void
xkblas_set_tile_parameter(xkblas_kernel_t kernel, size_t ts)
{
    xkblas_t * context = xkblas_get();
    assert(context);

    context->conf.kernels[kernel].tile = ts;
}

extern "C"
void
xkblas_finalize(void)
{
    xkblas_deinit();
}
