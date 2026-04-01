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

#ifndef __XKBLAS_XKRT_TASK_FORMAT_HPP__
# define __XKBLAS_XKRT_TASK_FORMAT_HPP__

# ifndef ROUTINE_NAME
#  error "Must define `ROUTINE_NAME` before including " __FILE__
# endif

# include <xkblas/routine.hpp>
# include <xkblas/support.h>
# include <xkrt/runtime.h>
# include <xkrt/xkrt.h>

template<auto F>
static inline void
xkblas_routine_device_task(
    runtime_t * runtime,
    device_t * device,
    task_t * task
) {
    int synchronous = 0;
    return xkrt_task_prog_launch(
        (xkrt_runtime_t *) runtime,
        (xkrt_device_t *) device,
        (xkrt_task_t * ) task,
        synchronous,
        (prog_launcher_t) F
    );
}

template<auto F>
static inline void
xkblas_routine_host_task(
    runtime_t * runtime,
    device_t * device,
    task_t * task
) {
    (void) runtime;
    (void) device;

    /* if targeting the host, just run synchronously */
    F(task);
}

# define CONCAT2(a, b) a##b
# define CONCAT(a, b) CONCAT2(a, b)
# define REGISTER_FORMAT_NAME CONCAT(xkblas_t::task_format_create_, ROUTINE_NAME)

TYPED
void
REGISTER_FORMAT_NAME(task_format_t * format)
{
    format->suggest = NULL;

    # if XKBLAS_SUPPORT_CLBLAST && CL
    format->f[XKRT_TASK_FORMAT_TARGET_CL] = (task_format_func_t) xkblas_routine_device_task<cl<P>>;
    # endif /* XKBLAS_SUPPORT_CLBLAST */

    # if XKBLAS_SUPPORT_CUBLAS && CUDA
    format->f[XKRT_TASK_FORMAT_TARGET_CUDA] = (task_format_func_t) xkblas_routine_device_task<cuda<P>>;
    # endif /* XKBLAS_SUPPORT_CUBLAS */

    # if XKBLAS_SUPPORT_HIP && HIP
    format->f[XKRT_TASK_FORMAT_TARGET_HIP] = (task_format_func_t) xkblas_routine_device_task<hip<P>>;
    # endif /* XKBLAS_SUPPORT_HIP */

    # if XKBLAS_SUPPORT_CBLAS && HOST
    format->f[XKRT_TASK_FORMAT_TARGET_HOST] = (task_format_func_t) xkblas_routine_host_task<host<P>>;
    # endif /* XKBLAS_SUPPORT_CBLAS */

    # if XKBLAS_SUPPORT_SYCL && SYCL
    format->f[XKRT_TASK_FORMAT_TARGET_SYCL] = (task_format_func_t) xkblas_routine_device_task<sycl_launch<P>>;
    # endif /* XKBLAS_SUPPORT_SYCL */

    # if XKBLAS_SUPPORT_ZE && ZE
    format->f[XKRT_TASK_FORMAT_TARGET_ZE] = (task_format_func_t) xkblas_routine_device_task<ze<P>>;
    # endif /* XKBLAS_SUPPORT_ZE */
}

# define DEFINE(P) template void REGISTER_FORMAT_NAME<P>(task_format_t * format);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE

// TODO: also instanciate the routines declaration

# undef CONCAT
# undef CONCAT2
# undef REGISTER_FORMAT_NAME

#endif /* __XKBLAS_XKRT_TASK_FORMAT_HPP__ */
