# include "offloader.hpp"

Offloader::Offloader() {}

Offloader::~Offloader() {}

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
        this->count[stype] = conf->streams[stype].n;
        cnt += this->count[stype];
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

xkblas_stream_t *
Offloader::stream_next(xkblas_stream_type_t stype)
{
    /* find native stream to use */
    int count = this->count[stype];
    switch (count)
    {
        case (0):
            XKBLAS_FATAL("No stream of type %d", stype);

        case (1):
            return this->streams[stype][0];

        default:
        {
            /* TODO : could be relaxed ? and maybe even non atomic, as it is only call by the device thread */
            int snext = this->next[stype].fetch_add(1, std::memory_order_seq_cst) % count;

            // TODO : track pending instr here ?

            return this->streams[stype][snext];
        }
    }
}

void
Offloader::instruction_new(
    xkblas_stream_type_t stype,             /* IN  */
    xkblas_stream_t ** stream,              /* OUT */
    xkblas_stream_instruction_type_t itype, /* IN  */
    xkblas_stream_instruction_t ** instr    /* OUT */
) {
    assert(stream);
    assert(instr);

    /* retrieve native stream */
    *stream = stream_next(stype);
    assert(*stream);

    /* allocate instruction */
    *instr = (*stream)->instr.buffer + ((*stream)->pos_w % (*stream)->instr.capacity);
    assert(*instr);

    (*instr)->type = itype;
}
