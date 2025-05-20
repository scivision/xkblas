/* ************************************************************************** */
/*                                                                            */
/*   driver.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/02 14:42:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DRIVER_H__
# define __DRIVER_H__

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>     /* cpu_set_t */
# include <stdint.h>    /* uint64_t */

# include <xkrt/consts.h>
# include <xkrt/driver/device.hpp>
# include <xkrt/driver/driver-type.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/metric.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mutex.h>

# include <hwloc.h>

typedef enum    xkrt_driver_module_format_t
{
    XKRT_DRIVER_MODULE_FORMAT_SPIRV,
    XKRT_DRIVER_MODULE_FORMAT_NATIVE,
    XKRT_DRIVER_MODULE_FORMAT_UNKWN
}               xkrt_driver_module_format_t;

typedef void * xkrt_driver_module_t;
typedef void * xkrt_driver_module_fn_t;

typedef struct  xkrt_driver_t
{
    /* type */
    xkrt_driver_type_t type;

    /* driver team */
    xkrt_team_t team;

    /* a barrier to synchronize all threads of the team and the main thread */
    pthread_barrier_t barrier;

    /* devices */
    xkrt_device_t * devices[XKRT_DEVICES_MAX];

    /* number of devices commited */
    std::atomic<int> ndevices_commited;

    /////////////////////////////////////
    //  API TO IMPLEMENT BY THE DRIVER //
    /////////////////////////////////////

    ///////////////////////
    //  DRIVER META DATA //
    ///////////////////////
    const char   *(*f_get_name)(void);          /* name of the driver (human-readable) */
    unsigned int (*f_get_ndevices_max)(void);   /* return the number of devices available to the driver */

    ///////////////////////
    //  DRIVER LIFECYCLE //
    ///////////////////////
    int (*f_init)(unsigned int ndevices, bool use_p2p);
    void (*f_finalize)(void);

    /////////////////////////////////
    //  DEVICES MANAGEMENT         //
    /////////////////////////////////

    /* Create a device for the given driver id */
    xkrt_device_t * (*f_device_create)(xkrt_driver_t *, int);

    /* initialize device */
    void (*f_device_init)(int device_driver_id);

    /* commit device (called once all devices of that driver had been initialized) */
    int (*f_device_commit)(int device_driver_id, xkrt_device_global_id_bitfield_t * affinity);

    /* Release a device */
    int (*f_device_destroy)(int device_driver_id);

    /* get device infos */
    void (*f_device_info)(int device_driver_id, char * buffer, size_t size);

    ////////////////////////////////
    //  MEMORY MANAGEMENT         //
    ////////////////////////////////

    /* retrieve memory infos */
    void   (*f_memory_device_info)(int device_driver_id, xkrt_device_memory_info_t info[XKRT_DEVICE_MEMORIES_MAX], int * nmemories);

    /* allocate device memory */
    void * (*f_memory_device_allocate)(int device_driver_id, const size_t size, int area_idx);
    void   (*f_memory_device_deallocate)(int device_driver_id, void * ptr, const size_t size, int area_idx);

    /* allocate host memory */
    void * (*f_memory_host_allocate)(int device_driver_id, const size_t size);
    void   (*f_memory_host_deallocate)(int device_driver_id, void * mem, const size_t size);

    /* allocate unified memory */
    void * (*f_memory_unified_allocate)(int device_driver_id, const size_t size);
    void   (*f_memory_unified_deallocate)(int device_driver_id, void * mem, const size_t size);

    /* register host memory */
    int    (*f_memory_host_register)(void * mem, uint64_t size);
    int    (*f_memory_host_unregister)(void * mem, uint64_t size);

    ///////////////
    // THREADING //
    ///////////////

    /* Get a cpuset of cpus with the best affinity for the given device */
    int (*f_device_cpuset)(hwloc_topology_t, cpu_set_t*, int);

    ////////////////////////////////
    // STREAM MANAGEMENT          //
    ////////////////////////////////

    /* suggest a number of stream to use for the given stream type */
    int (*f_stream_suggest)(int device_driver_id, xkrt_stream_type_t stype);

    /* alllocate and initialize a stream */
    xkrt_stream_t * (*f_stream_create)(xkrt_device_t * device, xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity);

    /* deallocate a stream */
    void (*f_stream_delete)(xkrt_stream_t * istream);

    ///////////////////
    //  P2P ROUTING  //
    ///////////////////

    // TODO

    /////////////
    // MODULES //
    /////////////
    xkrt_driver_module_t    (*f_module_load)(int device_driver_id, uint8_t * bin, size_t binsize, xkrt_driver_module_format_t format);
    void                    (*f_module_unload)(xkrt_driver_module_t module);
    xkrt_driver_module_fn_t (*f_module_get_fn)(xkrt_driver_module_t module, const char * name);

    /////////////////////
    // ENERGY COUNTER  //
    /////////////////////

    /* start power recording on the given device */
    void (*f_power_start)(int device_driver_id, xkrt_power_t * pwr);

    /* return time elapsed (in s.) and the power (in Watt) since last 'f_energy_power_start' call */
    void (*f_power_stop)(int device_driver_id, xkrt_power_t * pwr);

}               xkrt_driver_t;

extern "C"
xkrt_device_t * xkrt_driver_device_get(xkrt_driver_t * driver, xkrt_device_global_id_t driver_device_id);

/* one function per task per driver */
static_assert((uint8_t)XKRT_DRIVER_TYPE_MAX <= (uint8_t)TASK_FORMAT_TARGET_MAX);

typedef struct  xkrt_drivers_t
{
    /* list of drivers */
    xkrt_driver_t * list[XKRT_DRIVER_TYPE_MAX];

    struct {

        /* list of devices */
        xkrt_device_t * list[XKRT_DEVICES_MAX];

        /* number of devices */
        std::atomic<uint8_t> n;

        /* next device id to use */
        std::atomic<uint8_t> next_id;

        /* next worker to offload round robin mode */
        std::atomic<uint8_t> round_robin_device_global_id;

    } devices;

}               xkrt_drivers_t;

#endif /* __DRIVER_H__ */
