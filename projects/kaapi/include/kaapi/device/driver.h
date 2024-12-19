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

# include <kaapi/device/consts.h>
# include <kaapi/device/device.h>
# include <kaapi/device/stream.h>
# include <kaapi/device/task.hpp>
# include <kaapi/device/task-launcher.h>
# include <kaapi/logger/todo.h>
# include <kaapi/sync/mutex.h>

# pragma message(TODO "Organize this file, split independent part in multiple files")

# pragma message(TODO "Replace 'kaapi_driver_t' with a C++ abstract class")
# pragma message(TODO "Add metadata to each interface, for instance, whether its implementation if mandatory or optional")

typedef enum    kaapi_driver_type_t : uint8_t
{
    KAAPI_DRIVER_TYPE_CPU  = 0,
    KAAPI_DRIVER_TYPE_CUDA = 1,
    KAAPI_DRIVER_TYPE_HIP  = 2,
    KAAPI_DRIVER_TYPE_MAX  = 3
}               kaapi_driver_type_t;

typedef struct  kaapi_driver_t
{
    /* type */
    kaapi_driver_type_t type;

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

    /* create device object and initialize device_driver_id field with argument */
    kaapi_device_t * (*f_device_create)(kaapi_driver_t *, int);
    int (*f_device_destroy)(kaapi_device_t*);

    /* initialize device fields, especially with virtual functions */
    void (*f_device_init)(int device_driver_id);
    int (*f_device_commit)(int device_driver_id);
    void (*f_device_finalize)(kaapi_device_t*);

    /* consider device as the current device */
    int (*f_device_attach)(int device_driver_id);
    int (*f_device_detach)(kaapi_device_t*);

    /* get device infos */
    const char * (*f_device_info)(int device_driver_id);

    ////////////////////////////////
    //  MEMORY MANAGEMENT         //
    ////////////////////////////////
    int (*f_memory_register)(void * ptr, uint64_t size);
    int (*f_memory_unregister)(void * ptr, uint64_t size);

    ////////////////////////////////
    //  DRIVER STREAM MANAGEMENT  //
    ////////////////////////////////

    /* alllocate and initialize a stream */
    kaapi_stream_t * (*f_stream_create)(kaapi_stream_type_t type, kaapi_stream_instruction_counter_t capacity);

    /* deallocate a stream */
    void (*f_stream_delete)(kaapi_stream_t * istream);

		/* get best source for data movement, return global_id */
    kaapi_device_global_id_t (*f_get_source)(kaapi_device_global_id_t dst_global_id, kaapi_device_global_id_bitfield_t valid);

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
        kaapi_stream_instruction_callback_t callback,
        void * arg0, void * arg1, void * arg2
    );

    /* GPU blas support */
    void* (*f_get_gpublas_handle)(kaapi_device_t*);

    # endif

}               kaapi_driver_t;

void * kaapi_device_thread_main(void * a);

/* one function per task per driver */
static_assert(KAAPI_DRIVER_TYPE_MAX <= TASK_FORMAT_FUNC_MAX);

typedef struct  kaapi_drivers_t
{
    /* list of drivers */
    kaapi_driver_t list[KAAPI_DRIVER_TYPE_MAX];

    struct {

        /* list of devices */
        kaapi_device_t * list[KAAPI_DEVICES_MAX];

        /* number of devices */
        std::atomic<uint8_t> n;

        /* next worker to offload round robin mode */
        std::atomic<uint8_t> round_robin_device_global_id;

    } devices;

}               kaapi_drivers_t;

typedef struct  kaapi_driver_device_thread_arg_t
{
    kaapi_drivers_t * drivers;
    uint8_t driver_id;
    uint8_t device_driver_id;
}               kaapi_driver_device_thread_arg_t;

void kaapi_drivers_init(kaapi_drivers_t * drivers, uint8_t ngpus);
void kaapi_drivers_deinit(kaapi_drivers_t * drivers);
void kaapi_drivers_enqueue(kaapi_drivers_t * drivers, Task * task);

/* set the initial free block to the ptr device allocator (TODO: maybe
 * change that to some 'add_block' instead to add several blocks) */
void kaapi_device_memory_set_chunk0(kaapi_device_t * device, uintptr_t device_ptr, size_t size
);

/* return the host device */
kaapi_device_t * kaapi_get_device_host(kaapi_drivers_t * drivers);

/* launch a kernel */
int kaapi_task_launch(task_launcher_t * launcher);

/* allocate memory on the passed device */
kaapi_alloc_chunk_t * kaapi_memory_allocate(
    kaapi_driver_t * driver,
    kaapi_device_t * device,
    size_t size
);

/* release the memory chunk */
void kaapi_memory_deallocate(kaapi_device_t * device, kaapi_alloc_chunk_t * chunk);

/* deallocate all memory previously allocated on any devices */
void kaapi_memory_deallocate_all(void);

/* must be call once task accesses were all fetched */
void kaapi_device_task_execute(
    kaapi_device_t * device,
    Task * task
);

kaapi_driver_t * kaapi_driver_get(kaapi_driver_type_t type);
kaapi_device_t * kaapi_device_get(int device_global_id);

#endif /* __DRIVER_H__ */
