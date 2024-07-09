#ifndef __SCHEDULER_HPP__
# define __SCHEDULER_HPP__

# include "stream.hpp"
# include "thread.hpp"

class Scheduler
{
    public:
        Scheduler() {}
        ~Scheduler() {}

    private:

        /* list of threads */
        std::vector<Thread *> threads;

        /* list of GPU streams */
        std::vector<Stream_t *> streams;
};

#endif /* __SCHEDULER_HPP__ */
