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

# include <ptr/conf/conf.h>
# include <ptr/device/stream.h>
# include <ptr/logger/todo.h>
# include <ptr/sync/mutex.h>

# define PTR_STREAM_
# define PTR_STREAM_IO_INSTR_CAPACITY 512

/* Kaapi offload stream is virtual interface to be implemented by a device.
   Offloaders are decoupled from H2D/D2H/D2D/kernel executions.
   The number of stream per type is subject to change at start time
   by reading environement variables. See ptr_usage.
*/
class Offloader
{
    public:
        Offloader();
        virtual ~Offloader();

    public:

        /* initialise the streams */
        void init(
            ptr_conf_offloader_t * conf,
            ptr_stream_t * (*f_stream_create)(ptr_stream_type_t type, ptr_stream_instruction_counter_t capacity)
        );

        # pragma message(TODO "Use C++ abstract method and inheritance instead of 'C-style' abstract class")
        ptr_stream_t* (*f_stream_alloc)(int device_id,  int type, ptr_stream_instruction_counter_t capacity);
        void (*f_stream_free)(int device_id, ptr_stream_t * io_stream);

    public:
        int submit(ptr_stream_instruction_t * instr);

        /* create a new instruction on the given stream type, and return the instruction and the assigned stream for execution */
        void instruction_new(
            const ptr_stream_type_t stype,               /* IN  */
                  ptr_stream_t ** stream,                /* OUT */
            const ptr_stream_instruction_type_t itype,   /* IN  */
                  ptr_stream_instruction_t ** instr,     /* OUT */
            const ptr_callback_t & callback       /* IN */
        );

        /* TODO */
        bool is_empty(ptr_stream_type_t type) const;
        int launch_ready_instructions(ptr_stream_type_t type);
        int progress_pending_instructions(ptr_stream_type_t type, bool blocking);

    public:

        /* number of iostream per type */
        int count[PTR_STREAM_TYPE_ALL];

        /* next stream fifo */
        std::atomic<int> next[PTR_STREAM_TYPE_ALL];

        /* basic stream */
        ptr_stream_t ** streams[PTR_STREAM_TYPE_ALL];

    private:

        /* get next stream for the given type */
        ptr_stream_t * stream_next(ptr_stream_type_t type);


};

#endif /* __OFFLOADER_HPP__ */
