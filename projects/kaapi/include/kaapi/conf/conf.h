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

#ifndef __KAAPI_CONF_H__
# define __KAAPI_CONF_H__

# include <stdint.h>

# include <kaapi/logger/todo.h>
# include <kaapi/device/stream.h>

# pragma message(TODO "Rename 'cuda' conf variables to something vendor-agnostic")

//////////////////
//  DEVICE CONF //
//////////////////

typedef struct  kaapi_conf_stream_t
{
    /* number of stream per operation (<=> cuda stream) */
    uint8_t n;

    /* number of concurrent operations */
    uint8_t concurrency;

}               kaapi_conf_stream_t;

typedef struct  kaapi_conf_offloader_t
{
    kaapi_conf_stream_t streams[KAAPI_STREAM_TYPE_ALL];
    uint16_t capacity;

}               kaapi_conf_offloader_t;

typedef struct  kaapi_conf_device_t
{
    kaapi_conf_offloader_t offloader;

}               kaapi_conf_device_t;

//////////////////////////////////////////////////////////////////

typedef struct  kaapi_conf_s
{
    uint64_t    stackblocsize;      /* default stack bloc size */
    uint8_t     ngpus;              /* number of GPU for this node */
    uint32_t    gpu_set;            /* GPU to use */
    float       gpu_mem_percent;    /* % of gpu memory to allocate initially */

    kaapi_conf_device_t device;    /* device conf */

    bool merge_transfers;           /* attempt to merge continuous memory to a single transfer */

}               kaapi_conf_t;

void kaapi_init_conf(kaapi_conf_t * conf);

#endif /* __KAAPI_CONF_H__ */
