# include "stream.h"

int
xkblas_stream_t::submit(
    xkblas_stream_instruction_t * instruction
) {
    assert(instruction);
    XKBLAS_IMPL("submited instruction of type %u", instruction->type);
    // TODO
    return 0;
}

void
xkblas_stream_init(
    xkblas_stream_t * stream,
    xkblas_stream_type_t type,
    unsigned int capacity
) {
    stream->type = type;

    # if 0
    uint8_t * mem = (uint8_t *) malloc(sizeof(xkblas_stream_instruction_t) * capacity * 2);
    assert(mem);

    stream->instr.buffer  = (xkblas_stream_instruction_t *) (mem);
    stream->instr.pending = (xkblas_stream_instruction_t *) (mem + sizeof(xkblas_stream_instruction_t) * capacity);
    # else
    uint8_t * mem = (uint8_t *) malloc(sizeof(xkblas_stream_instruction_t) * capacity);
    assert(mem);

    stream->instr.buffer  = (xkblas_stream_instruction_t *) (mem);

    #endif
    stream->instr.capacity = capacity;

    stream->smax   = 0;
    stream->smax_p = 0;
    stream->max_p  = (uint64_t)-1; /* means not used */
    stream->pos_r  = 0;
    stream->pos_rp = 0;
    stream->pos_w  = 0;
    stream->pos_wp = 0;
    stream->ok_p   = 0;
}

void
xkblas_stream_deinit(xkblas_stream_t * stream)
{
    assert(stream);
    assert(stream->instr.buffer);
    free(stream->instr.buffer);
}
