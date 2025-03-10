/* ************************************************************************** */
/*                                                                            */
/*   runtime.h                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/10 17:12:22 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_RUNTIME_H__
# define __XKRT_RUNTIME_H__

# include <xkrt/conf/conf.h>
# include <xkrt/driver/driver.h>
# include <xkrt/thread/thread.h>
# include <xkrt/memory/coherency-controller.hpp>
# include <xkrt/router-affinity.hpp>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/spinlock.h>
# include <xkrt/task/task.hpp>

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
    RouterAffinity router;

    /* get a memory controller for the given ld/type size */
    MemoryCoherencyController * get_or_insert_memory_controller(const size_t ld, const size_t sizeof_type);

    /* hwloc topology, read only, initialized at init */
    hwloc_topology_t topology;

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
    xkrt_area_chunk_t * memory_device_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate the given chunk */
    void memory_device_deallocate(const xkrt_device_global_id_t device_global_id, xkrt_area_chunk_t * chunk);

    /* dealloacte all memory previously allocated on the device */
    void memory_device_deallocate_all(const xkrt_device_global_id_t device_global_id);

    /* allocate memory onto the host using the driver given device */
    void * memory_host_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate memory onto the host using the driver of the given device */
    void memory_host_deallocate(const xkrt_device_global_id_t device_global_id, void * mem, const size_t size);

    /* allocate unified memory using the driver of the given device */
    void * memory_unified_allocate(const xkrt_device_global_id_t device_global_id, const size_t size);

    /* deallocate unified memory using the driver of the given device */
    void memory_unified_deallocate(const xkrt_device_global_id_t device_global_id, void * mem, const size_t size);

    /////////////////////
    // SYNCHRONIZATION //
    /////////////////////

    /* wait until all instructions of the devices completed */
    void wait_device(xkrt_device_global_id_t device_global_id);

    /////////////
    // TASKING //
    /////////////

    /* Enqueue a ready task to the given device */
    void task_submit(const xkrt_device_global_id_t device_global_id, task_t * task);

    /* Commit a task - so it may be schedule from now once its dependences completed */
    void task_commit(task_t * task);

    /* Notify once a detachable task */
    void task_detachable_post(task_t * task);

    /* Complete a task */
    void task_complete(task_t * task);

    /* wait for children tasks of the current task to complete */
    void task_wait(void);

    /* schedule a ready task, and return 1 if one task was found, 0 otherwise */
    int task_schedule(void);

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
    template<bool worksteal = false>
    void team_barrier(xkrt_team_t * team, xkrt_thread_t * thread = NULL);

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

    //////////////////////////////////////
    // linked list for freeing on crash //
    //////////////////////////////////////
    struct xkrt_runtime_t * prev;
    struct xkrt_runtime_t * next;

     # if XKRT_SUPPORT_STATS
     struct {
         struct {
             stats_int_t submitted;
             stats_int_t commited;
             stats_int_t completed;
         } tasks[TASK_FORMAT_MAX];
     } stats;
     # endif /* XKRT_SUPPORT_STATS */

}               xkrt_runtime_t;

/* submit a ready task */
void xkrt_runtime_submit_task(xkrt_runtime_t * runtime, task_t * task);

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
    task_t * task
);

/* enqueue the task in the thread of the team */
void xkrt_team_thread_task_enqueue(xkrt_runtime_t * runtime, xkrt_team_t * team, xkrt_thread_t * thread, task_t * task);

/* submit a task to the given device */
void xkrt_device_task_submit(
    xkrt_runtime_t * runtime,
    xkrt_device_global_id_t device_global_id,
    task_t * task
);

/* report some stats about the runtime */
void xkrt_runtime_stats_report(xkrt_runtime_t * runtime);

#endif /* __XKRT_RUNTIME_H__ */
