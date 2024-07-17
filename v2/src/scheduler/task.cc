# include "task.hpp"

Task::Task(task_body_t body) :
    body(body),
    edges(8),
    accesses(),
    wc(1)
{}

Task::~Task()
{}

# pragma message(TODO "We have a copy of a 'region' here, change interface to avoid that")
void
Task::precedes(const Task * successor, const Region & region)
{
    assert(!region.is_empty());
    assert(this->nedges < TASK_MAX_EDGES);

    task_edge_t edge = {
        .successor = successor,
        .region = region
    };
    this->edges.push_back(edge);
}
