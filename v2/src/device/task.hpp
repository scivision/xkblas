#ifndef __TASK_HPP__
# define __TASK_HPP__

# include <atomic>
# include <cassert>
# include <cstdint>
# include <vector>

# include "device/consts.h"
# include "device/task-format.h"
# include "logger/logger.h"
# include "logger/todo.h"
# include "device/memory-access.hpp"
# include "sync/access.hpp"
# include "sync/cache-line-size.hpp"
# include "sync/spinlock.h"

# define TASK_MAX_ACCESSES 3

typedef enum    task_state_t : uint8_t
{
    TASK_STATE_ALLOCATED        = 0,    // Task is allocated
    TASK_STATE_READY            = 1,    // Task data can be fetched
    TASK_STATE_DATA_FETCHING    = 2,    // Task data is being fetched
    TASK_STATE_DATA_FETCHED     = 3,    // Task data is fetched, kernel can be executed
    TASK_STATE_EXECUTED         = 4,    // Task kernel executed
    TASK_STATE_COMPLETED        = 5,    // Task completed, dependences can be resolved
    TASK_STATE_DEALLOCATED      = 6,    // Task is deallocated (virtual state, never set)
}               task_state_t;

template <int K>
class alignas(CACHE_LINE_SIZE) KTask
{
    /* only supporting dimension K == 2 */
    static_assert(K == 2);
    using Access = KMemoryAccess<K>;
    using Region = Intervals<K>;

    public:

        /**
         *  An edge between two tasks.
         *      successor - the successor task
         *      region - accessed by both tasks, with at least one writing
         */
        class Edge {

            public:
                KTask * successor;
                const Region region;

            public:
                Edge(KTask * s, const Region & r) : successor(s), region(r) {}
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

        /* OCR parameter index, or -1 if none */
        uint8_t ocr_access_index;

        /* worker id on where to schedule once ready */
        uint8_t targetted_device_id;

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

        KTask() : KTask(TASK_FORMAT_NULL) {}

        KTask(task_format_id_t f) :
            fmtid(f),
            edges(),
            accesses(),
            naccesses(0),
            ocr_access_index(XKBLAS_DEVICES_MAX),
            targetted_device_id(XKBLAS_DEVICES_MAX),
            wc(1),
            state({.lock=0, .value=TASK_STATE_ALLOCATED})
        {
            edges.reserve(8);

            # ifndef NDEBUG
            strcpy(this->label, "(unamed task)");
            # endif /* NDEBUG */
        }

        virtual ~KTask()
        {
            this->state.value = TASK_STATE_DEALLOCATED;
        }

        ////////////////////////////////////
        // Methods to transition the task //
        ////////////////////////////////////

        /* this task precedes the passed task */
        inline void
        precedes(KTask * succ, const Region & region)
        {
            assert(succ);
            assert(this->state.value >= TASK_STATE_ALLOCATED);
            assert(succ->state.value >= TASK_STATE_ALLOCATED);
            assert(!region.is_empty());

            if (this->state.value < TASK_STATE_COMPLETED)
            {
                Edge edge(succ, region);

                SPINLOCK_LOCK(this->state.lock);
                {
                    if (this->state.value < TASK_STATE_EXECUTED)
                    {
                        succ->wc.fetch_add(1, std::memory_order_seq_cst);
                        this->edges.push_back(edge);
                    }
                }
                SPINLOCK_UNLOCK(this->state.lock);
            }
        }

        /* Return 'true' if the task is ready to be queued, 'false' otherwise */
        inline bool
        commit(void)
        {
            assert(this->state.value == TASK_STATE_ALLOCATED);
            if (this->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
            {
                this->state.value = TASK_STATE_READY;
                return true;
            }
            return false;
        }

        inline void
        fetching(void)
        {
            if (this->wc.fetch_add(1, std::memory_order_seq_cst) == 0)
            {
                assert(this->state.value == TASK_STATE_READY);
                this->state.value = TASK_STATE_DATA_FETCHING;
            }
        }

        inline task_state_t
        fetched(void)
        {
            assert(this->state.value == TASK_STATE_DATA_FETCHING);
            if (this->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
            {
                this->state.value = TASK_STATE_DATA_FETCHED;
                return TASK_STATE_DATA_FETCHED;
            }
            return TASK_STATE_DATA_FETCHING;
        }

        inline void
        executed(void)
        {
            assert(this->state.value == TASK_STATE_DATA_FETCHED);
            SPINLOCK_LOCK(this->state.lock);
            {
                this->state.value = TASK_STATE_EXECUTED;
            }
            SPINLOCK_UNLOCK(this->state.lock);
        }

        inline void
        complete(void)
        {
            assert(this->state.value == TASK_STATE_EXECUTED);
            this->state.value = TASK_STATE_COMPLETED;

            for (Edge & edge : this->edges)
            {
                if (edge.successor->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
                {
                    edge.successor->state.value = TASK_STATE_READY;
                    xkblas_task_ready(edge.successor);
                }
            }
        }
};

using Task = KTask<2>;

void xkblas_task_ready(Task * task);

#endif /* __TASK_HPP__ */
