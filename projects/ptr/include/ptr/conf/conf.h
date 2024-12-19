/* ************************************************************************** */
/*                                                                            */
/*   conf.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:00:06 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __PTR_CONF_H__
# define __PTR_CONF_H__

# include <stdint.h>

# include <ptr/logger/todo.h>
# include <ptr/device/stream.h>

# pragma message(TODO "Rename 'cuda' conf variables to something vendor-agnostic")

//////////////////
//  DEVICE CONF //
//////////////////

typedef struct  ptr_conf_stream_t
{
    /* number of stream per operation (<=> cuda stream) */
    uint8_t n;

    /* number of concurrent operations */
    uint8_t concurrency;

}               ptr_conf_stream_t;

typedef struct  ptr_conf_offloader_t
{
    ptr_conf_stream_t streams[PTR_STREAM_TYPE_ALL];
    uint16_t capacity;

}               ptr_conf_offloader_t;

typedef struct  ptr_conf_device_t
{
    ptr_conf_offloader_t offloader;

}               ptr_conf_device_t;

//////////////////////////////////////////////////////////////////

typedef struct  ptr_conf_s
{
    uint64_t    stackblocsize;      /* default stack bloc size */
    uint8_t     ngpus;              /* number of GPU for this node */
    uint32_t    gpu_set;            /* GPU to use */
    float       gpu_mem_percent;    /* % of gpu memory to allocate initially */

    ptr_conf_device_t device;    /* device conf */

    bool merge_transfers;           /* attempt to merge continuous memory to a single transfer */

}               ptr_conf_t;

void ptr_init_conf(ptr_conf_t * conf);

#endif /* __PTR_CONF_H__ */
