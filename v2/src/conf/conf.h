#ifndef __XKBLAS_CONF_H__
# define __XKBLAS_CONF_H__

# include <stdint.h>

# include "logger/todo.h"
# include "device/stream.h"

# pragma message(TODO "Rename 'cuda' conf variables to something vendor-agnostic")

//////////////////
//  DEVICE CONF //
//////////////////

typedef struct  xkblas_conf_stream_t
{
    /* number of stream per operation (<=> cuda stream) */
    uint8_t n;

    /* number of concurrent operations */
    uint8_t concurrency;

}               xkblas_conf_stream_t;

typedef struct  xkblas_conf_offloader_t
{
    xkblas_conf_stream_t streams[XKBLAS_STREAM_TYPE_ALL];
    uint16_t capacity;

}               xkblas_conf_offloader_t;

typedef struct  xkblas_conf_device_t
{
    xkblas_conf_offloader_t offloader;

}               xkblas_conf_device_t;

//////////////////
//  KERNEL CONF //
//////////////////

typedef struct  xkblas_conf_kernels_kernel_t
{
    int tile[2];

}               xkblas_conf_kernels_kernel_t;

typedef struct  xkblas_conf_kernels_t
{
    xkblas_conf_kernels_kernel_t gemm;

}               xkblas_conf_kernels_t;


//////////////////////////////////////////////////////////////////

typedef struct  xkblas_conf_s
{
    uint64_t    stackblocsize;      /* default stack bloc size */
    uint8_t     ngpus;              /* number of GPU for this node */
    uint32_t    gpu_set;            /* GPU to use */
    float cuda_cache_limit;

    xkblas_conf_device_t device;    /* device conf */
    xkblas_conf_kernels_t kernels;  /* kernels conf */

}               xkblas_conf_t;

void xkblas_init_conf(xkblas_conf_t * conf);

#endif /* __XKBLAS_CONF_H__ */
