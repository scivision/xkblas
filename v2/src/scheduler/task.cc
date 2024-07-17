# include "task.hpp"

Task::Task(task_body_t body) :
    body(body),
    edges(),
    accesses(),
    wc(1)
{}

Task::~Task()
{}
