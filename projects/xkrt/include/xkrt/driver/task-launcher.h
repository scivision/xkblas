/* ************************************************************************** */
/*                                                                            */
/*   task-launcher.h                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:29:10 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __TASK_LAUNCHER_H__
# define __TASK_LAUNCHER_H__

# include <xkrt/task/task.hpp>
# include <xkrt/task/task-format.h>

typedef struct  task_launcher_t
{
    /* the task to launch */
    const Task * task;

    /* the target func to use in the format */
    task_format_target_t target;

    // TODO : bad design
    /* task argument */
    void * handle;

}               task_launcher_t;

#endif /* __TASK_LAUNCHER_H__ */
