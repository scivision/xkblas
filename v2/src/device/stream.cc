# include "stream.h"
# include "logger/todo.h"

# pragma message(TODO "Make 'init' and 'deinit' methods too ? They are used by drivers, idk if we want C++ drivers...")

const char *
xkblas_stream_type_to_str(xkblas_stream_type_t type)
{
    switch (type)
    {
        case (XKBLAS_STREAM_TYPE_H2D):
            return "H2D";
        case (XKBLAS_STREAM_TYPE_D2H):
            return "D2H";
        case (XKBLAS_STREAM_TYPE_D2D):
            return "D2D";
        case (XKBLAS_STREAM_TYPE_KERN):
            return "KERN";
        case (XKBLAS_STREAM_TYPE_ALL):
            return "ALL";
        default:
          return NULL;
    }
}

const char *
xkblas_stream_instruction_type_to_str(xkblas_stream_instruction_type_t type)
{
     switch (type)
     {
         case (XKBLAS_STREAM_INSTR_TYPE_NOP):
             return "NOP";
         case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2H):
             return "COPY_H2H";
         case (XKBLAS_STREAM_INSTR_TYPE_COPY_H2D):
             return "COPY_H2D";
         case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2H):
             return "COPY_D2H";
         case (XKBLAS_STREAM_INSTR_TYPE_COPY_D2D):
             return "COPY_D2D";
         case (XKBLAS_STREAM_INSTR_TYPE_BARRIER):
             return "BARRIER";
         case (XKBLAS_STREAM_INSTR_TYPE_KERN):
             return "KERN";
        default:
             return NULL;
    }
}

static inline void
xkblas_stream_instruction_queue_init(
    xkblas_stream_instruction_queue_t * queue,
    uint8_t * buffer,
    unsigned int capacity
) {
    queue->spinlock = 0;
    queue->instr = (xkblas_stream_instruction_t *) buffer;
    queue->capacity = capacity;
    queue->pos.r = 0;
    queue->pos.w = 0;
}

# pragma message(TODO "Implement and use constructors instead of this routine")
void
xkblas_stream_init(
    xkblas_stream_t * stream,
    xkblas_stream_type_t type,
    unsigned int capacity,
    int (*f_instruction_launch)   (xkblas_stream_t *, xkblas_stream_instruction_t *),
    int (*f_instructions_progress)(xkblas_stream_t *, int)
) {
    stream->type = type;

    stream->f_instruction_launch    = f_instruction_launch;
    stream->f_instructions_progress = f_instructions_progress;

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkblas_stream_instruction_t) * capacity * 2);
    assert(mem);

    xkblas_stream_instruction_queue_init(
        &stream->ready,
        mem,
        capacity
    );

    xkblas_stream_instruction_queue_init(
        &stream->pending,
        mem + sizeof(xkblas_stream_instruction_t) * capacity,
        capacity
    );

    stream->ok_p = 0;
}

void
xkblas_stream_deinit(xkblas_stream_t * stream)
{
    assert(stream);
    assert(stream->ready.instr);
    assert(stream->pending.instr);

    free(stream->ready.instr);
}

xkblas_stream_instruction_t *
xkblas_stream_t::instruction_new(
    const xkblas_stream_instruction_type_t itype,
    const xkblas_stream_callback_t & callback
) {
    if (this->ready.is_full())
        return NULL;

    this->ready.lock();
    {
        if (this->ready.is_full())
        {
            this->ready.unlock();
            return NULL;
        }
    } /* unlocked in the commit routine */

    readmem_barrier();
    const int32_t w = this->ready.pos.w.load(std::memory_order_seq_cst) % this->ready.capacity;
    xkblas_stream_instruction_t * instr = this->ready.instr + w;
    instr->type = itype;
    instr->callback = callback;

    return instr;
}

int
xkblas_stream_t::commit(
    xkblas_stream_instruction_t * instr
) {
    assert(instr);

    XKBLAS_DEBUG("commiting an instruction of type `%s` (%d ready, %d pending)`",
            xkblas_stream_instruction_type_to_str(instr->type),
            this->ready.size(), this->pending.size());

    /* locked from the new_instruction routine */
    {
        this->ready.pos.w.fetch_add(1, std::memory_order_seq_cst);
        writemem_barrier();
    }
    this->ready.unlock();

    return 0;
}

int
xkblas_stream_t::launch_ready_instructions(void)
{
    XKBLAS_DEBUG("Launching ready instructions of stream %p of type `%s` (%d ready)",
            this, xkblas_stream_type_to_str(this->type), this->ready.size());
    assert(this->ready.pos.r <= this->ready.pos.w);

    /* launch every ready instructions */
    int err = 0;
    while (!this->ready.is_empty())
    {
        /* retrieve the next instruction to launch at index 'p' */
        uint64_t p = this->ready.pos.r % this->ready.capacity;
        ++this->ready.pos.r;
        writemem_barrier();

        xkblas_stream_instruction_t * instr = this->ready.instr + p;
        assert(instr);

        XKBLAS_DEBUG("Decoding instruction `%s` on stream %p of type `%s` (decoding via %p)",
            xkblas_stream_instruction_type_to_str(instr->type),
            this,
            xkblas_stream_type_to_str(this->type),
            this->f_instruction_launch
        );

        assert(this->f_instruction_launch);
        err = this->f_instruction_launch(this, instr);

        switch (err)
        {
            case (0):
            {
                /* no error */
                XKBLAS_DEBUG("Instruction completed");
                break ;
            }

            case (EINPROGRESS):
            {
                /* recopy op in pending op if still progressing */

                /* the pending queue must not be full */
                assert(!this->pending.is_full());
                uint64_t wp = this->pending.pos.w % this->pending.capacity;
                ++this->pending.pos.w;
                writemem_barrier();

                memcpy(
                    this->pending.instr + wp,
                    this->ready.instr + p,
                    sizeof(xkblas_stream_instruction_t)
                );

                break ;
            }

            case (ENOSYS):
            {
                XKBLAS_IMPL("Instruction `%s` not implemented",
                        xkblas_stream_instruction_type_to_str(instr->type));
                break ;
            }

            default:
            {
                XKBLAS_FATAL("Unknown error after decoding instruction");
            }
        }
    }
    return err;
}

int
xkblas_stream_t::progress_pending_instructions(int blocking)
{
    XKBLAS_DEBUG("Progressing pending instructions of stream %p of type `%s` (%d pending)",
            this, xkblas_stream_type_to_str(this->type), this->pending.size());
    assert(this->pending.pos.r <= this->pending.pos.w);
    assert(this->f_instructions_progress);

    int err = this->f_instructions_progress(this, blocking);
    assert((err == 0) || (err == EINPROGRESS));

    return err;
}

/* return true if the stream is empty, false otherwise */
int
xkblas_stream_t::is_empty(void) const
{
    return (this->ready.is_empty() && this->pending.is_empty());
}
