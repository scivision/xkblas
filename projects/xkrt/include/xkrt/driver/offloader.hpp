/* ************************************************************************** */
/*                                                                            */
/*   offloader.hpp                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:53:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __OFFLOADER_HPP__
# define __OFFLOADER_HPP__

# include <xkrt/conf/conf.h>
# include <xkrt/driver/stream.h>
# include <xkrt/logger/todo.h>
# include <xkrt/sync/mutex.h>

# include <atomic>

# define XKRT_STREAM_IO_INSTR_CAPACITY 512

/* Xkrt offload stream is virtual interface to be implemented by a device.
   Offloaders are decoupled from H2D/D2H/D2D/kernel executions.
   The number of stream per type is subject to change at start time
   by reading environement variables. See xkrt_usage.
*/
class Offloader
{
    public:
        Offloader();
        virtual ~Offloader();

    public:

        /* initialise the streams */
        void init(
            xkrt_conf_offloader_t * conf,
            xkrt_stream_t * (*f_stream_create)(xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity)
        );

        # pragma message(TODO "Use C++ abstract method and inheritance instead of 'C-style' abstract class")
        xkrt_stream_t* (*f_stream_alloc)(int device_id,  int type, xkrt_stream_instruction_counter_t capacity);
        void (*f_stream_free)(int device_id, xkrt_stream_t * io_stream);

    public:
        int submit(xkrt_stream_instruction_t * instr);

        /* create a new instruction on the given stream type, and return the instruction and the assigned stream for execution */
        void instruction_new(
            const xkrt_stream_type_t stype,                 /* IN  */
                  xkrt_stream_t ** stream,                  /* OUT */
            const xkrt_stream_instruction_type_t itype,     /* IN  */
                  xkrt_stream_instruction_t ** instr,       /* OUT */
            const xkrt_callback_t & callback                /* IN */
        );

        /* TODO */
        bool is_empty(xkrt_stream_type_t type) const;
        int launch_ready_instructions(xkrt_stream_type_t type);
        int progress_pending_instructions(xkrt_stream_type_t type, bool blocking);

    public:

        /* number of iostream per type */
        int count[XKRT_STREAM_TYPE_ALL];

        /* next stream fifo */
        std::atomic<int> next[XKRT_STREAM_TYPE_ALL];

        /* basic stream */
        xkrt_stream_t ** streams[XKRT_STREAM_TYPE_ALL];

    private:

        /* get next stream for the given type */
        xkrt_stream_t * stream_next(xkrt_stream_type_t type);

        /* the conf */
        xkrt_conf_offloader_t * conf;


};

#endif /* __OFFLOADER_HPP__ */
