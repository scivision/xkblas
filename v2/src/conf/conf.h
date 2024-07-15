#ifndef __XKBLAS_CONF_H__
# define __XKBLAS_CONF_H__

# include <stdint.h>

# include "logger/todo.h"
# pragma message(TODO "Rename 'cuda' conf variables to something vendor-agnostic")

typedef struct  xkblas_conf_s
{
    uint64_t    stackblocsize;          /* default stack bloc size */
    uint8_t     ngpus;                  /* number of GPU for this node */
    uint32_t    gpu_set;                /* GPU to use */
    uint16_t    cuda_stream_capacity;   /* capacity of input stream */
    uint8_t     cuda_conc_stream_kernel;/* number of concurrent cuda kernel stream per device*/
    uint8_t     cuda_conc_kernel;       /* number of pending kernel per kernel stream */
    uint8_t     cuda_conc_h2d;          /* number of concurrent cuda h2d stream per device*/
    uint8_t     cuda_conc_d2h;          /* number of concurrent cuda d2h stream per device*/
    uint8_t     cuda_conc_d2d;          /* number of concurrent cuda d2d stream per device*/
    float       cuda_cache_limit;       /* percent reserved for cache */
}               xkblas_conf_t;

void xkblas_init_conf(xkblas_conf_t * conf);

#endif /* __XKBLAS_CONF_H__ */
