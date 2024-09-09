#ifndef __DRIVER_H__
# define __DRIVER_H__

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>     /* cpu_set_t */
# include <stdint.h>    /* uint64_t */

# include "device/device.h"
# include "device/stream.h"
# include "device/consts.h"
# include "device/task.hpp"
# include "logger/todo.h"
# include "kernels/kernel-param.h"
# include "sync/mutex.h"

# pragma message(TODO "Organize this file, split independent part in multiple files")

# pragma message(TODO "Replace 'xkblas_driver_t' with a C++ abstract class")
# pragma message(TODO "Add metadata to each interface, for instance, whether its implementation if mandatory or optional")

typedef enum    xkblas_driver_type_t : uint8_t
{
    XKBLAS_DRIVER_TYPE_CPU = 0,
    XKBLAS_DRIVER_TYPE_CUDA = 1,
    XKBLAS_DRIVER_TYPE_MAX  = 2
}               xkblas_driver_type_t;

typedef struct  xkblas_driver_t
{
    /* type */
    xkblas_driver_type_t type;

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
    int (*f_init)(void);
    void (*f_finalize)(void);

    /////////////////////////////////
    //  DRIVER DEVICES MANAGEMENT  //
    /////////////////////////////////

    /* Set the cpuset of the attr for creating the thread that will manage the device dev */
    int (*f_device_set_cpuset)(cpu_set_t*, int);

    /* create device object and initialize device_id field with argument */
    xkblas_device_t * (*f_device_create)(xkblas_driver_t *, int);
    int (*f_device_destroy)(xkblas_device_t*);

    /* initialize device fields, especially with virtual functions */
    const char* (*f_device_info)(xkblas_device_t*);
    void (*f_device_init)(int device_id);
    int (*f_device_commit)(int device_id);
    void (*f_device_finalize)(xkblas_device_t*);

    /* consider device as the current device */
    int (*f_device_attach)(int device_id);
    int (*f_device_detach)(xkblas_device_t*);

    ////////////////////////////////
    //  DRIVER STREAM MANAGEMENT  //
    ////////////////////////////////

    /* alllocate and initialize a stream */
    xkblas_stream_t * (*f_stream_create)(xkblas_stream_type_t type, unsigned int capacity);

    /* deallocate a stream */
    void (*f_stream_delete)(xkblas_stream_t * istream);

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
        xkblas_stream_instruction_callback_t callback,
        void * arg0, void * arg1, void * arg2
    );

    /* GPU blas support */
    void* (*f_get_gpublas_handle)(xkblas_device_t*);

    # endif

}               xkblas_driver_t;

void * xkblas_device_thread_main(void * a);

/* one function per task per driver */
static_assert(XKBLAS_DRIVER_TYPE_MAX <= TASK_FORMAT_FUNC_MAX);

typedef struct  xkblas_drivers_t
{
    /* list of drivers */
    xkblas_driver_t list[XKBLAS_DRIVER_TYPE_MAX];

    struct {

        /* list of devices */
        xkblas_device_t * list[XKBLAS_DEVICES_MAX];

        /* number of devices */
        std::atomic<uint8_t> n;

        /* next worker to offload round robin mode */
        std::atomic<uint8_t> round_robin_device_id;

        /* connectivity performances to find which link to use when moving data between devices */
        int connectivity[XKBLAS_DEVICES_MAX+1][XKBLAS_DEVICES_MAX+1];

    } devices;

    /* number of uncompleted tasks (used for xkblas_sync) */
    std::atomic<uint32_t> uncompleted_tasks;

}               xkblas_drivers_t;

typedef struct  xkblas_driver_device_thread_arg_t
{
    xkblas_drivers_t * drivers;
    uint8_t driver_id;
    uint8_t driver_device_id;
}               xkblas_driver_device_thread_arg_t;

void xkblas_drivers_init(xkblas_drivers_t * drivers, uint8_t ngpus);
void xkblas_drivers_deinit(xkblas_drivers_t * drivers);
void xkblas_drivers_enqueue(xkblas_drivers_t * drivers, Task * task);

/* return the host device */
xkblas_device_t * xkblas_get_device_host(xkblas_drivers_t * drivers);

/* call the launch task kernel once accesses are all ready */
void xkblas_device_task_fetched(xkblas_driver_t * driver, xkblas_device_t * device, Task * task);

/* launch a kernel */
int xkblas_kernel_launch(xkblas_driver_type_t type, task_kernel_param_t * param);

/* allocate memory on the passed device */
void * xkblas_memory_allocate(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    size_t size
);

/* deallocate all memory allocated previously */
void xkblas_driver_invalidate_caches(
    xkblas_driver_t * driver,
    xkblas_device_t * device
);

/* must be call once task accesses were all fetched */
void xkblas_device_task_access_fetched(
    xkblas_driver_t * driver,
    xkblas_device_t * device,
    Task * task
);

xkblas_driver_t * xkblas_driver_get(xkblas_driver_type_t type);
xkblas_device_t * xkblas_device_get(int device_global_id);

#endif /* __DRIVER_H__ */
