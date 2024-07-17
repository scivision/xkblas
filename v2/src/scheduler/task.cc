# include "logger/logger.h"
# include "scheduler/task.hpp"

# include <assert.h>

# pragma message(TODO "We have a copy of a 'region' here, change interface to avoid that")
void
Task::precedes(Task * successor, const Region & region)
{
    assert(this->state      >= TASK_STATE_COMMITED);
    assert(successor->state >= TASK_STATE_ALLOCATED);
    assert(!region.is_empty());

    task_edge_t edge = {
        .successor = successor,
        .region = region
    };
    this->edges.push_back(edge);
    std::cout << this << " -> " << successor << " on region " << region << std::endl;
}

task_state_t
Task::commit(void)
{
    assert(this->state == TASK_STATE_ALLOCATED);

    if (this->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
        this->state = TASK_STATE_READY;
    else
        this->state = TASK_STATE_COMMITED;

    return this->state;
}

task_state_t
Task::finalize(const Task * pred, task_edge_t & edge)
{
    assert(pred->state == TASK_STATE_COMPLETED);
    if (edge.successor->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
        edge.successor->state = TASK_STATE_READY;
    return edge.successor->state;
}
