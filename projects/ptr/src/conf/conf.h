/* ************************************************************************** */
/*                                                                            */
/*   conf.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:47 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKBLAS_CONF_H__
# define __XKBLAS_CONF_H__

# include <stdint.h>

# include "xkblas-kernel-type.h"
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

typedef struct  xkblas_conf_kernel_t
{
    size_t tile;
}               xkblas_conf_kernel_t;

//////////////////////////////////////////////////////////////////

typedef struct  xkblas_conf_s
{
    uint64_t    stackblocsize;      /* default stack bloc size */
    uint8_t     ngpus;              /* number of GPU for this node */
    uint32_t    gpu_set;            /* GPU to use */
    float       gpu_mem_percent;    /* % of gpu memory to allocate initially */

    xkblas_conf_device_t device;                            /* device conf */
    xkblas_conf_kernel_t kernels[XKBLAS_KERNEL_TYPE_MAX];   /* kernels conf */

    bool merge_transfers;

}               xkblas_conf_t;

void xkblas_init_conf(xkblas_conf_t * conf);

#endif /* __XKBLAS_CONF_H__ */
