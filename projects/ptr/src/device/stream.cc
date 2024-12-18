/* ************************************************************************** */
/*                                                                            */
/*   stream.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# if USE_STATS
#  include "runtime.h"
# endif /* USE_STATS */
# include "device/stream.h"
# include "logger/todo.h"

# pragma message(TODO "Make 'init' and 'deinit' methods too ? They are used by drivers, idk if we want C++ drivers...")

const char *
ptr_stream_type_to_str(ptr_stream_type_t type)
{
    switch (type)
    {
        case (PTR_STREAM_TYPE_H2D):
            return "H2D";
        case (PTR_STREAM_TYPE_D2H):
            return "D2H";
        case (PTR_STREAM_TYPE_D2D):
            return "D2D";
        case (PTR_STREAM_TYPE_KERN):
            return "KERN";
        case (PTR_STREAM_TYPE_ALL):
            return "ALL";
        default:
          return NULL;
    }
}

const char *
ptr_stream_instruction_type_to_str(ptr_stream_instruction_type_t type)
{
     switch (type)
     {
         case (PTR_STREAM_INSTR_TYPE_COPY_H2H):
             return "COPY_H2H";
         case (PTR_STREAM_INSTR_TYPE_COPY_H2D):
             return "COPY_H2D";
         case (PTR_STREAM_INSTR_TYPE_COPY_D2H):
             return "COPY_D2H";
         case (PTR_STREAM_INSTR_TYPE_COPY_D2D):
             return "COPY_D2D";
         case (PTR_STREAM_INSTR_TYPE_BARRIER):
             return "BARRIER";
         case (PTR_STREAM_INSTR_TYPE_KERN):
             return "KERN";
        default:
             return NULL;
    }
}

static inline void
ptr_stream_instruction_queue_init(
    ptr_stream_instruction_queue_t * queue,
    uint8_t * buffer,
    ptr_stream_instruction_counter_t capacity
) {
    queue->instr = (ptr_stream_instruction_t *) buffer;
    queue->capacity = capacity;
    queue->pos.r = 0;
    queue->pos.w = 0;
}

# pragma message(TODO "Implement and use constructors instead of this routine")
void
ptr_stream_init(
    ptr_stream_t * stream,
    ptr_stream_type_t type,
    ptr_stream_instruction_counter_t capacity,
    int (*f_instruction_launch)   (ptr_stream_t *, ptr_stream_instruction_t *),
    int (*f_instructions_progress)(ptr_stream_t *, int)
) {
    stream->type = type;
    stream->spinlock = SPINLOCK_INITIALIZER;

    stream->f_instruction_launch    = f_instruction_launch;
    stream->f_instructions_progress = f_instructions_progress;

    uint8_t * mem = (uint8_t *) malloc(sizeof(ptr_stream_instruction_t) * capacity * 2);
    assert(mem);

    ptr_stream_instruction_queue_init(
        &stream->ready,
        mem,
        capacity
    );

    ptr_stream_instruction_queue_init(
        &stream->pending,
        mem + sizeof(ptr_stream_instruction_t) * capacity,
        capacity
    );

    stream->ok_p = 0;
}

void
ptr_stream_deinit(ptr_stream_t * stream)
{
    assert(stream);
    assert(stream->ready.instr);
    assert(stream->pending.instr);

    free(stream->ready.instr);
}

ptr_stream_instruction_t *
ptr_stream_t::instruction_new(
    const ptr_stream_instruction_type_t itype,
    const ptr_callback_t & callback
) {
    if (this->ready.is_full())
        return NULL;

    const ptr_stream_instruction_counter_t w = this->ready.pos.w % this->ready.capacity;
    ptr_stream_instruction_t * instr = this->ready.instr + w;
    instr->type = itype;
    instr->callback = callback;

    return instr;
}

void
ptr_stream_t::complete(const ptr_stream_instruction_counter_t i)
{
    ptr_stream_instruction_t * instr = this->pending.instr + (i % this->pending.capacity);
    assert(instr);

    if (instr->callback.func)
        instr->callback.func(instr->callback.args);

    ++this->pending.pos.r;

    # if USE_STATS
    ptr_context_t * context = ptr_context_get();
    assert(context);
    ++context->stats.streams[this->type].instructions[instr->type].completed;
    # endif /* USE_STATS */
}

int
ptr_stream_t::commit(ptr_stream_instruction_t * instr)
{
    assert(instr);

    LOGGER_DEBUG("commiting an instruction of type `%s` (%d ready, %d pending)`",
            ptr_stream_instruction_type_to_str(instr->type),
            this->ready.size(), this->pending.size());

    ++this->ready.pos.w;

    # if USE_STATS
    ptr_context_t * context = ptr_context_get();
    assert(context);
    ++context->stats.streams[this->type].instructions[instr->type].launched;
    # endif /* USE_STATS */

    return 0;
}

int
ptr_stream_t::launch_ready_instructions(void)
{
//    LOGGER_DEBUG("Launching ready instructions of stream %p of type `%s` (%d ready)",
//            this, ptr_stream_type_to_str(this->type), this->ready.size());

    assert(this->ready.pos.r <= this->ready.pos.w);

    /* launch every ready instructions */
    int err = 0;
    while (!this->ready.is_empty())
    {
        /* retrieve the next instruction to launch at index 'p' */
        ptr_stream_instruction_counter_t p = this->ready.pos.r % this->ready.capacity;
        ++this->ready.pos.r;

        ptr_stream_instruction_t * instr = this->ready.instr + p;
        assert(instr);

        LOGGER_DEBUG("Decoding instruction `%s` on stream %p of type `%s` (decoding via %p)",
            ptr_stream_instruction_type_to_str(instr->type),
            this,
            ptr_stream_type_to_str(this->type),
            this->f_instruction_launch
        );

        assert(this->f_instruction_launch);
        err = this->f_instruction_launch(this, instr);

        switch (err)
        {
            case (0):
            {
                /* no error */
                LOGGER_DEBUG("Instruction completed");
                break ;
            }

            case (EINPROGRESS):
            {
                /* recopy op in pending op if still progressing */

                /* the pending queue must not be full */
                assert(!this->pending.is_full());
                const ptr_stream_instruction_counter_t wp = this->pending.pos.w % this->pending.capacity;
                ++this->pending.pos.w;
                writemem_barrier();

                memcpy(
                    this->pending.instr + wp,
                    this->ready.instr + p,
                    sizeof(ptr_stream_instruction_t)
                );

                break ;
            }

            case (ENOSYS):
            {
                LOGGER_IMPL("Instruction `%s` not implemented",
                        ptr_stream_instruction_type_to_str(instr->type));
                break ;
            }

            default:
            {
                LOGGER_FATAL("Unknown error after decoding instruction");
            }
        }
    }

    return err;
}

int
ptr_stream_t::progress_pending_instructions(int blocking)
{
    // LOGGER_DEBUG("Progressing pending instructions of stream %p of type `%s` (%d pending)",
    //         this, ptr_stream_type_to_str(this->type), this->pending.size());

    assert(this->pending.pos.r <= this->pending.pos.w);
    assert(this->f_instructions_progress);

    int err = this->f_instructions_progress(this, blocking);
    assert((err == 0) || (err == EINPROGRESS));

    return err;
}

/* return true if the stream is empty, false otherwise */
int
ptr_stream_t::is_empty(void) const
{
    return (this->ready.is_empty() && this->pending.is_empty());
}
