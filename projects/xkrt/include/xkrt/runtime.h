/* ************************************************************************** */
/*                                                                            */
/*   runtime.h                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/28 00:53:22 by Romain PEREIRA            \_)     (_/    */
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
# include <xkrt/thread/thread.h>
# include <xkrt/memory/coherency-controller.hpp>
# include <xkrt/router-random.hpp>
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
    std::atomic<xkrt_runtime_state_t> state;

    /* driver list */
    xkrt_drivers_t drivers;

    /* task formats */
    struct {
        task_formats_t list;
        task_format_id_t coherent_async;
        task_format_id_t coherent_async_fetch;
        task_format_id_t copy_async;
    } formats;

    /* memory controller for coherency */
    std::vector<MemoryCoherencyController *> memcontrollers;

    /* worker thread to copy data asynchronously */
    Thread * memory_coherent_worker_thread;

    /* user conf */
    xkrt_conf_t conf;

    /* memory router */
    RouterRandom router;

    /* get a memory controller for the given ld/type size */
    MemoryCoherencyController * get_or_insert_memory_controller(const size_t ld, const size_t sizeof_type);

    //////////////////////////////////////////////////////////////////////////////////////////////
    // UTILITIES FOR THE MEMORY COHERENCY TREE - THIS SHOULD GTFO AND BE ABSTRACTED IN THE TREE //
    //////////////////////////////////////////////////////////////////////////////////////////////
    /// It currently only serves as a glue between the tree 'xkrt_device_global_id_t' representation
    /// and the runtime 'xkrt_device_t' structures
    //////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////
    // DATA MOVEMENTS //
    ////////////////////

    /* Submit a copy instruction to a stream of the device */
    void copy(
        const xkrt_device_global_id_t   device_global_id,
        const size_t                    size,
        const xkrt_device_global_id_t   dst_device_global_id,
        const uintptr_t                 dst_device_addr,
        const xkrt_device_global_id_t   src_device_global_id,
        const uintptr_t                 src_device_addr,
        const xkrt_callback_t         & callback
    );

    /* Submit a copy instruction to a stream of the device */
    void copy(
        const xkrt_device_global_id_t   device_global_id,
        const memory_view_t           & host_view,
        const xkrt_device_global_id_t   dst_device_global_id,
        const memory_replicate_view_t & dst_device_view,
        const xkrt_device_global_id_t   src_device_global_id,
        const memory_replicate_view_t & src_device_view,
        const xkrt_callback_t         & callback
    );

    ////////////
    // MEMORY //
    ////////////

    /* allocate memory onto the given device */
    xkrt_area_chunk_t * memory_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate the given chunk */
    void memory_deallocate(const xkrt_device_global_id_t device_global_id, xkrt_area_chunk_t * chunk);

    /* dealloacte all memory previously allocated on the device */
    void memory_deallocate_all(const xkrt_device_global_id_t device_global_id);

    /* allocate memory onto the host using the given driver */
    void * memory_host_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate memory onto the host using the given driver */
    void memory_host_deallocate(const xkrt_device_global_id_t device_global_id, void * mem, const size_t size);

    /////////////////////
    // SYNCHRONIZATION //
    /////////////////////

    /* wait until all instructions of the devices completed */
    void wait_device(xkrt_device_global_id_t device_global_id);

    /////////////
    // TASKING //
    /////////////

    /* Enqueue a ready task to the given device */
    void task_submit(Task * task, const xkrt_device_global_id_t device_global_id);

    /* Commit a task - so it may be schedule from now once its dependences completed */
    void task_commit(Task * task);

    /* Complete a task */
    void task_complete(Task * task);

    ///////////////
    // THREADING //
    ///////////////

    /* Retrieve the cpuset of the calling thread */
    static void thread_getaffinity(cpu_set_t & cpuset);

    /* Bind the calling thread to the given cpu set */
    static void thread_setaffinity(cpu_set_t & cpuset);

    /* create a new thread team */
    void team_create(xkrt_team_t * team);

    /* wait until all threads called the barrier */
    void team_barrier(xkrt_team_t * team);

    /* wait until all threads finished and destroy the team */
    void team_join(xkrt_team_t * team);

    /* start a critical section */
    void team_critical_begin(xkrt_team_t * team);

    /* end a critical section */
    void team_critical_end(xkrt_team_t * team);

    ///////////////
    // UTILITIES //
    ///////////////

    /* get driver */
    xkrt_driver_t * driver_get(const xkrt_driver_type_t type);

    /* get device */
    xkrt_device_t * device_get(const xkrt_device_global_id_t device_global_id);

}               xkrt_runtime_t;

/* submit a ready task */
void xkrt_runtime_submit_task(xkrt_runtime_t * runtime, Task * task);

/* memory async thread management */
void xkrt_memory_coherent_async_worker_thread_init(xkrt_runtime_t * runtime);
void xkrt_memory_coherent_async_register_format(xkrt_runtime_t * runtime);
void xkrt_memory_copy_async_register_format(xkrt_runtime_t * runtime);

/* Main entry thread created per device */
void xkrt_device_thread_main(void * vruntime, xkrt_driver_type_t driver_type, uint8_t device_driver_id);

/* must be call once task accesses were all fetched */
void xkrt_device_task_execute(
    xkrt_runtime_t * runtime,
    xkrt_device_t * device,
    Task * task
);

/* report some stats about the runtime */
void xkrt_runtime_stats_report(xkrt_runtime_t * runtime);

#endif /* __XKRT_RUNTIME_H__ */
