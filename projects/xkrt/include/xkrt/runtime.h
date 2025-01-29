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
# include <xkrt/device/driver.h>
# include <xkrt/device/memory-tree.hpp>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/spinlock.h>

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

    /* memory state on each device */
    std::vector<MemoryTree *> memtrees;
    MemoryTree *
    get_memory_tree(const size_t ld, const size_t sizeof_type)
    {
        /* find previous memtree for that ld */
        for (MemoryTree * memtree : this->memtrees)
            if (memtree->ld == ld && memtree->sizeof_type == sizeof_type)
                return memtree;

        LOGGER_DEBUG("Created a new memory tree with (ld, sizeof(type), merge) = (%lu, %lu, %s)",
                ld, sizeof_type, this->conf.merge_transfers ? "true" : "false");

        /* if not found, create a new memtree */
        MemoryTree * memtree = new MemoryTree(ld, sizeof_type, this->conf.merge_transfers);
        assert(memtree);
        assert(memtree->ld == ld);
        assert(memtree->sizeof_type == sizeof_type);
        this->memtrees.push_back(memtree);
        return memtree;
    }

    /* stats for debugging */
    xkrt_stats_t stats;

}               xkrt_runtime_t;

// TODO : currently using a global variable to preserve previous 'xkrt_init'
// and 'xkrt_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'xkrt_runtime_t' argument that the user must keep
// track of
extern "C"
xkrt_runtime_t * xkrt_runtime_get(void);

/* submit a ready task */
void xkrt_runtime_submit_task(xkrt_runtime_t * runtime, Task * task);

/* memory async thread management */
void xkrt_memory_coherent_async_worker_thread_init(xkrt_runtime_t * runtime);
void xkrt_memory_coherent_async_register_format(void);

#endif /* __XKRT_RUNTIME_H__ */
