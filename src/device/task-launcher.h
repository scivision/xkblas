#ifndef __TASK_LAUNCHER_H__
# define __TASK_LAUNCHER_H__

# include "device/task.hpp"

typedef struct  task_launcher_t
{
    /* the task to launch */
    const Task * task;

    /* the target func to use in the format */
    uint8_t target;

    // TODO : bad design
    /* task argument */
    void * handle;

}               task_launcher_t;

#endif /* __TASK_LAUNCHER_H__ */
