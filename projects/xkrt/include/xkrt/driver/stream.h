/* ************************************************************************** */
/*                                                                            */
/*   stream.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/03 05:31:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STREAM_HPP__
# define __STREAM_HPP__

# include <xkrt/support.h>
# include <xkrt/driver/stream-instruction.h>
# include <xkrt/driver/stream-type.h>
# include <xkrt/stats/stats.h>
# include <xkrt/sync/lockable.hpp>
# include <xkrt/thread/thread.h>

# include <atomic>

class xkrt_stream_instruction_queue_t
{
    public:

        xkrt_stream_instruction_t * instr;                /* instructions buffer */
        xkrt_stream_instruction_counter_t capacity;       /* buffer capacity */
        struct {
            volatile xkrt_stream_instruction_counter_t r; /* first instruction to process */
            volatile xkrt_stream_instruction_counter_t w; /* next position for inserting instructions */
        } pos;

    public:

        /* methods */
        int
        is_full(void) const
        {
            return (this->pos.w - this->pos.r == this->capacity);
        }

        int
        is_empty(void) const
        {
            return (this->pos.r == this->pos.w);
        }

        xkrt_stream_instruction_counter_t
        size(void) const
        {
            return (this->pos.w - this->pos.r);
        }
};

# pragma message(TODO "make this a C++ class and use inheritance/pure virtual - currently hybrid of C struct C++ class :(")

/* this is a 'xkrt_io_stream' equivalent */
class xkrt_stream_t : public Lockable
{
    public:

        /* the type of that stream */
        xkrt_stream_type_t type;

        /* queue for ready instruction */
        xkrt_stream_instruction_queue_t ready;

        /* queue for pending instructions to progress */
        xkrt_stream_instruction_queue_t pending;

        # if XKRT_SUPPORT_STATS
        struct {
            struct {
                stats_int_t commited;
                stats_int_t completed;
            } instructions[XKRT_STREAM_INSTR_TYPE_MAX];
            stats_int_t transfered;
        } stats;
        # endif /* XKRT_SUPPORT_STATS */

        /* launch a stream instruction */
        int (*f_instruction_launch)(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr, xkrt_stream_instruction_counter_t idx);

        /* progrtream instruction */
        int (*f_instructions_progress)(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr, xkrt_stream_instruction_counter_t idx);

        /* wait  instructions completion on a stream */
        int (*f_instructions_wait)(xkrt_stream_t * stream);

    public:

        /* allocate a new instruction to the stream (must then be commited via 'commit') */
        xkrt_stream_instruction_t * instruction_new(
            const xkrt_stream_instruction_type_t itype,
            const xkrt_callback_t & callback
        );

        /* complete the instruction at the i-th position in the pending queue (invoke the callback) */
        void complete(const xkrt_stream_instruction_counter_t i);

        /* commit the instruction to the stream (must be allocated via 'instruction_new') */
        int commit(xkrt_stream_instruction_t * instruction);

        /* launch instructions, and may generate pending instructions */
        int launch_ready_instructions(void);

        /* progress pending instructions */
        int progress_pending_instructions(void);

        /* (internal) complete all instructions to 'ok_p' */
        void complete_instructions(const xkrt_stream_instruction_counter_t ok_p);

        /* wait for completion of all pending instructions */
        void wait_pending_instructions(void);

        /* return true if the stream is full of instructions, false otherwise */
        int is_full(void) const;

        /* return true if the stream is empty, false otherwise */
        int is_empty(void) const;


};  /* xkrt_stream_t */

void xkrt_stream_init(
    xkrt_stream_t * stream,
    xkrt_stream_type_t type,
    xkrt_stream_instruction_counter_t capacity,
    int (*f_instruction_launch)(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr, xkrt_stream_instruction_counter_t idx),
    int (*f_instructions_progress)(xkrt_stream_t * stream, xkrt_stream_instruction_t * instr, xkrt_stream_instruction_counter_t idx),
    int (*f_instructions_wait)(xkrt_stream_t * stream)
);

void xkrt_stream_deinit(xkrt_stream_t * stream);

#endif /* __STREAM_HPP__ */
