# include "stream.h"
# include "logger/todo.h"

# pragma message(TODO "Make 'init' and 'deinit' methods too ? They are used by drivers, idk if we want C++ drivers...")

static inline void
xkblas_stream_instruction_queue_init(
    xkblas_stream_instruction_queue_t * queue,
    uint8_t * buffer,
    unsigned int capacity
) {
    queue->instr = (xkblas_stream_instruction_t *) buffer;
    queue->capacity = capacity;
    queue->pos.r = 0;
    queue->pos.w = 0;
}

void
xkblas_stream_init(
    xkblas_stream_t * stream,
    xkblas_stream_type_t type,
    unsigned int capacity,
    int (*f_instruction_launch)   (xkblas_stream_t *, xkblas_stream_instruction_t *),
    int (*f_instructions_progress)(xkblas_stream_t *, int)
) {
    stream->type = type;

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

    stream->f_instruction_launch    = f_instruction_launch;
    stream->f_instructions_progress = f_instructions_progress;
}

void
xkblas_stream_deinit(xkblas_stream_t * stream)
{
    assert(stream);
    assert(stream->ready.instr);
    assert(stream->pending.instr);

    free(stream->ready.instr);
}

# pragma message(TODO "do we really need to lock in 'new' and unlock in 'commit' - couldn't we already unlock in 'new' ?")

xkblas_stream_instruction_t *
xkblas_stream_t::instruction_new(
    const xkblas_stream_instruction_type_t itype,
    const xkblas_stream_callback_t & callback
) {

    /* Lock the stream to add a new instruction. */
    while (1)
    {
        // TODO : isn't it an infinite loop ? this is executed by the device
        // thread that only him can empty the queue, and its not being emptied
        // here. I believe these might be remains from an old multi-consumer
        // scheme on instructions
        if (!this->ready.is_full())
        {
            SPINLOCK_LOCK(this->spinlock);
            {
                if (!this->ready.is_full())
                {
                    break ;
                }
            }
            SPINLOCK_UNLOCK(this->spinlock);
        }
    }

    xkblas_stream_instruction_t * instr = this->ready.instr + (this->ready.pos.w % this->ready.capacity);
    XKBLAS_DEBUG("Returning a new instruction at index %d on stream %p", this->ready.pos.w, this);

    /* copy type / callback */
    instr->type = itype;
    instr->callback = callback;

    return instr;
}

int
xkblas_stream_t::commit(
    xkblas_stream_instruction_t * instr
) {
    assert(SPINLOCK_ISLOCKED(this->spinlock));
    assert(instr);

    static const char * NAMES[] = {
        "NOP"     ,
        "COPY_H2H",
        "COPY_H2D",
        "COPY_D2H",
        "COPY_D2D",
        "BARRIER" ,
        "KERN"
    };

    writemem_barrier();
    ++this->ready.pos.w;

    XKBLAS_DEBUG("commiting an instruction of type `%s (%d ready, %d pending)`",
            NAMES[instr->type], this->ready.size(), this->pending.size());

    SPINLOCK_UNLOCK(this->spinlock);

    return 0;
}

static char const * INSTRUCTIONS_NAME[] = {
    "NOP",
    "COPY_H2H",
    "COPY_H2D",
    "COPY_D2H",
    "COPY_D2D",
    "BARRIER",
    "KERN"
};

# pragma message(TODO "do we really need to lock in 'new' and unlock in 'commit' - couldn't we already unlock in 'new' ?")
int
xkblas_stream_t::launch_ready_instructions(void)
{
    XKBLAS_DEBUG("Lauching ready instructions of stream %p (%d ready, %d pending)",
            this, this->ready.size(), this->pending.size());

    assert(this->ready.pos.r <= this->ready.pos.w);
    assert(this->f_instruction_launch);

    int err = 0;
    while (!this->ready.is_empty())
    {
        SPINLOCK_LOCK(this->spinlock);
        {
            if (!this->ready.is_empty())
            {
                int p = this->ready.pos.r % this->ready.capacity;
                xkblas_stream_instruction_t * instr = this->ready.instr + p;
                assert(instr);

                XKBLAS_DEBUG("Decoding instruction `%s` on stream %p (decoding via %p)",
                        INSTRUCTIONS_NAME[instr->type], this, this->f_instruction_launch);

                err = this->f_instruction_launch(this, instr);
                ++this->ready.pos.r;

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
                        XKBLAS_DEBUG("Instruction in progress");
                        int wp = this->pending.pos.w % this->pending.capacity;
                        memcpy(this->pending.instr + wp, this->ready.instr + p, sizeof(xkblas_stream_instruction_t));
                        ++this->pending.pos.w;
                        break ;
                    }

                    case (ENOSYS):
                    {
                        XKBLAS_IMPL("Instruction `%s` not implemented", INSTRUCTIONS_NAME[instr->type]);
                        break ;
                    }

                    default:
                    {
                        XKBLAS_FATAL("Unknown error after decoding instruction");
                    }
                }
            }
        }
        SPINLOCK_UNLOCK(this->spinlock);
    }
    return err;
}

int
xkblas_stream_t::progress_pending_instructions(int blocking)
{
    XKBLAS_DEBUG("Progressing pending instructions of stream %p (%d ready, %d pending)",
            this, this->ready.size(), this->pending.size());

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
    return this->ready.is_empty() && this->pending.is_empty();
}
