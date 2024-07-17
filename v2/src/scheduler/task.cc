# include "task.hpp"

Task::Task(task_body_t body) :
    body(body),
    edges(),
    accesses(),
    wc(1)
{}

Task::~Task()
{}

template<int N>
void
commit(Thread * thread)
{
    // the task cannot be scheduled
    this->wc.fetch_add(1, std::memory_order_seq_cst);

    // set edges with previously inserted tasks
    for (int i = 0 ; i < N ; ++i)
        thread->memtree.intersect(this->accesses[i].mode, this->accesses[i].region, task);

    // register accesses for linking with future tasks
    for (int i = 0 ; i < N ; ++i)
        thread->memtree.insert(this->accesses[i].mode, this->accesses[i].region, task);

    // the task may not be scheduled
    if (this->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
        thread->queue.push(task);
}
