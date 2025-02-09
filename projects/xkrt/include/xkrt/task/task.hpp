/* ************************************************************************** */
/*                                                                            */
/*   task.hpp                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:29:00 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __TASK_HPP__
# define __TASK_HPP__

# include <atomic>
# include <cassert>
# include <cstdint>
# include <vector>

# include <xkrt/consts.h>
# include <xkrt/driver/memory-access.hpp>
# include <xkrt/task/task-format.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/sync/spinlock.h>

# define TASK_MAX_ACCESSES          3
# define UNSPECIFIED_TASK_ACCESS    (TASK_MAX_ACCESSES)

// # define LOGGER_DEBUG_TASK(...) LOGGER_DEBUG(__VA_ARGS__)
# define LOGGER_DEBUG_TASK(...)

typedef enum    task_state_t : uint8_t
{
    TASK_STATE_ALLOCATED        = 0,    // Task is allocated
    TASK_STATE_READY            = 1,    // Task data can be fetched
    TASK_STATE_DATA_FETCHING    = 2,    // Task data is being fetched
    TASK_STATE_DATA_FETCHED     = 3,    // Task data is fetched, kernel can be executed
    TASK_STATE_COMPLETED        = 4,    // Task completed, dependences can be resolved (kernel executed)
    TASK_STATE_DEALLOCATED      = 5,    // Task is deallocated (virtual state, never set)
    TASK_STATE_MAX              = 6,
}               task_state_t;

static inline const char *
task_state_to_str(task_state_t state)
{
    switch (state)
    {
        case (TASK_STATE_ALLOCATED):
            return "allocated";
        case (TASK_STATE_READY):
            return "ready";
        case (TASK_STATE_DATA_FETCHING):
            return "fetching";
        case (TASK_STATE_DATA_FETCHED):
            return "fetched";
        case (TASK_STATE_COMPLETED):
            return "completed";
        case (TASK_STATE_DEALLOCATED):
            return "deallocated";
        default:
            return "unk";
    }
}

// TODO
# if USE_STATS
#  define xkrt_stats_task_state_incr(fmtid, state)
# else
#  define xkrt_stats_task_state_incr(...)
# endif /* USE_STATS */

template <int K>
class alignas(CACHE_LINE_SIZE) KTask
{
    /* only supporting dimension K == 2 */
    // static_assert(K == 2);
    using Access = KMemoryAccess<K>;

    public:

        class Edge {

            public:
                KTask * successor;

            public:
                Edge(KTask * s) : successor(s) {}
                virtual ~Edge() {}

        }; /* Edge */

    public:
        ////////////////
        // Attributes //
        ////////////////

        /* task format id */
        task_format_id_t fmtid;

        /* list of out-going edges */
        std::vector<Edge> edges;

        /* task accesses */
        Access accesses[TASK_MAX_ACCESSES];
        uint8_t naccesses;

        /* execute on the device that owns a copy of the access at accesses[ocr_access_index]
         * If 'UNSPECIFIED_TASK_ACCESS', leave the decision to the scheduler */
        uint8_t ocr_access_index;

        /* worker id on where to schedule once ready (or XKRT_DEVICES_MAX if
         * leaving the decision to the scheduler) */
        xkrt_device_global_id_t targeted_device_id;

        /* wait counter - the task may be scheduled once it reached 0 */
        # pragma message(TODO "Memory accesses ordering this atomic")
        alignas(CACHE_LINE_SIZE)
            std::atomic<uint16_t> wc;

        /* task state */
        struct {
            spinlock_t      lock;
            task_state_t    value;
        } state;

        # ifndef NDEBUG
        char label[128];
        # endif /* NDEBUG */

    public:

        KTask() : KTask(TASK_FORMAT_NULL, UNSPECIFIED_TASK_ACCESS, UNSPECIFIED_DEVICE_GLOBAL_ID) {}

        KTask(
            task_format_id_t f,             // task format to use
            uint8_t ocr_access_index_p,     // the ocr access to use
            uint8_t targeted_device_id_p    // targeted device
        ) :
            fmtid(f),
            edges(),
            accesses(),
            naccesses(0),
            ocr_access_index(ocr_access_index_p),
            targeted_device_id(targeted_device_id_p),
            wc(1),
            state({.lock=0, .value=TASK_STATE_ALLOCATED})
        {
            edges.reserve(8);

            # ifndef NDEBUG
            strcpy(this->label, "(unamed task)");
            # endif /* NDEBUG */
            xkrt_stats_task_state_incr(this->fmtid, TASK_STATE_ALLOCATED);
        }

        virtual ~KTask()
        {
            this->state.value = TASK_STATE_DEALLOCATED;
            xkrt_stats_task_state_incr(this->fmtid, TASK_STATE_DEALLOCATED);
        }

    public:
        ////////////////////////////////////
        // Methods to transition the task //
        ////////////////////////////////////

        /* this task precedes the passed task */
        inline void
        precedes(KTask * succ)
        {
            assert(succ);
            assert(this->state.value >= TASK_STATE_ALLOCATED);
            assert(succ->state.value >= TASK_STATE_ALLOCATED);

            if (this->state.value < TASK_STATE_COMPLETED)
            {
                SPINLOCK_LOCK(this->state.lock);
                {
                    if (this->state.value < TASK_STATE_COMPLETED)
                    {
                        succ->wc.fetch_add(1, std::memory_order_seq_cst);
                        this->edges.push_back(Edge(succ));
                    }
                }
                SPINLOCK_UNLOCK(this->state.lock);
            }
        }

        /* Return the 'TASK_STATE_READY' if the task is now ready */
        template <void (*callback)(KTask * task, void * vargs)>
        inline task_state_t
        commit(void * vargs)
        {
            assert(this->state.value == TASK_STATE_ALLOCATED);
            if (this->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
            {
                LOGGER_DEBUG_TASK("State of task `%s` changed to ready", this->label);
                this->state.value = TASK_STATE_READY;
                xkrt_stats_task_state_incr(this->fmtid, TASK_STATE_READY);
                callback(this, vargs);
                return TASK_STATE_READY;
            }
            return TASK_STATE_ALLOCATED;
        }

        inline void
        fetching(const uint16_t n = 1)
        {
            if (this->wc.fetch_add(n, std::memory_order_seq_cst) == 0)
            {
                assert(this->state.value == TASK_STATE_READY);
                LOGGER_DEBUG_TASK("State of task `%s` changed to fetching", this->label);
                this->state.value = TASK_STATE_DATA_FETCHING;
                xkrt_stats_task_state_incr(this->fmtid, TASK_STATE_DATA_FETCHING);
            }
        }

        inline task_state_t
        fetched(const uint16_t n = 1)
        {
            assert(this->state.value == TASK_STATE_DATA_FETCHING);
            if (this->wc.fetch_sub(n, std::memory_order_seq_cst) == 1)
            {
                LOGGER_DEBUG_TASK("State of task `%s` changed to fetched", this->label);
                this->state.value = TASK_STATE_DATA_FETCHED;
                xkrt_stats_task_state_incr(this->fmtid, TASK_STATE_DATA_FETCHED);
                return TASK_STATE_DATA_FETCHED;
            }

            return TASK_STATE_DATA_FETCHING;
        }

        template<void (*callback)(KTask * task, void * vargs)>
        inline void
        complete(void * vargs)
        {
            assert(this->state.value == TASK_STATE_DATA_FETCHED || this->state.value == TASK_STATE_READY);
            SPINLOCK_LOCK(this->state.lock);
            {
                this->state.value = TASK_STATE_COMPLETED;
                LOGGER_DEBUG_TASK("State of task `%s` changed to completed", this->label);
                xkrt_stats_task_state_incr(this->fmtid, TASK_STATE_COMPLETED);
            }
            SPINLOCK_UNLOCK(this->state.lock);

            for (Edge & edge : this->edges)
                edge.successor->template commit<callback>(vargs);
        }
};

using Task = KTask<2>;

#endif /* __TASK_HPP__ */
