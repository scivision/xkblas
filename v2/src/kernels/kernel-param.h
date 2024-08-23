#ifndef __KERNEL_PARAM_H__
# define __KERNEL_PARAM_H__

# include "device/task.hpp"

typedef struct  task_kernel_param_t
{
    const Task * task;
    void * handle;
}               task_kernel_param_t;

#endif /* __KERNEL_PARAM_H__ */
