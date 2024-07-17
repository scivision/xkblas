#ifndef __TASK_HPP__
# define __TASK_HPP__

# include <atomic>
# include <cstdint>
# include <new>
# include <vector>

# include "sync/access.hpp"

# define TASK_MAX_ACCESSES  4

# if 0
typedef enum    task_state_t : uint8_t
{
    TASK_STATE_INITIALIZED      = 0,
    TASK_STATE_READY            = 1,
    TASK_STATE_RUNNING          = 2,
    TASK_STATE_EXECUTED         = 3,
    TASK_STATE_COMPLETED        = 4,
}               task_state_t;

/* task state */
alignas(std::hardware_constructive_interference_size) task_state_t state;
# endif

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
    const Task * successor;
    const Region region;
}               task_edge_t;

class alignas(std::hardware_constructive_interference_size) Task
{
    public:
        Task(task_body_t body);
        virtual ~Task();

        // TODO : make attributes private and add inline member methods ?

        /* task body */
        task_body_t body;

        /* list of out-going edges */
        std::vector<task_edge_t> edges;

        /* task accesses */
        task_access_t accesses[TASK_MAX_ACCESSES];

        /* wait counter - the task may be scheduled once it reached 0 */
        # pragma message(TODO "Memory accesses ordering this atomic")
        alignas(std::hardware_constructive_interference_size) std::atomic<uint16_t> wc;

        /* this task precedes the passed task */
        void precedes(const Task * successor, const Region & region);
};

#endif /* __TASK_HPP__ */
