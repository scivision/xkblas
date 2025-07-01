/* ************************************************************************** */
/*                                                                            */
/*   register.cc                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/10/07 14:28:00 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/04 23:25:00 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>

constexpr int                         ac = 1;
constexpr task_flag_bitfield_t     flags = TASK_FLAG_DEPENDENT;
constexpr               size_t task_size = task_compute_size(flags, ac);
constexpr               size_t args_size = 0;

int
xkrt_runtime_t::memory_register_async(
    xkrt_team_t * team,
    void * ptr,
    const size_t chunk_size,
    int n
) {
    LOGGER_FATAL("Not implemented");

    xkrt_thread_t * tls = xkrt_thread_t::get_tls();

    // null format, the registration occurs during the fetching/fetched state
    const task_format_id_t fmtid = TASK_FORMAT_NULL;

    for (int i = 0 ; i < n ; ++i)
    {
        // inserts the interval in the tree to ensure they exist
        const uintptr_t a = ((const uintptr_t) ptr) + (i+0) * chunk_size;
        const uintptr_t b = ((const uintptr_t) ptr) + (i+1) * chunk_size;

        // create a task that will register/pin/unpin the memory
        task_t * task = tls->allocate_task(task_size + args_size);
        new(task) task_t(fmtid, flags);

        #ifndef NDEBUG
        snprintf(task->label, sizeof(task->label), "memory_register_async");
        #endif

        task_dep_info_t * dep = TASK_DEP_INFO(task);
        new (dep) task_dep_info_t(ac);

        access_t * accesses = TASK_ACCESSES(task, flags);
        new(accesses + 0) access_t(task, a, b, ACCESS_MODE_PIN, ACCESS_CONCURRENCY_COMMUTATIVE);

        tls->commit(task, xkrt_team_task_enqueue, this, team);
    }

    return 0;
}
