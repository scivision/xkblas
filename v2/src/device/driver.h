#ifndef __DRIVER_H__
# define __DRIVER_H__

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>     /* cpu_set_t */
# include <stdint.h>    /* uint64_t */

# include "device/address-space.h"
# include "device/device.h"
# include "device/io.h"
# include "device/memory-tree.hpp"
# include "logger/todo.h"
# include "device/consts.h"
# include "device/task.hpp"
# include "sync/mutex.h"

# pragma message(TODO "Organize this file, split independent part in multiple files")

# define XKBLAS_STREAM_CAPACITY 512

# define XKBLAS_DRIVER_PREFIX_NAME "XKBLAS_DRIVER_"
# define XKBLAS_DRIVER_ENTRYPOINT_NAME( func_name ) XKBLAS_DRIVER_PREFIX_NAME #func_name
# define XKBLAS_DRIVER_ENTRYPOINT( func_name ) XKBLAS_DRIVER_ ## func_name

# pragma message(TODO "Replace 'xkblas_driver_t' with a C++ abstract class")

typedef struct  xkblas_driver_t
{
    /* number of devices targeted, used in initialization */
    volatile std::atomic<int> ndevices_targeted;

    /* number of devices in the INIT state */
    volatile std::atomic<int> ndevices_inited;

    /* number of devices in the COMMIT state */
    volatile std::atomic<int> ndevices_commited;

    /* Function handlers: accessor to meta data */
    const char   *(*f_get_name)(void);          /* name of the driver (human-readable) */
    unsigned int (*f_get_flags)(void);          /* flags: not really used */
    unsigned int (*f_get_ndevices_max)(void);   /* return the number of devices available to the driver */

    /* life cycle functions for the driver of devices (1 device == 1 ressource) */
    int (*f_init)(void);
    void (*f_finalize)(void);

    /* driver specific functions for all devices managed by the driver */
    /* Memory registration of host memory */
    uint64_t (*f_host_register)(
            void * ptr, uint64_t sz,
            xkblas_io_callback_func_t callback,
            void * arg0, void * arg1, void * arg2
    );

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
        xkblas_io_callback_func_t callback,
        void * arg0, void * arg1, void * arg2
    );

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
    /* consider device as the current device */
    int (*f_device_detach)(xkblas_device_t*);

    /* GPU blas support */
    void* (*f_get_gpublas_handle)(xkblas_device_t*);

}               xkblas_driver_t;

void * xkblas_device_thread_main(void * a);

typedef enum    xkblas_driver_type_t
{
    XKBLAS_DRIVER_HOST = 0,
    XKBLAS_DRIVER_CUDA = 1,
    XKBLAS_DRIVER_MAX  = 2,
}               xkblas_driver_type_t;

typedef struct  xkblas_drivers_t
{
    /* list of drivers */
    xkblas_driver_t list[XKBLAS_DRIVER_MAX];

    struct {
        /* list of devices */
        xkblas_device_t * list[XKBLAS_DEVICES_MAX];

        /* number of devices */
        std::atomic<uint8_t> n;

        /* next worker to offload round robin mode */
        std::atomic<uint8_t> round_robin_device_id;
    } devices;

    /* memory mapping */
    MemoryTree memtree;

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

#endif /* __DRIVER_H__ */
