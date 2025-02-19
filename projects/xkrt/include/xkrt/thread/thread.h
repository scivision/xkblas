/* ************************************************************************** */
/*                                                                            */
/*   thread.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/19 19:23:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/19 20:51:58 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKRT_THREAD_H__
#  define __XKRT_THREAD_H__

#  include <xkrt/consts.h>

/* thread states */
typedef enum    xkrt_thread_state_t
{
    XKRT_THREAD_UNINITIALIZED,
    XKRT_THREAD_INITIALIZED
}               xkrt_thread_state_t;

/* a thread */
typedef struct  xkrt_thread_t
{
    xkrt_thread_state_t state;
    pthread_t pthread;
}               xkrt_thread_t;

/* team description */
typedef struct  xkrt_team_desc_t
{
    void * (*routine)(xkrt_device_global_id_t, void *);
    void * args;
    xkrt_device_global_id_bitfield_t devices;
}               xkrt_team_desc_t;

/* a team, currently is made of 1 thread max per device, bound onto its closest physical cpu */
typedef struct  xkrt_team_t
{
    xkrt_team_desc_t desc;

    struct {

        // threads
        xkrt_thread_t threads[XKRT_DEVICES_MAX];
        std::atomic<int> nthreads;

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
