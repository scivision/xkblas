/* ************************************************************************** */
/*                                                                            */
/*   thread.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/19 19:23:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/06 06:58:09 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKRT_THREAD_H__
#  define __XKRT_THREAD_H__

#  include <xkrt/consts.h>
#  include <xkrt/sync/spinlock.h>
#  include <xkrt/task/task.hpp>
#  include <xkrt/thread/deque.hpp>

#  include <pthread.h>
#  include <atomic>

/* thread states */
typedef enum    xkrt_thread_state_t
{
    XKRT_THREAD_UNINITIALIZED   = 0,
    XKRT_THREAD_INITIALIZED     = 1
}               xkrt_thread_state_t;

struct xkrt_team_node_t;

/* a thread */
typedef struct  xkrt_thread_t
{
    /* the thread state, use for synchronizing */
    xkrt_thread_state_t state;

    /* the pthread */
    pthread_t pthread;

    /* the thread tid */
    int tid;

    /* the thread deque */
    xkrt_deque_t<task_t *, 4096> deque;

}               xkrt_thread_t;

//  NOTES
//
//  A binary tree of height 'n' has
//      'm' nodes  with 2^(n-1) + 1 <= m <= 2^n - 1
//  and 'k' leaves with           1 <= k <= 2^(n-1)
//
//  <=>
//
//  Given a binary tree with 'k' leaves, its height 'n' must verify
//     1 <=      k      <= 2^(n-1)
// <=> 0 <= log2(k)     <= n-1
// <=>      log2(k) + 1 <= n
//                                     _             _
//  So we need a tree of height 'n' = |   log2(k) + 1 | to represent the 'k' threads

/* type of nodes in the tree */
typedef enum    xkrt_team_node_type_t
{
    XKRT_TEAM_NODE_TYPE_HYPERTHREAD = 0,    // hyperthread
    XKRT_TEAM_NODE_TYPE_CORE        = 1,    // core
    XKRT_TEAM_NODE_TYPE_CACHE_L1    = 2,    // shared cache, L2 or L3 typically
    XKRT_TEAM_NODE_TYPE_CACHE_L2    = 3,    // shared cache, L2 or L3 typically
    XKRT_TEAM_NODE_TYPE_CACHE_L3    = 4,    // shared cache, L2 or L3 typically
    XKRT_TEAM_NODE_TYPE_NUMA        = 5,    // numa node
    XKRT_TEAM_NODE_TYPE_SOCKET      = 6,    // full dram
    XKRT_TEAM_NODE_TYPE_MACHINE     = 7     // multi socket system
}               xkrt_team_node_type_t;

/* a node in the topology graph */
typedef struct  xkrt_team_node_t
{
    /* the node type */
    xkrt_team_node_type_t type;

    /* the thread owning that node */
    xkrt_thread_t * thread;

}               xkrt_team_node_t;

typedef enum    xkrt_team_binding_mode_t
{
    XKRT_TEAM_BINDING_MODE_COMPACT,
    XKRT_TEAM_BINDING_MODE_SPREAD,
}               xkrt_team_binding_mode_t;

typedef enum    xkrt_team_binding_places_t
{
    XKRT_TEAM_BINDING_PLACES_HYPERTHREAD,
    XKRT_TEAM_BINDING_PLACES_CORE,
    XKRT_TEAM_BINDING_PLACES_L1,
    XKRT_TEAM_BINDING_PLACES_L2,
    XKRT_TEAM_BINDING_PLACES_L3,
    XKRT_TEAM_BINDING_PLACES_NUMA,
    XKRT_TEAM_BINDING_PLACES_DEVICE,
    XKRT_TEAM_BINDING_PLACES_SOCKET,
    XKRT_TEAM_BINDING_PLACES_MACHINE,
}               xkrt_team_binding_places_t;

/**
 *  The supported combinations are:
 *    (mode = COMPACT, places = DEVICE)
 *      -> that will compactly bind 1 thread per device
 *
 *    (mode = SPREAD, places = MACHINE) with any nthreads
 *      -> that will spread threads across all cores of the machine
 */
typedef struct  xkrt_team_binding_t
{
    xkrt_team_binding_mode_t mode;
    xkrt_team_binding_places_t places;

}               xkrt_team_binding_t;

/* team description */
struct xkrt_team_t;
typedef struct  xkrt_team_desc_t
{
    // routine that will be executed by each thread
    void * (*routine)(struct xkrt_team_t * team, struct xkrt_thread_t * thread);

    // user arguments
    void * args;

    // number of threads to spawn
    int nthreads;

    // type of the team
    xkrt_team_binding_t binding;

    // TODO : add flags with enabled feature ? (barrier, critical, etc...)

}               xkrt_team_desc_t;

/* a team, currently is made of 1 thread max per device, bound onto its closest physical cpu */
typedef struct  xkrt_team_t
{
    // team description, to be filled by the user before forking it
    xkrt_team_desc_t desc;

    struct {

        // threads
        xkrt_thread_t * threads;
        int nthreads;

        // barrier
        struct {
            std::atomic<int> n;     /* for spawned threads to sync */
            volatile int version;
            pthread_cond_t cond;    /* to sleep threads when synchronizing */
            pthread_mutex_t mtx;
        } barrier;

        // critical
        struct {
            pthread_mutex_t mtx;
        } critical;

    } priv;

}               xkrt_team_t;

# endif /* __XKRT_THREAD_H__ */
