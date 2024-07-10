#ifndef __TASK_HPP__
# define __TASK_HPP__

# include <atomic>
# include <cstdint>
# include <vector>


typedef uint16_t xkblas_task_body_t;

class Task
{
    public:
        Task() : readiness(1), successors(4) {}
        ~Task() {}

    private:

        /* readiness counter - the task may be scheduled once it reached 0 */
        std::atomic<int> readiness;

        /* list of successors */
        std::vector<Task *> successors;
};

#endif /* __TASK_HPP__ */
