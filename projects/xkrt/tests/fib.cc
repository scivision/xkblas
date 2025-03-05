/* ************************************************************************** */
/*                                                                            */
/*   fib.cc                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/05 04:56:05 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

# define N 20
# define CUTOFF_DEPTH 5

static const int fib_values[] = {
    1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610,
    987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368, 75025,
    121393, 196418, 317811, 514229, 832040, 1346269, 2178309, 3524578,
    5702887, 9227465, 14930352, 24157817, 39088169, 63245986, 102334155,
    165580141, 267914296, 433494437, 701408733, 1134903170, 1836311903
};

static_assert(N < sizeof(fib_values) / sizeof(int));

static xkrt_runtime_t runtime;
static task_format_id_t fmtid;

constexpr task_flag_bitfield_t flags = TASK_FLAG_ZERO;
constexpr size_t task_size = task_compute_size(flags, 0);

static inline int
fib(int n, int depth)
{
    if (n <= 2)
        return n;

    int fn1, fn2;
    if (depth >= CUTOFF_DEPTH)
    {
        fn1 = fib(n-1, depth+1);
        fn2 = fib(n-2, depth+1);
    }
    else
    {
        Thread * thread = Thread::self();

        // shared(fn1) firstprivate(n, depth)
        task_t * t1 = thread->allocate_task(task_size + sizeof(int *) + 2 * sizeof(int));
        new(t1) task_t(fmtid, flags);
        int ** shared1        = (int **) ((char*)t1 + sizeof(task_t));
        int *  firstprivate11 = (int *)  ((char*)t1 + sizeof(task_t) + sizeof(int *));
        int *  firstprivate12 = (int *)  ((char*)t1 + sizeof(task_t) + sizeof(int *) + sizeof(int));
        *shared1        = &fn1;
        *firstprivate11 = n - 1;
        *firstprivate12 = depth+1;
        runtime.task_commit(t1);

        // shared(fn2) firstprivate(n, depth)
        task_t * t2 = thread->allocate_task(task_size + sizeof(int *) + 2 * sizeof(int));
        new(t2) task_t(fmtid, flags);
        int ** shared2        = (int **) ((char*)t2 + sizeof(task_t));
        int *  firstprivate21 = (int *)  ((char*)t2 + sizeof(task_t) + sizeof(int *));
        int *  firstprivate22 = (int *)  ((char*)t1 + sizeof(task_t) + sizeof(int *) + sizeof(int));
        *shared2        = &fn2;
        *firstprivate21 = n - 2;
        *firstprivate22 = depth+1;
        runtime.task_commit(t2);

        runtime.task_wait();
    }
    return fn1 + fn2;
}

static void
body_host(task_t * task)
{
          int ** fn     = (      int **) ((char*)task + sizeof(task_t));
    const int *  n      = (const int *)  ((char*)task + sizeof(task_t) + sizeof(int *));
    const int *  depth  = (const int *)  ((char*)task + sizeof(task_t) + sizeof(int *) + sizeof(int));
    *fn[0] = fib(*n, *depth);
}

static void *
main_team(xkrt_team_t * team, int tid)
{
    LOGGER_INFO("Thread %d running", tid);
    return NULL;

    if (tid == 0)
    {
        // warmup
        fib(8, 0);
        runtime.task_wait();

        // run
        double t0 = xkrt_get_nanotime();
        int r = fib(N, 0);
        runtime.task_wait();
        double tf = xkrt_get_nanotime();
        LOGGER_INFO("Fib(%d) = %d - took %.2lf s", N, r, (tf - t0) / (double)1e9);
        assert(r == fib_values[N]);
    }
    runtime.team_barrier(team);
    return NULL;
}

static int
get_ncpus(void)
{
    // Allocate, initialize, and perform topology detection
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    // Try to get the number of CPU cores from topology
    int depth = hwloc_get_type_depth(topology, HWLOC_OBJ_CORE);
    int r = hwloc_get_nbobjs_by_depth(topology, depth);

    // Destroy topology object and return
    hwloc_topology_destroy(topology);

    return r;
}

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    // register task format
    task_format_t format;
    memset(format.f, 0, sizeof(format.f));
    format.f[TASK_FORMAT_TARGET_HOST] = (task_format_func_t) body_host;
    snprintf(format.label, sizeof(format.label), "fib");
    fmtid = task_format_create(&(runtime.formats.list), &format);

    xkrt_team_t team = {
        .desc = {
            .routine = main_team,
            .args = NULL,
            .nthreads = get_ncpus(),
            .binding = {
                .mode = XKRT_TEAM_BINDING_MODE_COMPACT,
                .places = XKRT_TEAM_BINDING_PLACES_CORE,
            }
        }
    };

    runtime.team_create(&team);
    runtime.team_join(&team);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
