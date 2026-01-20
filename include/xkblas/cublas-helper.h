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

#ifndef __CUBLAS_HELPER_H__
# define __CUBLAS_HELPER_H__

# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/logger-cublas.h>

#  define XKBLAS_CUBLAS_CALL_POST()                                                         \
    do {                                                                                    \
        CU_SAFE_CALL(cuEventRecord(queue->cu.events.buffer[idx], queue->cu.handle.high)); \
    } while (0)

# define XKBLAS_CUBLAS_CALL(CALL)       \
    do {                                \
        CUBLAS_SAFE_CALL(CALL);         \
        XKBLAS_CUBLAS_CALL_POST();      \
    } while (0)

# define XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, PX, T) \
    if constexpr (P == xkblas_precision_t::PX) cuda_run<P, cublas##PX##NAME, T>(runtime, device, task, queue, cmd, idx);

# define XKBLAS_CUBLAS_DISPATCH_PRECISION_REAL(NAME)            \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, S, float)          \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, D, double)

# define XKBLAS_CUBLAS_DISPATCH_PRECISION_COMPLEX(NAME)         \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, C, cuComplex)      \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_P(NAME, Z, cuDoubleComplex)

# define XKBLAS_CUBLAS_DISPATCH_PRECISION(NAME)     \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_REAL(NAME)     \
    XKBLAS_CUBLAS_DISPATCH_PRECISION_COMPLEX(NAME)

# include "xkblas/cblas.h"

/* device pointer to constant, to avoid copy parameters from host to device */
# define XKBLAS_CUBLAS_FOREACH_CONST(F)  \
    F(-2.0,  MTWO)                       \
    F(-1.0,  MONE)                       \
    F(-0.5, MHALF)                       \
    F(-0.0, MZERO)                       \
    F( 0.0,  ZERO)                       \
    F( 0.5,  HALF)                       \
    F( 1.0,   ONE)                       \
    F( 2.0,   TWO)

# include <xkblas/routine.h>

typedef enum    xkblas_cublas_const_t
{
    # define F(VALUE, NAME) XKBLAS_CUBLAS_CONST_##NAME,
    XKBLAS_CUBLAS_FOREACH_CONST(F)
    # undef F
    XKBLAS_CUBLAS_CONST_MAX
}               xkblas_cublas_const_t;

extern float            XKBLAS_CUBLAS_HOST_CONST_S[XKBLAS_CUBLAS_CONST_MAX];
extern double           XKBLAS_CUBLAS_HOST_CONST_D[XKBLAS_CUBLAS_CONST_MAX];
extern cuComplex        XKBLAS_CUBLAS_HOST_CONST_C[XKBLAS_CUBLAS_CONST_MAX];
extern cuDoubleComplex  XKBLAS_CUBLAS_HOST_CONST_Z[XKBLAS_CUBLAS_CONST_MAX];

extern float            * XKBLAS_CUBLAS_DEVICE_CONST_S[XKRT_DEVICES_MAX];
extern double           * XKBLAS_CUBLAS_DEVICE_CONST_D[XKRT_DEVICES_MAX];
extern cuComplex        * XKBLAS_CUBLAS_DEVICE_CONST_C[XKRT_DEVICES_MAX];
extern cuDoubleComplex  * XKBLAS_CUBLAS_DEVICE_CONST_Z[XKRT_DEVICES_MAX];

# include <xkblas/xkblas.hpp>
# include <xkrt/consts.h>

template <xkblas_precision_t P>
static inline const TYPE * xkblas_cublas_pointer_mode(
    xkblas_t * xkblas,
    const xkrt::device_global_id_t device_global_id,
    cublasHandle_t handle,
    const TYPE * value
) {
    assert(device_global_id >= 0 && device_global_id < XKRT_DEVICES_MAX);
    static std::mutex mtxs[XKRT_DEVICES_MAX];

    volatile TYPE ** p_dst;
    TYPE  * src;
    # define FUNC(PX) if constexpr(P == xkblas_precision_t::PX) { p_dst = (volatile TYPE **) &XKBLAS_CUBLAS_DEVICE_CONST_##PX[device_global_id] ; src = (TYPE *) &XKBLAS_CUBLAS_HOST_CONST_##PX[0]; }
    XKBLAS_FORALL_PRECISIONS(FUNC)
    # undef FUNC

    # define FUNC(VALUE, NAME)                                                                                          \
        if (*value == (TYPE) VALUE)                                                                                     \
        {                                                                                                               \
            std::mutex * mtx = mtxs + device_global_id;                                                                 \
            mtx->lock();                                                                                                \
            if (*p_dst == NULL)                                                                                         \
            {                                                                                                           \
                const size_t buffer_size = sizeof(TYPE) * XKBLAS_CUBLAS_CONST_MAX;                                      \
                *p_dst = (volatile TYPE *) xkblas->runtime.memory_device_allocate(device_global_id, buffer_size)->ptr;  \
                assert(*p_dst);                                                                                         \
                CUstream stream;                                                                                        \
                CUBLAS_SAFE_CALL(cublasGetStream(handle, &stream));                                                     \
                CU_SAFE_CALL(cuMemcpyHtoDAsync((CUdeviceptr) *p_dst, src, buffer_size, stream));                        \
            }                                                                                                           \
            mtx->unlock();                                                                                              \
            cublasSetPointerMode(handle, CUBLAS_POINTER_MODE_DEVICE);                                                   \
            return (const TYPE *) ((*p_dst) + XKBLAS_CUBLAS_CONST_##NAME);                                              \
        }
    XKBLAS_CUBLAS_FOREACH_CONST(FUNC)
    # undef FUNC

    cublasSetPointerMode(handle, CUBLAS_POINTER_MODE_HOST);
    return value;
}


static inline cublasOperation_t
cblas2cublas_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return CUBLAS_OP_N;
        case CblasTrans:        return CUBLAS_OP_T;
        case CblasConjTrans:    return CUBLAS_OP_C;
    }
    LOGGER_FATAL("Unknown trans code");
    abort();
}

static inline cublasSideMode_t
cblas2cublas_side(int side)
{
    switch (side)
    {
        case CblasLeft:     return CUBLAS_SIDE_LEFT;
        case CblasRight:    return CUBLAS_SIDE_RIGHT;
    }
    LOGGER_FATAL("Unknown side code");
    abort();
}

static inline
cublasFillMode_t cblas2cublas_uplo( int uplo )
{
    switch (uplo)
    {
        case CblasUpper:    return CUBLAS_FILL_MODE_UPPER;
        case CblasLower:    return CUBLAS_FILL_MODE_LOWER;
    }
    LOGGER_FATAL("Unknown uplo code");
    abort();
}

static inline
cublasDiagType_t cblas2cublas_diag(int diag)
{
    switch (diag)
    {
        case CblasNonUnit:  return CUBLAS_DIAG_NON_UNIT;
        case CblasUnit:     return CUBLAS_DIAG_UNIT;
    }
    LOGGER_FATAL("Unknown diag code");
    abort();
}

#endif /* __CUBLAS_HELPER_H__ */
