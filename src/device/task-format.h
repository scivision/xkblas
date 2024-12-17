/* ************************************************************************** */
/*                                                                            */
/*   task-format.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __TASK_FORMAT_H__
# define __TASK_FORMAT_H__

# include <stdint.h>

# if 0

enum task_kern_t : uint8_t
{
    TASK_KERN_NOOP      = 0,

    TASK_KERN_GEMM      = 1,
    TASK_KERN_TRSM      = 2,
    TASK_KERN_COPYSCALE = 3,

    TASK_KERN_MAX       = 4,
};

enum task_kern_precision_t : uint8_t
{
    TASK_KERN_PRECISION_NONE = 0,
    TASK_KERN_PRECISION_B    = 1,    /* byte */
    TASK_KERN_PRECISION_S    = 2,    /* float */
    TASK_KERN_PRECISION_C    = 3,    /* float complex */
    TASK_KERN_PRECISION_D    = 4,    /* double */
    TASK_KERN_PRECISION_Z    = 5,    /* double complex */
};

# endif

/* maximum number of function version per task */
# define TASK_FORMAT_FUNC_MAX 4

typedef enum    task_format_target_t : uint8_t
{
    TASK_FORMAT_TARGET_DRIVER  = 0,
    TASK_FORMAT_TARGET_HOST    = 1,
}               task_format_target_t;

typedef struct  task_format_t
{
    /* kernel launch */
    void (*f[TASK_FORMAT_FUNC_MAX])(void * args);

    /* the target of that task format :
     * - DRIVER - means the func must be executed as part of an I/O instructions on a device
     * - HOST   - means the func must be executed in the host-thread directly
     */
    task_format_target_t target;

    /* a label */
    char label[16];

}               task_format_t;

/* maximum number of task format */
typedef uint8_t task_format_id_t;

# define TASK_FORMAT_MAX ((1 << (sizeof(task_format_id_t) * 8)) - 1)
# define TASK_FORMAT_NULL 0

task_format_id_t task_format_create(task_format_t * format);
task_format_t * task_format_get(task_format_id_t id);

#endif /* __TASK_TARGET_H__ */
