/* ************************************************************************** */
/*                                                                            */
/*   conf.h                                                       .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 18:31:21 by Romain PEREIRA          __/_*_*(_        */
<<<<<<< HEAD:projects/xkrt_deprecated/include/xkrt/conf/conf.h
/*   Updated: 2025/06/04 19:56:10 by Romain PEREIRA         / _______ \       */
=======
/*   Updated: 2025/06/04 23:09:36 by Romain PEREIRA         / _______ \       */
>>>>>>> fc356b3a952cec28a52dcc5dd79c6e5b7a5bea7b:projects/xkrt/include/xkrt/conf/conf.h
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_CONF_H__
# define __XKRT_CONF_H__

# include <stdint.h>

# include <xkrt/logger/todo.h>
# include <xkrt/driver/driver-type.h>
# include <xkrt/driver/stream.h>

//////////////////
//  DEVICE CONF //
//////////////////

typedef struct  xkrt_conf_stream_t
{
    /* number of stream per operation (<=> cuda stream) */
    int8_t n;

    /* number of concurrent operations */
    uint32_t concurrency;

}               xkrt_conf_stream_t;

typedef struct  xkrt_conf_offloader_t
{
    xkrt_conf_stream_t streams[XKRT_STREAM_TYPE_ALL];
    uint16_t capacity;

}               xkrt_conf_offloader_t;

typedef struct  xkrt_conf_device_t
{
    float gpu_mem_percent;              /* % of gpu memory to allocate initially */
    int ngpus;                          /* number of GPU for this node */
    bool use_p2p;                       /* enable/disable p2p */
    xkrt_conf_offloader_t offloader;    /* offloader conf */
}               xkrt_conf_device_t;

typedef struct  xkrt_conf_driver_t
{
    int nthreads_per_device;
    int used;
}               xkrt_conf_driver_t;

typedef struct  xkrt_conf_drivers_t
{
    xkrt_conf_driver_t list[XKRT_DRIVER_TYPE_MAX];

}               xkrt_conf_drivers_t;

//////////////////////////////////////////////////////////////////

typedef struct  xkrt_conf_s
{
    xkrt_conf_device_t device;      /* device conf */
    xkrt_conf_drivers_t drivers;    /* driver conf */
    bool merge_transfers;           /* attempt to merge continuous memory to a single transfer */
    bool report_stats_on_deinit;    /* report stats on deinit */
<<<<<<< HEAD:projects/xkrt_deprecated/include/xkrt/conf/conf.h
    bool export_tdg_on_deinit;      /* export tdg on deinit */
=======

    /* keep track of registered memory, and split transfers for each registered
     * segment to avoid cuda crashing while transfering memory that is
     * partially registered */
    bool protect_registered_memory_overflow;

>>>>>>> fc356b3a952cec28a52dcc5dd79c6e5b7a5bea7b:projects/xkrt/include/xkrt/conf/conf.h
}               xkrt_conf_t;

void xkrt_init_conf(xkrt_conf_t * conf);

#endif /* __XKRT_CONF_H__ */
