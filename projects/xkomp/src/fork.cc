# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

int
__kmp_invoke_microtask(
    kmpc_micro f,
    int gtid,
    int tid,
    int argc,
    void ** p
) {
    switch (argc)
    {
        default:
            LOGGER_FATAL("Too many args (%d)\n", argc);
        case 0:
            (*f)(&gtid, &tid);
            break;
        case 1:
            (*f)(&gtid, &tid, p[0]);
            break;
        case 2:
            (*f)(&gtid, &tid, p[0], p[1]);
            break;
        case 3:
            (*f)(&gtid, &tid, p[0], p[1], p[2]);
            break;
        case 4:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3]);
            break;
        case 5:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4]);
            break;
        case 6:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5]);
            break;
        case 7:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
            break;
        case 8:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
            break;
        case 9:
            (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
            break;
        case 10:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
           break;
        case 11:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10]);
           break;
        case 12:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11]);
           break;
        case 13:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12]);
           break;
        case 14:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13]);
           break;
        case 15:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14]);
           break;
        case 16:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
           break;
        case 17:
           (*f)(&gtid, &tid, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15], p[16]);
           break;
    }
    return 1;
}

# define TEAM_MAX_ARGS (64)

typedef struct  wargs_t
{
    int argc;
    void * args[TEAM_MAX_ARGS];
    kmpc_micro f;
}               wargs_t;

static void *
fork_call_wrapper(
    xkrt_team_t * team,
    xkrt_thread_t * thread
) {
    assert(team);
    assert(thread);

    wargs_t * wargs = (wargs_t *) team->desc.args;
	__kmp_invoke_microtask(wargs->f, thread->gtid, thread->tid, wargs->argc, wargs->args);

    return NULL;
}

_Thread_local int pushed_num_threads;

// # pragma omp [...] num_threads(X)
extern "C"
void
__kmpc_push_num_threads(
    ident_t * loc,
    kmp_int32 global_tid,
    kmp_int32 num_threads
) {
    pushed_num_threads = num_threads < 0 ? 0 : num_threads;
}

// # pragma omp parallel
extern "C"
void
__kmpc_fork_call(
    ident_t * loc,
    kmp_int32 argc,
    kmpc_micro f,
    ...
) {

    // parse number of threads - see Algorithm 12.1 Determine Number of Threads
    // This is not standard, but whatever for now
    xkomp_t * omp = xkomp_get();
    unsigned int nthreads = pushed_num_threads ? pushed_num_threads :
                            omp->env.OMP_NUM_THREADS ? omp->env.OMP_NUM_THREADS : 0;
    if (nthreads > omp->env.OMP_THREAD_LIMIT)
        nthreads = omp->env.OMP_THREAD_LIMIT;

    if (omp->team.priv.threads == NULL)
    {
        // create the team
        omp->team.desc.binding.mode     = XKRT_TEAM_BINDING_MODE_COMPACT;
        omp->team.desc.binding.places   = XKRT_TEAM_BINDING_PLACES_CORE;
        omp->team.desc.binding.flags    = XKRT_TEAM_BINDING_FLAG_NONE;
        omp->team.desc.routine          = XKRT_TEAM_ROUTINE_PARALLEL_FOR;
        omp->team.desc.args             = malloc(sizeof(wargs_t));
        omp->team.desc.nthreads         = nthreads;
        omp->team.desc.master_is_member = false;

        omp->runtime.team_create(&omp->team);
    }
    assert(omp->team.priv.threads != NULL);
    assert(omp->team.priv.nthreads == nthreads);
    assert(omp->team.desc.args && argc <= TEAM_MAX_ARGS);

    // copy parallel region routine arguments
    va_list args;
    va_start(args, f);
    wargs_t * wargs = (wargs_t *) omp->team.desc.args;
    wargs->argc = argc;
    wargs->f = f;
    for (kmp_int32 i = 0; i < argc; ++i)
        wargs->args[i] = va_arg(args, void *);
    va_end(args);

    // run parallel for
    omp->runtime.team_parallel_for(&omp->team, fork_call_wrapper);

    # if 0
    omp->runtime.team_join(&omp->team);
    free(omp->team.desc.args);
    omp->team.priv.threads = NULL;
    # endif

    // reset pushed num threads
    pushed_num_threads = 0;
}
