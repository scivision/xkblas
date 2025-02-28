/* ************************************************************************** */
/*                                                                            */
/*   device.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/28 01:01:00 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEVICE_HPP__
# define __DEVICE_HPP__

# include <stdint.h>    /* uint64_t */

# include <xkrt/conf/conf.h>
# include <xkrt/driver/driver-type.h>
# include <xkrt/driver/thread.hpp>
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
    XKRT_DEVICE_STATE_DESTROYED   = 7

}               xkrt_device_state_t;

/* Memory info of a device */
typedef struct  xkrt_device_memory_info_t
{
    /* memory capacity */
    size_t capacity;

    /* memory name */
    char name[32];

    /* the area of that memory */
    xkrt_area_t area;

}               xkrt_device_memory_info_t;

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

    /* global device id in [0, XKRT_DEVICES_MAX[ - host is a virtual device of id 'XKRT_DEVICES_MAX' */
    xkrt_device_global_id_t global_id;

    /* the device state */
    std::atomic<xkrt_device_state_t> state;

    /* the device worker thread */
    Thread * thread;

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

    /* memory areas of that device - sorted by performance */
    xkrt_device_memory_info_t memories[XKRT_DEVICE_MEMORIES_MAX];
    int nmemories;

    /* allocate memory on a specific area */
    xkrt_area_chunk_t * memory_allocate_on(const size_t size, int area_idx);

    /* allocate memory */
    xkrt_area_chunk_t * memory_allocate(const size_t size);

    /* deallocate the given chunk */
    void memory_deallocate(xkrt_area_chunk_t * chunk);

    /* free all memory of every area of that device, resetting their state to chunk0 */
    void memory_reset(void);

    /* free all memory of the given area of that device, resetting their state to chunk0 */
    void memory_reset_on(int area_idx);

    /* set chunk0 of an area */
    void memory_set_chunk0(uintptr_t device_ptr, size_t size, int area_idx);

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
    int
    offloader_stream_instructions_progress(
        const xkrt_stream_type_t stype
    ) {
        int err = 0;
        unsigned int bgn = (stype == XKRT_STREAM_TYPE_ALL) ?                    0 : stype;
        unsigned int end = (stype == XKRT_STREAM_TYPE_ALL) ? XKRT_STREAM_TYPE_ALL : stype + 1;
        for (unsigned int s = bgn ; s < end ; ++s)
        {
            for (unsigned int i = 0 ; i < this->count[s] ; ++i)
            {
                xkrt_stream_t * stream = this->streams[s][i];
                assert(stream);

                if (stream->pending.is_empty())
                    continue ;

                xkrt_stream_instruction_counter_t n;
                do {
                    stream->lock();
                    if (blocking)
                    {
                        stream->wait_pending_instructions();
                        err = 0;
                    }
                    else
                        err = stream->progress_pending_instructions();
                    stream->unlock();
                    n = stream->pending.size();
                } while (n > this->conf->offloader.streams[s].concurrency);
                assert(err == 0 || err == EINPROGRESS);
            }
        }
        return 0;
    }

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

    /* copy */
    template <typename HOST_VIEW_T, typename DEVICE_VIEW_T>
    void offloader_stream_instruction_submit_copy(
        const HOST_VIEW_T             & host_view,
        const xkrt_device_global_id_t   dst_device_global_id,
        const DEVICE_VIEW_T           & dst_device_view,
        const xkrt_device_global_id_t   src_device_global_id,
        const DEVICE_VIEW_T           & src_device_view,
        const xkrt_callback_t         & callback
    ) {
        assert(this->global_id == dst_device_global_id || this->global_id == src_device_global_id);

        /* find the instruction type */
        xkrt_stream_instruction_type_t itype;
        const int src_is_host = (src_device_global_id == HOST_DEVICE_GLOBAL_ID) ? 1 : 0;
        const int dst_is_host = (dst_device_global_id == HOST_DEVICE_GLOBAL_ID) ? 1 : 0;

        /* assertions */
        # define IS_1D (std::is_same<HOST_VIEW_T, size_t>()        && std::is_same<DEVICE_VIEW_T, uintptr_t>())
        # define IS_2D (std::is_same<HOST_VIEW_T, memory_view_t>() && std::is_same<DEVICE_VIEW_T, memory_replicate_view_t>())
        static_assert(IS_1D || IS_2D);
        if constexpr(IS_1D) {
            assert(host_view);
            assert(dst_device_view);
            assert(src_device_view);
            itype = ( src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D :
                    ( src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D :
                    (!src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D :
                    (!src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D :
                    XKRT_STREAM_INSTR_TYPE_MAX;
        } else if constexpr(IS_2D) {
            assert(host_view.m);
            assert(host_view.n);
            assert(host_view.sizeof_type);

            assert(dst_device_view.addr);
            assert(dst_device_view.ld);

            assert(src_device_view.addr);
            assert(src_device_view.ld);

            itype = ( src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D :
                    ( src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D :
                    (!src_is_host &&  dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D :
                    (!src_is_host && !dst_is_host) ? XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D :
                    XKRT_STREAM_INSTR_TYPE_MAX;
        } else {
            LOGGER_FATAL("Wrong parameters");
        }

        /* find the type of stream to use */
        xkrt_stream_type_t stype;
        switch(itype)
        {
            # pragma message(TODO "No H2H streams, do we want one ? Currently using H2D stream for H2H copies")
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D):
            {
               stype = XKRT_STREAM_TYPE_H2D;
               break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D):
            {
                stype = XKRT_STREAM_TYPE_D2H;
                break ;
            }

            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D):
            case (XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D):
            {
                stype = XKRT_STREAM_TYPE_D2D;
                break ;
            }

            default:
            {
                LOGGER_FATAL("Impossible occured");
                break ;
            }
        }

        /* create a new instruction and retrieve its offload stream */
        xkrt_stream_t * stream;
        xkrt_stream_instruction_t * instr;
        this->offloader_stream_instruction_new(stype, &stream, itype, &instr, callback);
        assert(stream);
        assert(instr);

        /* create a new copy instruction */
        if constexpr (IS_1D) {
            instr->copy.D1.size = host_view;
            instr->copy.D1.dst_device_addr  = dst_device_view;
            instr->copy.D1.src_device_addr  = src_device_view;
            XKRT_STATS_INCR(stream->stats.transfered, instr->copy.D1.size);
        } else if constexpr (IS_2D) {
            instr->copy.D2.m                = host_view.m;
            instr->copy.D2.n                = host_view.n;
            instr->copy.D2.sizeof_type      = host_view.sizeof_type;
            instr->copy.D2.dst_device_view  = dst_device_view;
            instr->copy.D2.src_device_view  = src_device_view;
            XKRT_STATS_INCR(stream->stats.transfered, host_view.m * host_view.n * host_view.sizeof_type);
        }

        this->offloader_stream_instruction_commit(stream, instr);

        # undef IS_1D
        # undef IS_2D
    }
}               xkrt_device_t;

#endif /* __DEVICE_HPP__ */
