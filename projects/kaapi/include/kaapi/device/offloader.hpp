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

# include <kaapi/conf/conf.h>
# include <kaapi/device/stream.h>
# include <kaapi/logger/todo.h>
# include <kaapi/sync/mutex.h>

# define KAAPI_STREAM_
# define KAAPI_STREAM_IO_INSTR_CAPACITY 512

/* Kaapi offload stream is virtual interface to be implemented by a device.
   Offloaders are decoupled from H2D/D2H/D2D/kernel executions.
   The number of stream per type is subject to change at start time
   by reading environement variables. See kaapi_usage.
*/
class Offloader
{
    public:
        Offloader();
        virtual ~Offloader();

    public:

        /* initialise the streams */
        void init(
            kaapi_conf_offloader_t * conf,
            kaapi_stream_t * (*f_stream_create)(kaapi_stream_type_t type, kaapi_stream_instruction_counter_t capacity)
        );

        # pragma message(TODO "Use C++ abstract method and inheritance instead of 'C-style' abstract class")
        kaapi_stream_t* (*f_stream_alloc)(int device_id,  int type, kaapi_stream_instruction_counter_t capacity);
        void (*f_stream_free)(int device_id, kaapi_stream_t * io_stream);

    public:
        int submit(kaapi_stream_instruction_t * instr);

        /* create a new instruction on the given stream type, and return the instruction and the assigned stream for execution */
        void instruction_new(
            const kaapi_stream_type_t stype,               /* IN  */
                  kaapi_stream_t ** stream,                /* OUT */
            const kaapi_stream_instruction_type_t itype,   /* IN  */
                  kaapi_stream_instruction_t ** instr,     /* OUT */
            const kaapi_callback_t & callback       /* IN */
        );

        /* TODO */
        bool is_empty(kaapi_stream_type_t type) const;
        int launch_ready_instructions(kaapi_stream_type_t type);
        int progress_pending_instructions(kaapi_stream_type_t type, bool blocking);

    public:

        /* number of iostream per type */
        int count[KAAPI_STREAM_TYPE_ALL];

        /* next stream fifo */
        std::atomic<int> next[KAAPI_STREAM_TYPE_ALL];

        /* basic stream */
        kaapi_stream_t ** streams[KAAPI_STREAM_TYPE_ALL];

    private:

        /* get next stream for the given type */
        kaapi_stream_t * stream_next(kaapi_stream_type_t type);


};

#endif /* __OFFLOADER_HPP__ */
