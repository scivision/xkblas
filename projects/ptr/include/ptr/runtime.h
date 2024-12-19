/* ************************************************************************** */
/*                                                                            */
/*   runtime.h                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __PTR_CONTEXT_H__
# define __PTR_CONTEXT_H__

# include "conf/conf.h"
# include "device/driver.h"
# include "device/memory-tree.hpp"
# include "stats/stats.h"
# include "sync/spinlock.h"

typedef enum    ptr_runtime_state_t : uint8_t
{
    PTR_CONTEXT_DEINITIALIZED = 0,
    PTR_CONTEXT_INITIALIZED,
}               ptr_runtime_state_t;

typedef struct  ptr_runtime_t
{
    /* context state */
    struct {
        spinlock_t spinlock;
        volatile std::atomic<ptr_runtime_state_t> current;
    } state;

    /* user conf */
    ptr_conf_t conf;

    /* worker thread to copy data asynchronously */
    ThreadWorker * memory_coherent_worker_thread;

    /* driver list */
    ptr_drivers_t drivers;

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
    ptr_stats_t stats;

}               ptr_runtime_t;

// TODO : currently using a global variable to preserve previous 'ptr_init'
// and 'ptr_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'ptr_runtime_t' argument that the user must keep
// track of
extern "C"
ptr_runtime_t * ptr_runtime_get(void);

/* submit a ready task */
void ptr_runtime_submit_task(ptr_runtime_t * context, Task * task);

/* memory async thread management */
void ptr_memory_coherent_async_worker_thread_init(ptr_runtime_t * context);
void ptr_memory_coherent_async_register_format(void);

#endif /* __PTR_CONTEXT_H__ */
