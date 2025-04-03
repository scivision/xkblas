/* ************************************************************************** */
/*                                                                            */
/*   conf.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/02 22:50:29 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_CONF_H__
# define __XKRT_CONF_H__

# include <stdint.h>

# include <xkrt/logger/todo.h>
# include <xkrt/driver/stream.h>

# pragma message(TODO "Rename 'cuda' conf variables to something vendor-agnostic")

//////////////////
//  DEVICE CONF //
//////////////////

typedef struct  xkrt_conf_stream_t
{
    /* number of stream per operation (<=> cuda stream) */
    int8_t n;

    /* number of concurrent operations */
    int8_t concurrency;

}               xkrt_conf_stream_t;

typedef struct  xkrt_conf_offloader_t
{
    xkrt_conf_stream_t streams[XKRT_STREAM_TYPE_ALL];
    uint16_t capacity;
    uint8_t nthreads_per_device;

}               xkrt_conf_offloader_t;

typedef struct  xkrt_conf_device_t
{
    float gpu_mem_percent;              /* % of gpu memory to allocate initially */
    int ngpus;                          /* number of GPU for this node */
    xkrt_conf_offloader_t offloader;    /* offloader conf */
}               xkrt_conf_device_t;

//////////////////////////////////////////////////////////////////

typedef struct  xkrt_conf_s
{
    xkrt_conf_device_t device;      /* device conf */
    bool merge_transfers;           /* attempt to merge continuous memory to a single transfer */
    bool report_stats_on_deinit;    /* report stats on deinit */
    int drivers_mask;               /* bitmask to enable/disable drivers */
}               xkrt_conf_t;

void xkrt_init_conf(xkrt_conf_t * conf);

#endif /* __XKRT_CONF_H__ */
