# include "logger/logger.h"
# include "device/task.hpp"
# include "sync/spinlock.h"

# include <assert.h>

# pragma message(TODO "We have a copy of a 'region' here, do we really need that copy ?")
void
Task::precedes(Task * succ, const Region & region)
{
    assert(succ);
    assert(this->state.value >= TASK_STATE_ALLOCATED);
    assert(succ->state.value >= TASK_STATE_ALLOCATED);
    assert(!region.is_empty());

    if (this->state.value < TASK_STATE_COMPLETED)
    {
        task_edge_t edge = {
            .successor = succ,
            .region = region
        };

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

bool
Task::commit(void)
{
    assert(this->state.value == TASK_STATE_ALLOCATED);
    if (this->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
    {
        this->state.value = TASK_STATE_READY;
        return true;
    }
    return false;
}

void
Task::execute(void)
{
    // TODO

    SPINLOCK_LOCK(this->state.lock);
    {
        this->state.value = TASK_STATE_EXECUTED;
    }
    SPINLOCK_UNLOCK(this->state.lock);
}

# pragma message(TODO "This routine is unused currently")
void
Task::complete(void)
{
    assert(this->state.value == TASK_STATE_EXECUTED);
    this->state.value = TASK_STATE_COMPLETED;

    for (task_edge_t & edge : this->edges)
    {
        if (edge.successor->wc.fetch_sub(1, std::memory_order_seq_cst) - 1 == 0)
        {
            edge.successor->state.value = TASK_STATE_READY;
            // TODO : queue 'succ'
        }
    }
}
