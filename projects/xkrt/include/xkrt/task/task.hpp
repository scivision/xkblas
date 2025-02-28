/* ************************************************************************** */
/*                                                                            */
/*   task.hpp                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/28 01:27:54 by Romain PEREIRA            \_)     (_/    */
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

/* wait counter type */
typedef uint8_t task_wc_type_t;

/* task flags */
typedef enum    task_flags_t
{
    TASK_FLAG_IMPLICIT      = (1 << 0), // task is implicit
    TASK_FLAG_UNDEFERED     = (1 << 1), // suspend the current task execution until that task completed
    TASK_FLAG_DETACHABLE    = (1 << 2), // the task completion is associated with the completion of user-defined external events
    TASK_FLAG_MAX           = (1 << 3)
}               task_flags_t;

typedef uint8_t task_flag_bitfield_t;
static_assert(TASK_FLAG_MAX <= 8*sizeof(task_flag_bitfield_t)); // if this fails increase 'task_flag_bitfield_t'

/* task states */
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

        /* parent task */
        KTask * parent;

        /* task format id */
        task_format_id_t fmtid;

        /* task flags */
        task_flag_bitfield_t flags;

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
        # pragma message(TODO "Memory accesses ordering on this atomic")
        std::atomic<uint8_t> wc;

        /* children counter - number of threads with uncompleted tasks scheduled */
        std::atomic<uint32_t> cc;

        /* task state */
        struct {
            spinlock_t      lock;
            task_state_t    value;
        } state;

        # ifndef NDEBUG
        char label[128];
        # endif /* NDEBUG */

    public:

        KTask() :
            KTask(
                TASK_FORMAT_NULL,
                UNSPECIFIED_TASK_ACCESS,
                UNSPECIFIED_DEVICE_GLOBAL_ID,
                0
            )
        {}

        KTask(task_flag_bitfield_t flags_p) :
            KTask(
                TASK_FORMAT_NULL,
                UNSPECIFIED_TASK_ACCESS,
                UNSPECIFIED_DEVICE_GLOBAL_ID,
                flags_p
            )
        {}

        KTask(
            task_format_id_t f,
            uint8_t ocr_access_index_p,
            uint8_t targeted_device_id_p
        ) :
            KTask(f, ocr_access_index_p, targeted_device_id_p, 0)
        {}

        KTask(
            task_format_id_t f,
            uint8_t ocr_access_index_p,
            uint8_t targeted_device_id_p,
            task_flag_bitfield_t flags_p
        ) :
            fmtid(f),
            flags(flags_p),
            edges(),
            accesses(),
            naccesses(0),
            ocr_access_index(ocr_access_index_p),
            targeted_device_id(targeted_device_id_p),
            wc(1),
            cc(0),
            state({.lock=0, .value=TASK_STATE_ALLOCATED})
        {
            edges.reserve(8);

            // NOT SUPPORTED YET
            assert(!(this->flags & TASK_FLAG_UNDEFERED));
            assert(!(this->flags & TASK_FLAG_DETACHABLE));

            # ifndef NDEBUG
            strcpy(this->label, "(unamed task)");
            # endif /* NDEBUG */
        }

        virtual ~KTask()
        {
            this->state.value = TASK_STATE_DEALLOCATED;
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
        template <void (*callback)(void * vargs, KTask * task)>
        inline task_state_t
        commit(void * vargs)
        {
            assert(this->state.value == TASK_STATE_ALLOCATED);
            if (this->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
            {
                LOGGER_DEBUG_TASK("State of task `%s` changed to ready", this->label);
                this->state.value = TASK_STATE_READY;
                callback(vargs, this);
                return TASK_STATE_READY;
            }
            return TASK_STATE_ALLOCATED;
        }

        inline void
        fetching(const task_wc_type_t n = 1)
        {
            if (this->wc.fetch_add(n, std::memory_order_seq_cst) == 0)
            {
                assert(this->state.value == TASK_STATE_READY);
                LOGGER_DEBUG_TASK("State of task `%s` changed to fetching", this->label);
                this->state.value = TASK_STATE_DATA_FETCHING;
            }
        }

        inline task_state_t
        fetched(const task_wc_type_t n = 1)
        {
            assert(this->state.value == TASK_STATE_DATA_FETCHING);
            if (this->wc.fetch_sub(n, std::memory_order_seq_cst) == 1)
            {
                LOGGER_DEBUG_TASK("State of task `%s` changed to fetched", this->label);
                this->state.value = TASK_STATE_DATA_FETCHED;
                return TASK_STATE_DATA_FETCHED;
            }

            return TASK_STATE_DATA_FETCHING;
        }

        template<void (*callback)(void * vargs, KTask * task)>
        inline void
        complete(void * vargs)
        {
            assert(this->wc == 0);
            assert(this->state.value == TASK_STATE_DATA_FETCHED || this->state.value == TASK_STATE_READY);
            SPINLOCK_LOCK(this->state.lock);
            {
                this->state.value = TASK_STATE_COMPLETED;
                LOGGER_DEBUG_TASK("State of task `%s` changed to completed", this->label);
                assert(this->parent);
                this->parent->cc.fetch_sub(1, std::memory_order_relaxed);
            }
            SPINLOCK_UNLOCK(this->state.lock);

            for (Edge & edge : this->edges)
                edge.successor->template commit<callback>(vargs);
        }

};

using Task = KTask<2>;

#endif /* __TASK_HPP__ */
