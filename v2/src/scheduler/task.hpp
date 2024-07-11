#ifndef __TASK_HPP__
# define __TASK_HPP__

# include <atomic>
# include <cstdint>
# include <vector>

typedef uint16_t xkblas_task_body_t;

class Task
{
    typedef struct  edge_t
    {
        Task * succ;
    }               edge_t;

    public:
        Task() : readiness(1), edges(4) {}
        ~Task() {}

    private:
        /* readiness counter - the task may be scheduled once it reached 0 */
        std::atomic<int> readiness;

        /* list of out-going edges */
        std::vector<edge_t> edges;

};

#endif /* __TASK_HPP__ */
