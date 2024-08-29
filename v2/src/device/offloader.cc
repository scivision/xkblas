# include "offloader.hpp"
# include "xkblas-context.h"

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
    uint16_t prefix[XKBLAS_STREAM_TYPE_ALL+1];

    prefix[0] = 0;
    for (int stype = 0 ; stype < XKBLAS_STREAM_TYPE_ALL ; ++stype)
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
Offloader::is_empty(xkblas_stream_type_t stype) const
{
    int err = 0;

    unsigned int bgn = (stype == XKBLAS_STREAM_TYPE_ALL) ?                      0 : stype;
    unsigned int end = (stype == XKBLAS_STREAM_TYPE_ALL) ? XKBLAS_STREAM_TYPE_ALL : stype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
        for (unsigned int i = 0 ; i < this->count[s] ; ++i)
            if (!this->streams[s][i]->is_empty())
                return false;

    return true;
}

int
Offloader::launch_ready_instructions(xkblas_stream_type_t stype)
{
    # pragma message(TODO "Better handling of error in case 'STREAM_ALL'")

    int err = 0;

    unsigned int bgn = (stype == XKBLAS_STREAM_TYPE_ALL) ?                      0 : stype;
    unsigned int end = (stype == XKBLAS_STREAM_TYPE_ALL) ? XKBLAS_STREAM_TYPE_ALL : stype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (unsigned int i = 0 ; i < this->count[s] ; ++i)
        {
            xkblas_stream_t * stream = this->streams[s][i];
            assert(stream);

            err = stream->launch_ready_instructions();
            assert(err == 0 || err == EINPROGRESS);
        }
    }

    return err;
}

int
Offloader::progress_pending_instructions(xkblas_stream_type_t stype, bool blocking)
{
    # pragma message(TODO "Better handling of error in case 'STREAM_ALL'")

    xkblas_context_t * ctx = xkblas_context_get();
    int err = 0;

    unsigned int bgn = (stype == XKBLAS_STREAM_TYPE_ALL) ?                      0 : stype;
    unsigned int end = (stype == XKBLAS_STREAM_TYPE_ALL) ? XKBLAS_STREAM_TYPE_ALL : stype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (unsigned int i = 0 ; i < this->count[s] ; ++i)
        {
            xkblas_stream_t * stream = this->streams[s][i];
            assert(stream);

            int n;
            do {
                err = stream->progress_pending_instructions(blocking);
                n = stream->pending.size();
            } while (s == XKBLAS_STREAM_TYPE_KERN && n > ctx->conf.device.offloader.streams[XKBLAS_STREAM_TYPE_KERN].concurrency);
            assert(err == 0 || err == EINPROGRESS);
        }
    }

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
    const xkblas_stream_type_t stype,               /* IN  */
          xkblas_stream_t ** pstream,               /* OUT */
    const xkblas_stream_instruction_type_t itype,   /* IN  */
          xkblas_stream_instruction_t ** pinstr,    /* OUT */
    const xkblas_stream_callback_t & callback       /* IN */
) {
    assert(pstream);
    assert(pinstr);

    /* retrieve native stream */
    xkblas_stream_t * stream = this->stream_next(stype);

    /* allocation instruction */
    xkblas_stream_instruction_t * instr = stream->instruction_new(itype, callback);

    /* out */
    assert(stream);
    assert(instr);
    *pstream = stream;
    *pinstr  = instr;
}
