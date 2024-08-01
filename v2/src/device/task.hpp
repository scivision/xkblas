#ifndef __TASK_HPP__
# define __TASK_HPP__

# include <atomic>
# include <cstdint>
# include <new>
# include <vector>

# include "device/consts.h"
# include "sync/access.hpp"

# define TASK_MAX_ACCESSES  4

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

using Region = Intervals<2>;

struct task_access_t : access_t<2>
{
    task_access_t() : access_t<2>() {}
    ~task_access_t() {}
};

class Task;

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

class alignas(std::hardware_constructive_interference_size) Task
{
    public:

        Task(task_body_t body) :
            body(body),
            edges(8),
            accesses(),
            ocr_access_index(XKBLAS_DEVICES_MAX),
            targetted_device_id(XKBLAS_DEVICES_MAX),
            wc(1),
            state(TASK_STATE_ALLOCATED)
        {}

        ~Task()
        {
            this->state = TASK_STATE_DEALLOCATED;
        }

        ////////////////
        // Attributes //
        ////////////////

        /* task body */
        task_body_t body;

        /* list of out-going edges */
        std::vector<task_edge_t> edges;

        /* task accesses */
        task_access_t accesses[TASK_MAX_ACCESSES];

        /* OCR parameter index, or -1 if none */
        uint8_t ocr_access_index;

        /* worker id on where to schedule once ready */
        uint8_t targetted_device_id;

        /* wait counter - the task may be scheduled once it reached 0 */
        # pragma message(TODO "Memory accesses ordering this atomic")
        alignas(std::hardware_constructive_interference_size) std::atomic<uint16_t> wc;

        /* task state */
        task_state_t state;

        ////////////////////////////////////
        // Methods to transition the task //
        ////////////////////////////////////

        /* this task precedes the passed task */
        void precedes(Task * successor, const Region & region);

        /**
         * Return 'true' if the task is ready to be queued, 'false' otherwise
         */
        bool commit(void);

        /* resolve the passed edge, and return the successor state */
        static task_state_t finalize(const Task * pred, task_edge_t & edge);
};

#endif /* __TASK_HPP__ */
