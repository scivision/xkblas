/* ************************************************************************** */
/*                                                                            */
/*   offloader.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:58:05 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>
# include <xkrt/device/offloader.hpp>

Offloader::Offloader() {}

Offloader::~Offloader() {}

void
Offloader::init(
    xkrt_conf_offloader_t * conf,
    xkrt_stream_t * (*f_stream_create)(xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity)
) {
    assert(conf);
    assert(f_stream_create);

    /* next stream to use (round robin) */
    memset(this->next, 0, sizeof(this->next));

    /* count total number of stream */
    uint16_t cnt = 0;
    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        this->count[stype] = conf->streams[stype].n;
        cnt += conf->streams[stype].n;
    }

    /* allocate streams array */
    xkrt_stream_t ** all_streams = (xkrt_stream_t **) malloc(sizeof(xkrt_stream_t *) * cnt);
    assert(all_streams);

    /* retrieve stream offset per type */
    uint16_t i = 0;
    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        this->streams[stype] = all_streams + i;
        for (int j = 0 ; j < conf->streams[stype].n ; ++j, ++i)
        {
            all_streams[i] = f_stream_create(static_cast<xkrt_stream_type_t>(stype), conf->capacity);
            assert(all_streams[i]);
        }
    }

    assert(i == cnt);
}

bool
Offloader::is_empty(xkrt_stream_type_t stype) const
{
    int err = 0;

    unsigned int bgn = (stype == XKRT_STREAM_TYPE_ALL) ?                      0 : stype;
    unsigned int end = (stype == XKRT_STREAM_TYPE_ALL) ? XKRT_STREAM_TYPE_ALL : stype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
        for (unsigned int i = 0 ; i < this->count[s] ; ++i)
            if (!this->streams[s][i]->is_empty())
                return false;

    return true;
}

int
Offloader::launch_ready_instructions(xkrt_stream_type_t stype)
{
    # pragma message(TODO "Better handling of error in case 'STREAM_ALL'")

    int err = 0;

    unsigned int bgn = (stype == XKRT_STREAM_TYPE_ALL) ?                      0 : stype;
    unsigned int end = (stype == XKRT_STREAM_TYPE_ALL) ? XKRT_STREAM_TYPE_ALL : stype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (unsigned int i = 0 ; i < this->count[s] ; ++i)
        {
            xkrt_stream_t * stream = this->streams[s][i];
            assert(stream);

            stream->lock();
            err = stream->launch_ready_instructions();
            stream->unlock();

            switch (err)
            {
                case (0):
                case (EINPROGRESS):
                    break ;

                case (ENOSYS):
                {
                    LOGGER_FATAL("Not implemented");
                    break ;
                }

                default:
                {
                    LOGGER_FATAL("Driver implementation of `stream_instruction_launch` returned an unknown error code");
                    break ;
                }

            }
        }
    }

    return err;
}

int
Offloader::progress_pending_instructions(xkrt_stream_type_t stype, bool blocking)
{
    # pragma message(TODO "Better handling of error in case 'STREAM_ALL'")

    xkrt_runtime_t * ctx = xkrt_runtime_get();
    int err = 0;

    unsigned int bgn = (stype == XKRT_STREAM_TYPE_ALL) ?                      0 : stype;
    unsigned int end = (stype == XKRT_STREAM_TYPE_ALL) ? XKRT_STREAM_TYPE_ALL : stype + 1;
    for (unsigned int s = bgn ; s < end ; ++s)
    {
        for (unsigned int i = 0 ; i < this->count[s] ; ++i)
        {
            xkrt_stream_t * stream = this->streams[s][i];
            assert(stream);

            if (stream->pending.is_empty())
                continue ;

            xkrt_stream_instruction_counter_t n;
            do {
                stream->lock();
                err = stream->progress_pending_instructions(blocking);
                stream->unlock();
                n = stream->pending.size();
            } while (s == XKRT_STREAM_TYPE_KERN && n > ctx->conf.device.offloader.streams[XKRT_STREAM_TYPE_KERN].concurrency);
            assert(err == 0 || err == EINPROGRESS);
        }
    }

    return 0;
}

xkrt_stream_t *
Offloader::stream_next(xkrt_stream_type_t stype)
{
    /* find native stream to use */
    int count = this->count[stype];
    switch (count)
    {
        case (0):
            LOGGER_FATAL("No stream of type %d", stype);

        case (1):
            return this->streams[stype][0];

        default:
        {
            /* TODO : could be relaxed ? and maybe even non atomic, as it is
             * only call by the device thread */
            int snext = this->next[stype].fetch_add(1, std::memory_order_seq_cst) % count;

            LOGGER_DEBUG("instruction_new on stream type %d - count is %d - returning %d",
                    stype, count, snext);

            // TODO : track pending instr here ?

            return this->streams[stype][snext];
        }
    }
}

void
Offloader::instruction_new(
    const xkrt_stream_type_t stype,               /* IN  */
          xkrt_stream_t ** pstream,               /* OUT */
    const xkrt_stream_instruction_type_t itype,   /* IN  */
          xkrt_stream_instruction_t ** pinstr,    /* OUT */
    const xkrt_callback_t & callback       /* IN */
) {
    assert(pstream);
    assert(pinstr);

    /* retrieve native stream */
    xkrt_stream_t * stream = this->stream_next(stype);
    assert(stream->type == stype);

    /* allocate the instruction */
    xkrt_stream_instruction_t * instr;
try_instruction_new:

    do {
        stream->lock();
        instr = stream->instruction_new(itype, callback);
        if (instr)
            break ;
        stream->unlock();

        LOGGER_FATAL("Stream is full, increase 'XKRT_OFFLOADER_CAPACITY' or implement support for full-queue management in PTR yourself :-) (sorry)");

    } while (1);

    /* stream is locked, will be unlocked in the commit */

    /* out */
    assert(stream);
    assert(instr);
    *pstream = stream;
    *pinstr  = instr;
}
