/* ************************************************************************** */
/*                                                                            */
/*   runtime.h                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:11:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_RUNTIME_H__
# define __XKRT_RUNTIME_H__

# include <xkrt/logger/todo.h>
# pragma message(TODO "split this file in two with public/private interfaces - moving public interfaces to 'xkrt.h'")

# include <xkrt/conf/conf.h>
# include <xkrt/driver/driver.h>
# include <xkrt/memory/coherency-controller.hpp>
# include <xkrt/sync/spinlock.h>

# include <hwloc.h>

typedef enum    xkrt_runtime_state_t : uint8_t
{
    XKRT_RUNTIME_DEINITIALIZED = 0,
    XKRT_RUNTIME_INITIALIZED,
}               xkrt_runtime_state_t;

typedef struct  xkrt_runtime_t
{
    /* runtime state */
    struct {
        spinlock_t spinlock;
        volatile std::atomic<xkrt_runtime_state_t> current;
    } state;

    /* user conf */
    xkrt_conf_t conf;

    /* worker thread to copy data asynchronously */
    ThreadWorker * memory_coherent_worker_thread;

    /* driver list */
    xkrt_drivers_t drivers;

    /* task formats */
    task_formats_t task_formats;

    /* memory controller for coherency */
    std::vector<MemoryCoherencyController *> memcontrollers;

    /* get a memory controller for the given ld/type size */
    MemoryCoherencyController * get_or_insert_memory_controller(const size_t ld, const size_t sizeof_type);

    //////////////////////////////////////////////////////////////////////////////////////////////
    // UTILITIES FOR THE MEMORY COHERENCY TREE - THIS SHOULD GTFO AND BE ABSTRACTED IN THE TREE //
    //////////////////////////////////////////////////////////////////////////////////////////////
    /// It currently only serves as a glue between the tree 'xkrt_device_global_id_t' representation
    /// and the runtime 'xkrt_device_t' structures
    //////////////////////////////////////////////////////////////////////////////////////////////

    /* allocate memory onto the given device */
    xkrt_area_chunk_t * memory_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate the given chunk */
    void memory_deallocate(const xkrt_device_global_id_t device_global_id, xkrt_area_chunk_t * chunk);

    /* get device/driver */
    xkrt_driver_t * driver_get(const xkrt_driver_type_t type);
    xkrt_device_t * device_get(const xkrt_device_global_id_t device_global_id);

    /* submissions */
    void submit_copy(
        const xkrt_device_global_id_t   device_global_id,
        const memory_view_t           & host_view,
        const xkrt_device_global_id_t   dst_device_global_id,
        const memory_replicate_view_t & dst_device_view,
        const xkrt_device_global_id_t   src_device_global_id,
        const memory_replicate_view_t & src_device_view,
        const xkrt_callback_t         & callback
    );

    void task_execute(
        Task * task,
        const xkrt_device_global_id_t device_global_id
    );

    //////////////////////////////////////////////////////////////////////////////////////////////
    // UTILITIES FOR TASK SCHEDULING
    //////////////////////////////////////////////////////////////////////////////////////////////
    void commit(Task * task);
    void complete(Task * task);

}               xkrt_runtime_t;

/* submit a ready task */
void xkrt_runtime_submit_task(xkrt_runtime_t * runtime, Task * task);

/* memory async thread management */
void xkrt_memory_coherent_async_worker_thread_init(xkrt_runtime_t * runtime);
void xkrt_memory_coherent_async_register_format(xkrt_runtime_t * runtime);

/* Main entry thread created per device */
void xkrt_device_thread_main(void * vruntime, xkrt_driver_type_t driver_type, uint8_t device_driver_id);

/* must be call once task accesses were all fetched */
void xkrt_device_task_execute(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    Task * task
);

# if USE_STATS
/* report some stats about the runtime */
void xkrt_runtime_stats_report(xkrt_runtime_t * runtime);
# endif /* USE_STATS */

#endif /* __XKRT_RUNTIME_H__ */
