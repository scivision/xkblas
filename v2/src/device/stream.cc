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
    unsigned int capacity
) {
    stream->type = type;

    uint8_t * mem = (uint8_t *) malloc(sizeof(xkblas_stream_instruction_t) * capacity * 2);
    assert(mem);

    xkblas_stream_instruction_queue_init(
        &stream->queue,
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
    assert(stream->queue.instr);
    assert(stream->pending.instr);
    free(stream->queue.instr);
}

# pragma message(TODO "do we really need to lock in 'new' and unlock in 'commit' - couldn't we already unlock in 'new' ?")

xkblas_stream_instruction_t *
xkblas_stream_t::instruction_new(
    xkblas_stream_instruction_type_t itype
) {

    /* Lock the stream to add a new instruction.  Unlock in the commit
     * operation, so the caller can initialize the instruction in-between */
    while (1)
    {
        if (!this->queue.is_full())
        {
            SPINLOCK_LOCK(this->spinlock);
            {
                if (!this->queue.is_full())
                {
                    break ;
                }
            }
        }
        SPINLOCK_UNLOCK(this->spinlock);
    }

    xkblas_stream_instruction_t * instr = this->queue.instr + (this->queue.pos.w % this->queue.capacity);
    instr->type = itype;

    return instr;
}

int
xkblas_stream_t::commit(
    xkblas_stream_instruction_t * instr
) {
    XKBLAS_IMPL("commiting instruction of type %u", instr->type);

    assert(instr);
    assert(instr == this->queue.instr + (this->queue.pos.w % this->queue.capacity));
    assert(SPINLOCK_ISLOCKED(this->spinlock));

    /* assuming TSO, so writemen barrier is enough */
    writemem_barrier();
    ++this->queue.pos.w;

    SPINLOCK_UNLOCK(this->spinlock);

    return 0;
}

int
xkblas_stream_t::process_instructions(void)
{
    int err = 0;

    assert(this->queue.pos.r <= this->queue.pos.w);

    while (!this->queue.is_empty())
    {
        SPINLOCK_LOCK(this->spinlock);
        {
            if (!this->queue.is_empty())
            {
                int p = this->queue.pos.r % this->queue.capacity;
                // TODO : call this
                // err = stream->f_stream_decode_ioinstruction(stream->device, ios, &ios->instr[p]);
                assert(err == 0 || err == EINPROGRESS);
                ++this->queue.pos.r;

                /* recopy op in pending op if still progressing */
                if (err == EINPROGRESS)
                {
                    int wp = this->pending.pos.w % this->pending.capacity;
                    this->pending.instr[wp] = this->queue.instr[p];
                    ++this->pending.pos.w;
                }
            }
        }
        SPINLOCK_UNLOCK(this->spinlock);
    }

    return err;
}
