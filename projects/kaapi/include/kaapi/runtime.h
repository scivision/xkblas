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

#ifndef __KAAPI_RUNTIME_H__
# define __KAAPI_RUNTIME_H__

# include <kaapi/logger/todo.h>
# pragma message(TODO "split this file in two with public/private interfaces - moving public interfaces to 'kaapi.h'")

# include <kaapi/conf/conf.h>
# include <kaapi/device/driver.h>
# include <kaapi/device/memory-tree.hpp>
# include <kaapi/stats/stats.h>
# include <kaapi/sync/spinlock.h>

typedef enum    kaapi_runtime_state_t : uint8_t
{
    KAAPI_RUNTIME_DEINITIALIZED = 0,
    KAAPI_RUNTIME_INITIALIZED,
}               kaapi_runtime_state_t;

typedef struct  kaapi_runtime_t
{
    /* runtime state */
    struct {
        spinlock_t spinlock;
        volatile std::atomic<kaapi_runtime_state_t> current;
    } state;

    /* user conf */
    kaapi_conf_t conf;

    /* worker thread to copy data asynchronously */
    ThreadWorker * memory_coherent_worker_thread;

    /* driver list */
    kaapi_drivers_t drivers;

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
    kaapi_stats_t stats;

}               kaapi_runtime_t;

// TODO : currently using a global variable to preserve previous 'kaapi_init'
// and 'kaapi_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'kaapi_runtime_t' argument that the user must keep
// track of
extern "C"
kaapi_runtime_t * kaapi_runtime_get(void);

/* submit a ready task */
void kaapi_runtime_submit_task(kaapi_runtime_t * runtime, Task * task);

/* memory async thread management */
void kaapi_memory_coherent_async_worker_thread_init(kaapi_runtime_t * runtime);
void kaapi_memory_coherent_async_register_format(void);

#endif /* __KAAPI_RUNTIME_H__ */
