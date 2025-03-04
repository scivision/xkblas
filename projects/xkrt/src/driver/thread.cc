/* ************************************************************************** */
/*                                                                            */
/*   thread.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/04 05:41:59 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/driver/thread.hpp>
# include <xkrt/logger/logger.h>

# include <cassert>
# include <cstring>

thread_local Thread * __TLS;

void
Thread::init(void)
{
    assert(!__TLS);
    __TLS = new Thread();
}

void
Thread::deinit(void)
{
    assert(__TLS);
    delete __TLS;
}

Thread *
Thread::self(void)
{
    if (__TLS == NULL)
        Thread::init();
    assert(__TLS);
    return __TLS;
}

Thread::Thread() :
    cpuset(),
    implicit_task(TASK_FORMAT_NULL, TASK_FLAG_DOMAIN),
    current_task(&this->implicit_task),
    memory_stack_bottom(NULL),
    capacity(THREAD_MAX_MEMORY),
    queue()
    // sleep(),
    // deptrees()
    # ifndef NDEBUG
    , tasks()
    # endif /* NDEBUG */
{
    while (1)
    {
        this->memory_stack_bottom = (uint8_t *) malloc(this->capacity);
        if (this->memory_stack_bottom)
            break ;

        this->capacity = (size_t) (this->capacity * 2 / 3);
        if (this->capacity == 0)
            this->memory_stack_bottom = NULL;
    }
    this->memory_stack_ptr = this->memory_stack_bottom;
    assert(this->memory_stack_bottom);

    pthread_mutex_init(&this->sleep.lock, 0);
    pthread_cond_init (&this->sleep.cond, 0);
    this->sleep.sleeping = false;

    task_dom_info_t * dom = TASK_DOM_INFO(&this->implicit_task);
    new (dom) task_dom_info_t();
    # ifndef NDEBUG
    snprintf(this->implicit_task.label, sizeof(this->implicit_task.label), "implicit");
    # endif
}

Thread::~Thread()
{
    free(this->memory_stack_ptr);
}

void
Thread::warmup(void)
{
    // touches every pages to avoid minor page faults later during the execution
    size_t pagesize = (size_t) getpagesize();
    uint8_t * ptr = this->memory_stack_ptr;
    for (uint8_t * ptr = this->memory_stack_ptr ; ptr < this->memory_stack_bottom + THREAD_MAX_MEMORY ; ptr += pagesize)
        *ptr = 42;
}

task_t *
Thread::allocate_task(const size_t size)
{
    # if 1
    if (this->memory_stack_ptr >= this->memory_stack_bottom + THREAD_MAX_MEMORY)
        LOGGER_FATAL("Stack overflow ! Increase `THREAD_MAX_MEMORY` and recompile");
    task_t * task = (task_t *) this->memory_stack_ptr;
    this->memory_stack_ptr += size;
    return task;
    # else
    return (uint8_t *) malloc(size);
    # endif
}

void
Thread::deallocate_all_tasks(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}

void
Thread::push(task_t * const & task)
{
    this->queue.push(task);
    this->wakeup();
}

task_t *
Thread::pop(void)
{
    /* this is true as we only have 1 worker per device currently */
    assert(Thread::self() == this);
    return this->queue.pop();
}

void
Thread::pause(void)
{
    assert(Thread::self() == this);
    pthread_mutex_lock(&this->sleep.lock);
    {
        this->sleep.sleeping = true;
        while (this->sleep.sleeping)
        {
            pthread_cond_wait(&this->sleep.cond, &this->sleep.lock);
        }
    }
    pthread_mutex_unlock(&this->sleep.lock);
}

void
Thread::wakeup(void)
{
    pthread_mutex_lock(&this->sleep.lock);
    if (this->sleep.sleeping)
    {
        this->sleep.sleeping = false;
        pthread_cond_signal(&this->sleep.cond);
    }
    pthread_mutex_unlock(&this->sleep.lock);
}

# ifndef NDEBUG
void
Thread::report_tasks(void)
{
    int summary[TASK_STATE_MAX];
    memset(summary, 0, sizeof(summary));

    for (size_t i = 0 ; i < this->tasks.size() ; ++i)
    {
        task_t * task = this->tasks[i];
        assert(task);

        LOGGER_WARN("%4lu - %12s - %s", i, task_state_to_str(task->state.value), task->label);
        ++summary[task->state.value];
    }

    LOGGER_WARN("Summary");
    for (int i = 0 ; i < TASK_STATE_MAX ; ++i)
        LOGGER_WARN("  %12s: %6d", task_state_to_str((task_state_t)i), summary[i]);
}

# endif /* NDEBUG */
