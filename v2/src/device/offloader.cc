# include "offloader.hpp"

Offloader::Offloader() {}

Offloader::~Offloader() {}

int
Offloader::submit(xkblas_stream_instruction_t * instruction)
{
    (void) instruction;
    // TODO
    return 0;
}

void
lol(
) {
}

void
Offloader::init(
    xkblas_conf_offloader_t * conf,
    xkblas_stream_t * (*f_stream_create)(xkblas_stream_type_t type, unsigned int capacity)
) {
    assert(conf);
    assert(f_stream_create);

    memset(this->next, 0, sizeof(this->next));

    uint16_t cnt = 0;
    uint16_t prefix[XKBLAS_STREAM_ALL+1];

    prefix[0] = 0;
    for (int stype = 0 ; stype < XKBLAS_STREAM_ALL ; ++stype)
    {
        cnt += conf->streams[stype].n;
        prefix[stype+1] = cnt;
    }

    xkblas_stream_t ** all_streams = (xkblas_stream_t **) malloc(sizeof(xkblas_stream_t *) * cnt);
    assert(all_streams);

    int stype = 0;
    this->streams[stype] = all_streams;
    for (int i = 0 ; i < cnt ; ++i)
    {
        all_streams[i] = f_stream_create(static_cast<xkblas_stream_type_t>(stype), conf->capacity);
        assert(all_streams[i]);

        if (i >= prefix[stype+1])
            this->streams[++stype] = all_streams + i;
    }
}


bool
Offloader::is_empty(xkblas_stream_type_t type) const
{
    return true;
}

int
Offloader::process_instruction(xkblas_stream_type_t type)
{
    return 0;
}

int
Offloader::test(xkblas_stream_type_t type)
{
    return 0;
}

int
Offloader::wait(xkblas_stream_type_t type)
{
    return 0;
}

xkblas_stream_instruction_t *
Offloader::instruction_new(xkblas_stream_instruction_type_t type)
{
    xkblas_stream_instruction_t * instr = NULL;
    assert(instr);

    instr->type = type;
    return instr;
}
