# ifndef __TEAM_H__
#  define __TEAM_H__

# include <atomic>
# include <pthread.h>
# include <hwloc.h>
# include <xkbm/thread.h>

typedef struct  xkbm_team_t
{
    void (*routine)(xkbm_team_t *, int, void *);
    void * args;

    xkbm_thread_t * threads;    /* spawned threads */
    hwloc_cpuset_t cpuset;
    int nthreads;
    struct {
        std::atomic<int> n;     /* for spawned threads to sync */
        volatile int version;
        pthread_cond_t cond;    /* to sleep threads when synchronizing */
        pthread_mutex_t mtx;
    } barrier;
}               xkbm_team_t;

void xkbm_team_work(
    hwloc_cpuset_t cpuset,
    void (*routine)(xkbm_team_t *, int, void *),
    void * args
);

void xkbm_team_barrier(xkbm_team_t *team);

# endif /* __TEAM_H__ */
