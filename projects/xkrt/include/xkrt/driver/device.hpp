/* ************************************************************************** */
/*                                                                            */
/*   device.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:49:01 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEVICE_HPP__
# define __DEVICE_HPP__

# include <stdint.h>    /* uint64_t */

# include <xkrt/conf/conf.h>
# include <xkrt/driver/driver-type.h>
# include <xkrt/driver/thread-worker.hpp>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/area.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/sync/mutex.h>
# include <xkrt/task/task.hpp>

typedef enum    xkrt_device_state_t : uint8_t
{
    XKRT_DEVICE_STATE_DEALLOCATED = 0,
    XKRT_DEVICE_STATE_CREATE      = 1,
    XKRT_DEVICE_STATE_INIT        = 2,
    XKRT_DEVICE_STATE_COMMIT      = 3,
    XKRT_DEVICE_STATE_RUNNING     = 4,
    XKRT_DEVICE_STATE_STOP        = 5,
    XKRT_DEVICE_STATE_STOPPED     = 6,
    XKRT_DEVICE_STATE_FINALISE    = 7,
    XKRT_DEVICE_STATE_FINALIZED   = 8,
    XKRT_DEVICE_STATE_DESTROY     = 9,
    XKRT_DEVICE_STATE_DESTROYED   = 10

}               xkrt_device_state_t;

/* A device virtualize a ressource with its one address space and
   a communication stream between host and the ressource */
typedef struct  xkrt_device_t
{
    /////////////////
    //  ATTRIBUTES //
    /////////////////

    /* the conf */
    xkrt_conf_device_t * conf;

    /* the driver type in [0..XKRT_DRIVER_TYPE_MAX[ */
    xkrt_driver_type_t driver_type;

    /* driver device id in [0..ngpus_for_device] */
    uint8_t driver_id;

    /* global device id in [0, XKRT_DEVICES_MAX[ - host is a virtual device of id 'XKRT_DEVICES_MAX'*/
    xkrt_device_global_id_t global_id;

    /* the device state */
    std::atomic<xkrt_device_state_t> state;

    /* the device worker thread */
    ThreadWorker * thread;

    ///////////
    // STATS //
    ///////////

    # if XKRT_SUPPORT_STATS
    struct {
        struct {
            stats_int_t freed;
            struct {
                stats_int_t total;
                stats_int_t currently;
            } allocated;
        } memory;
    } stats;
    # endif /* XKRT_SUPPORT_STATS */


    //////////////////////
    // MEMORY MANAGMENT //
    //////////////////////

    /* memory areas of that device */
    xkrt_area_t area;

    /* allocate memory */
    xkrt_area_chunk_t * memory_allocate(const size_t size);

    /* deallocate the given chunk */
    void memory_deallocate(xkrt_area_chunk_t * chunk);

    /* free all memory of every areas of that device, resetting their state to chunk0 */
    void memory_reset(void);

    /* set the chunk0 of the device memory area*/
    void memory_set_chunk0(uintptr_t device_ptr, size_t size);

    ///////////////////////
    // STREAM MANAGEMENT //
    ///////////////////////

    /* number of iostream per type */
    int count[XKRT_STREAM_TYPE_ALL];

    /* next stream fifo */
    std::atomic<int> next[XKRT_STREAM_TYPE_ALL];

    /* basic stream */
    xkrt_stream_t ** streams[XKRT_STREAM_TYPE_ALL];

    /* initialize the offloader */
    void offloader_init(
        xkrt_stream_t * (*f_stream_create)(xkrt_device_t * device, xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity)
    );

    /* poll the device for launching and progressing pending instructions in every streams */
    int offloader_poll(void);

    /* return true if the the streams for the given type are all empty */
    bool offloader_streams_are_empty(const xkrt_stream_type_t stype) const;

    /* get next stream to use for submitting an instruction for the given type */
    xkrt_stream_t * offloader_stream_next(const xkrt_stream_type_t type);

    /* launch ready instructions dispatching them in streams of the given type */
    int offloader_stream_instructions_launch(const xkrt_stream_type_t stype);

    /* progress pending instructions in streams of the given type. If blocking is true, also waits for the completion of pending instructions */
    template <bool blocking>
    int offloader_stream_instructions_progress(const xkrt_stream_type_t stype);

    /* create a new instruction and lock the stream */
    void offloader_stream_instruction_new(
        const xkrt_stream_type_t stype,             /* IN  */
              xkrt_stream_t ** pstream,             /* OUT */
        const xkrt_stream_instruction_type_t itype, /* IN  */
              xkrt_stream_instruction_t ** pinstr,  /* OUT */
        const xkrt_callback_t & callback            /* IN */
    );

    /* commit an instruction previously returned with "offloader_stream_instruction_new" and unlock the stream */
    void offloader_stream_instruction_commit(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr);

    /* submit a kernel execution instruction */
    void offloader_stream_instruction_submit_kernel(
        void (*launch)(void * istream, void * instr, xkrt_stream_instruction_counter_t idx),
        void * vargs,
        const xkrt_callback_t & callback
    );

    // TODO : template the memcpy with the view

    /* submit a memory copy instruction */
    void offloader_stream_instruction_submit_copy(
        const memory_view_t           & host_view,
        const xkrt_device_global_id_t   dst_device_global_id,
        const memory_replicate_view_t & dst_device_view,
        const xkrt_device_global_id_t   src_device_global_id,
        const memory_replicate_view_t & src_device_view,
        const xkrt_callback_t         & callback
    );

}               xkrt_device_t;

#endif /* __DEVICE_HPP__ */
