#ifndef __TASK_H__
# define __TASK_H__

# include <vector>

typedef struct  Task
{
    std::vector<struct Task *> successors;

    Task() : successors() {}
    Task(const Task & task) : successors(task.successors) {}
    ~Task() {}

}               Task;

# define MAX_TASKS 5000000
static Task TASKS[MAX_TASKS] = { Task() };
static int NTASKS = 0;

Task *
task_new(void)
{
    Task * task = &(TASKS[NTASKS++]);
    if (NTASKS > MAX_TASKS)
    {
        fprintf(stderr, "Increase 'MAX_TASKS' in %s:%d\n", __FILE__, __LINE__);
        exit(0);
    }
    return task;
}

template<int K>
inline void
task_link(const Intervals<K> & rx, Task * x, const Intervals<K> & ry, Task * y)
{
    (void) rx;
    (void) ry;
 // std::cout << rx << " and " << ry << " intersects for " << rx.intersection(ry) << std::endl;

    Task * pred = (Task *) x;
    Task * succ = (Task *) y;
    if (pred->successors.empty() || pred->successors.back() != succ)
        pred->successors.push_back(succ);
}

int
task_nedges(void)
{
    int nedges = 0;
    for (int i = 0 ; i < NTASKS ; ++i)
    {
        Task * task = TASKS + i;
        nedges += task->successors.size();
    }
    return nedges;
}

void
task_clear(void)
{
    NTASKS = 0;
}

void
task_dot(FILE * f)
{
    fprintf(f, "digraph G {\n");

    const char * label = "task";
    for (Task * task = TASKS ; task != TASKS + NTASKS ; ++task)
        fprintf(f, "    \"%p\" [label=\"%s\"] ;\n", task, label);

    for (Task * pred = TASKS ; pred != TASKS + NTASKS ; ++pred)
        for (Task * & succ : pred->successors)
            fprintf(f, "    \"%p\" -> \"%p\" ;\n", pred, succ);

    fprintf(f, "}\n");
}


#endif /* __TASK_H__ */
