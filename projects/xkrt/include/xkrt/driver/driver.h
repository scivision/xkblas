/* ************************************************************************** */
/*                                                                            */
/*   driver.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:48:24 by Romain PEREIRA            \_)     (_/    */
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
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mutex.h>

# include <hwloc.h>

typedef struct  xkrt_driver_t
{
    /* type */
    xkrt_driver_type_t type;

    /* number of devices targeted, used in initialization */
    volatile std::atomic<int> ndevices_targeted;

    /* number of devices in the INIT state */
    volatile std::atomic<int> ndevices_inited;

    /* number of devices in the COMMIT state */
    volatile std::atomic<int> ndevices_commited;

    ///////////////////////
    //  DRIVER META DATA //
    ///////////////////////
    const char   *(*f_get_name)(void);          /* name of the driver (human-readable) */
    unsigned int (*f_get_flags)(void);          /* flags: not really used */
    unsigned int (*f_get_ndevices_max)(void);   /* return the number of devices available to the driver */

    ///////////////////////
    //  DRIVER LIFECYCLE //
    ///////////////////////
    int (*f_init)(unsigned int ngpus);
    void (*f_finalize)(void);

    /////////////////////////////////
    //  DRIVER DEVICES MANAGEMENT  //
    /////////////////////////////////

    /* Set the cpuset of the attr for creating the thread that will manage the device dev */
    int (*f_device_set_cpuset)(hwloc_topology_t, cpu_set_t*, int);

    /* create device object and initialize device_driver_id field with argument */
    xkrt_device_t * (*f_device_create)(xkrt_driver_t *, int);
    int (*f_device_destroy)(xkrt_device_t*);

    /* initialize device fields, especially with virtual functions */
    void (*f_device_init)(int device_driver_id);
    int (*f_device_commit)(int device_driver_id);
    void (*f_device_finalize)(xkrt_device_t*);

    /* consider device as the current device */
    int (*f_device_attach)(int device_driver_id);
    int (*f_device_detach)(xkrt_device_t*);

    /* get device infos */
    const char * (*f_device_info)(int device_driver_id);

    ////////////////////////////////
    //  MEMORY MANAGEMENT         //
    ////////////////////////////////
    void * (*f_memory_alloc)(int device_driver_id, const size_t size);
    void (*f_memory_info)(int device_driver_id, size_t * total);
    int (*f_memory_register)(void * ptr, uint64_t size);
    int (*f_memory_unregister)(void * ptr, uint64_t size);

    ////////////////////////////////
    //  DRIVER STREAM MANAGEMENT  //
    ////////////////////////////////

    /* alllocate and initialize a stream */
    xkrt_stream_t * (*f_stream_create)(xkrt_device_t * device, xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity);

    /* deallocate a stream */
    void (*f_stream_delete)(xkrt_stream_t * istream);

		/* get best source for data movement, return global_id */
    xkrt_device_global_id_t (*f_get_source)(xkrt_device_global_id_t dst_global_id, xkrt_device_global_id_bitfield_t valid);

    ///////////////////////
    //  UNUSED YET       //
    ///////////////////////


    # if 0
    /* test completion of asynchronous host_register operation.
       Argument is the handle returned by host_register (if != -1).
       If flag == 0, then the operation is a non blocking test that allows
       to test progression of pinning.
       If flag == 1, then the operation blocks the caller until the request
       has been completed.
       If flag == 2, then the operation blocks the caller until all previous
       requests have been completed.
       Return values are:
       - 0 non error or the test operation complets
       - EINVAL: invalid handle
       - EINPROGESS: request not yet completed
       */
    int  (*f_host_register_testwait)(uint64_t handle, int flag);

    /* Memory unregistration of host memory: asynchronous version */
    uint64_t  (*f_host_unregister)(
        void * ptr, uint64_t sz,
        xkrt_stream_instruction_callback_t callback,
        void * arg0, void * arg1, void * arg2
    );

    /* GPU blas support */
    void* (*f_get_gpublas_handle)(xkrt_device_t*);

    # endif

}               xkrt_driver_t;

void * xkrt_device_thread_main(void * a);

/* one function per task per driver */
static_assert(XKRT_DRIVER_TYPE_MAX <= TASK_FORMAT_FUNC_MAX);

typedef struct  xkrt_drivers_t
{
    /* list of drivers */
    xkrt_driver_t list[XKRT_DRIVER_TYPE_MAX];

    struct {

        /* list of devices */
        xkrt_device_t * list[XKRT_DEVICES_MAX];

        /* number of devices */
        std::atomic<uint8_t> n;

        /* next worker to offload round robin mode */
        std::atomic<uint8_t> round_robin_device_global_id;

    } devices;

}               xkrt_drivers_t;

void
xkrt_drivers_init(
    xkrt_drivers_t * drivers,
    uint8_t ngpus,
    void (*routine)(void * vargs, xkrt_driver_type_t driver_type, uint8_t device_driver_id),
    void * vargs
);
void xkrt_drivers_deinit(xkrt_drivers_t * drivers);

#endif /* __DRIVER_H__ */
