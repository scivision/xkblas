#ifndef __TASK_HPP__
# define __TASK_HPP__

# include <atomic>
# include <cstdint>
# include <vector>

# include "device/consts.h"
# include "logger/todo.h"
# include "sync/access.hpp"
# include "sync/cache-line-size.hpp"
# include "sync/spinlock.h"

# define TASK_MAX_ACCESSES 3

typedef enum    task_state_t : uint8_t
{
    TASK_STATE_ALLOCATED        = 0,    // Task is allocated
    TASK_STATE_READY            = 1,    // Task body can be executed
    TASK_STATE_RUNNING          = 2,    // Task body is executing
    TASK_STATE_EXECUTED         = 3,    // Task body executed
    TASK_STATE_COMPLETED        = 4,    // Task completed, dependences can be resolved
    TASK_STATE_DEALLOCATED      = 5,    // Task is deallocated (virtual state, never set)
}               task_state_t;

enum task_body_t : uint8_t
{
    TASK_BODY_NOOP      = 0,

    TASK_BODY_GEMM      = 1,
    TASK_BODY_TRSM      = 2,
    TASK_BODY_COPYSCALE = 3,

    TASK_BODY_MAX       = 4,
};

template<int K>
class task_access_t : public access_t<K>
{
    public:

        /* matrix host address (passed to the BLAS kernel) */
        uintptr_t host_addr;

        /* matrix LD */
        int LD;

        /* tile accessed (in [0..ntiles[) */
        int tm;
        int tn;

        /* tile size */
        int bs_m;
        int bs_n;

    public:

        task_access_t() {}

        task_access_t(
            const access_mode_t & m,
            const uintptr_t & host_addr,
            const int & LD,
            const int & tm,   const int & tn,
            const int & bs_m, const int & bs_n
        ) :
            access_t<K>(m, host_addr, LD, tm, tn, bs_m, bs_n),
            host_addr(host_addr),
            LD(LD),
            tm(tm),     tn(tn),
            bs_m(bs_m), bs_n(bs_n)
        {}

        virtual ~task_access_t() {}
};

class Task;
using Region = Intervals<2>;

/**
 *  An edge between two tasks.
 *      successor - the successor task
 *      region - accessed by both tasks, with at least one writing
 */
typedef struct  task_edge_t
{
    Task * successor;
    const Region region;
}               task_edge_t;

class alignas(CACHE_LINE_SIZE) Task
{
    public:
        ////////////////
        // Attributes //
        ////////////////

        /* task body */
        task_body_t body;

        /* list of out-going edges */
        std::vector<task_edge_t> edges;

        /* task accesses */
        task_access_t<2> accesses[TASK_MAX_ACCESSES];
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

        Task() : Task(TASK_BODY_NOOP) {}

        Task(task_body_t body) :
            body(body),
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

        virtual ~Task()
        {
            this->state.value = TASK_STATE_DEALLOCATED;
        }

        ////////////////////////////////////
        // Methods to transition the task //
        ////////////////////////////////////

        /* this task precedes the passed task */
        void precedes(Task * successor, const Region & region);

        /* Return 'true' if the task is ready to be queued, 'false' otherwise */
        bool commit(void);

        // TODO
        void execute(void);
        void complete(void);
};

#endif /* __TASK_HPP__ */
