/* ************************************************************************** */
/*                                                                            */
/*   device.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:56:36 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/driver/device.hpp>

//////////////////////
// MEMORY MANAGMENT //
//////////////////////

void
xkrt_device_t::memory_reset(void)
{
    xkrt_area_t * area = &(this->area);

    # pragma message(TODO "This is leaking")
    xkrt_area_chunk_t * chunk0 = (xkrt_area_chunk_t *) malloc(sizeof(xkrt_area_chunk_t));
    assert(chunk0);
    memcpy(chunk0, &(area->chunk0), sizeof(xkrt_area_chunk_t));
    area->free_chunk_list = chunk0;

    XKRT_STATS_INCR(this->stats.memory.freed, this->stats.memory.allocated.currently);
    XKRT_STATS_SET (this->stats.memory.allocated.currently, 0);
}

void
xkrt_device_t::memory_set_chunk0(
    uintptr_t device_ptr,
    size_t size
) {
    xkrt_area_t * area = &(this->area);

    area->chunk0.device_ptr    = device_ptr;
    area->chunk0.size          = size;
    area->chunk0.state         = XKRT_ALLOC_CHUNK_STATE_FREE;
    area->chunk0.prev          = NULL;
    area->chunk0.next          = NULL;
    area->chunk0.freelink      = NULL;
    area->chunk0.use_counter   = 0;

    this->memory_reset();
}

void
xkrt_device_t::memory_deallocate(xkrt_area_chunk_t * chunk)
{
    bool delete_chunk = false;

    xkrt_area_t * area = &(this->area);
    XKRT_MUTEX_LOCK(area->lock);
    {
        chunk->state = XKRT_ALLOC_CHUNK_STATE_FREE;
        chunk->use_counter = 0;

        /* can we merge chunk into next_chunk ? */
        xkrt_area_chunk_t * next_chunk = chunk->next;
        if (next_chunk && next_chunk->state == XKRT_ALLOC_CHUNK_STATE_FREE)
        {
            next_chunk->prev = chunk->prev;
            if (chunk->prev)
                chunk->prev->next = next_chunk;
            next_chunk->size += chunk->size;
            assert(next_chunk->device_ptr > chunk->device_ptr);
            next_chunk->device_ptr = chunk->device_ptr;
            delete_chunk = true;
        }

        xkrt_area_chunk_t * prev_chunk = chunk->prev;
        if (prev_chunk)
        {
            /*  if prev_chunk is a free chunk and 'delete_chunk' is true,
             *  then we have to merge prev and next */
            if (prev_chunk->state == XKRT_ALLOC_CHUNK_STATE_FREE)
            {
                if (delete_chunk)
                {
                    assert(prev_chunk->device_ptr < chunk->device_ptr);
                    assert(prev_chunk->device_ptr < next_chunk->device_ptr);

                    prev_chunk->size += next_chunk->size;
                    prev_chunk->next = next_chunk->next;
                    if (next_chunk->next)
                        next_chunk->next->prev = prev_chunk;
                    prev_chunk->freelink = next_chunk->freelink;
                    free(next_chunk);
                }
                else
                {
                    /* merge chunk into prev_chunk */
                    assert(prev_chunk->device_ptr < chunk->device_ptr);
                    prev_chunk->next = chunk->next;
                    if (chunk->next)
                        chunk->next->prev = prev_chunk;
                    prev_chunk->size += chunk->size;
                    delete_chunk = true;
                }
            }
            else if (!delete_chunk)
            {
                /* free_chunk_list is ordered by increasing adress: search form prev the previous bloc */
                while (prev_chunk && prev_chunk->state != XKRT_ALLOC_CHUNK_STATE_FREE)
                    prev_chunk = prev_chunk->prev;

                if (!prev_chunk)
                {
                    chunk->freelink = area->free_chunk_list;
                    area->free_chunk_list = chunk;
                }
                else
                {
                    chunk->freelink = prev_chunk->freelink;
                    prev_chunk->freelink = chunk;
                }
            }
        }
        else if (!delete_chunk)
        {
            chunk->freelink = area->free_chunk_list;
            area->free_chunk_list = chunk;
        }
    }
    XKRT_MUTEX_UNLOCK(area->lock);

    XKRT_STATS_INCR(this->stats.memory.freed, chunk->size);
    XKRT_STATS_DECR(this->stats.memory.allocated.currently, chunk->size);

    if (delete_chunk)
        free(chunk);
}

xkrt_area_chunk_t *
xkrt_device_t::memory_allocate(const size_t user_size)
{
    /* align data */
    const size_t size = (user_size + 7UL) & ~7UL;

    xkrt_area_chunk_t * curr;

    xkrt_area_t * area = &(this->area);
    XKRT_MUTEX_LOCK(area->lock);
    {
        /* best fit strategy */
        curr = area->free_chunk_list;

        xkrt_area_chunk_t * prevfree = NULL;
        size_t min_size = 0;
        xkrt_area_chunk_t * min_size_curr = NULL;
        xkrt_area_chunk_t * min_size_prevfree = NULL;

        while (curr)
        {
            size_t curr_size = curr->size;
            if (curr_size >= size)
            {
                if ((min_size_curr == 0) || (min_size > curr_size))
                {
                    min_size = curr_size;
                    min_size_curr = curr;
                    min_size_prevfree = prevfree;
                }
            }
            prevfree = curr;
            curr = curr->freelink;
        }

        /* and the winner is min_size_curr ! */
        curr = min_size_curr;
        prevfree = min_size_prevfree;

        /* split chunk */
        if ((curr != NULL) && (min_size - size >= (size_t)(0.5*(double)size)))
        {
            size_t curr_size = curr->size;
            xkrt_area_chunk_t * remainder = (xkrt_area_chunk_t *) malloc(sizeof(xkrt_area_chunk_t));
            remainder->device_ptr   = size + curr->device_ptr;
            remainder->size         = (curr_size - size);
            remainder->state        = XKRT_ALLOC_CHUNK_STATE_FREE;
            remainder->use_counter  = 0;
            remainder->prev         = curr;
            remainder->next         = curr->next;
            remainder->freelink     = curr->freelink;

            /* link remainder segment after curr */
            if (curr->next)
                curr->next->prev = remainder;
            curr->next = remainder;
            curr->size = size;
            curr->freelink = remainder;
        }

        if (curr != NULL)
        {
            if (prevfree)
                prevfree->freelink = curr->freelink;
            else
                area->free_chunk_list = curr->freelink;
            curr->state = XKRT_ALLOC_CHUNK_STATE_ALLOCATED;
            curr->freelink = NULL;
        }
    }

    XKRT_MUTEX_UNLOCK(area->lock);

    if (curr)
    {
        XKRT_STATS_INCR(this->stats.memory.allocated.total,       size);
        XKRT_STATS_INCR(this->stats.memory.allocated.currently,   size);
    }

    return curr;
}


///////////////////////
// STREAM MANAGEMENT //
///////////////////////

void
xkrt_device_t::offloader_init(
    xkrt_stream_t * (*f_stream_create)(xkrt_device_t * device, xkrt_stream_type_t type, xkrt_stream_instruction_counter_t capacity)
) {
    assert(f_stream_create);

    /* next stream to use (round robin) */
    memset(this->next, 0, sizeof(this->next));

    /* count total number of stream */
    uint16_t cnt = 0;
    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        this->count[stype] = this->conf->offloader.streams[stype].n;
        cnt += this->conf->offloader.streams[stype].n;
    }

    /* allocate streams array */
    xkrt_stream_t ** all_streams = (xkrt_stream_t **) malloc(sizeof(xkrt_stream_t *) * cnt);
    assert(all_streams);

    /* retrieve stream offset per type */
    uint16_t i = 0;
    for (int stype = 0 ; stype < XKRT_STREAM_TYPE_ALL ; ++stype)
    {
        this->streams[stype] = all_streams + i;
        for (int j = 0 ; j < this->conf->offloader.streams[stype].n ; ++j, ++i)
        {
            all_streams[i] = f_stream_create(this, static_cast<xkrt_stream_type_t>(stype), this->conf->offloader.capacity);
            assert(all_streams[i]);
        }
    }

    assert(i == cnt);
}

bool
xkrt_device_t::offloader_streams_are_empty(const xkrt_stream_type_t stype) const
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
xkrt_device_t::offloader_stream_instructions_launch(const xkrt_stream_type_t stype)
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

template <bool blocking>
int
xkrt_device_t::offloader_stream_instructions_progress(
    const xkrt_stream_type_t stype
) {
    # pragma message(TODO "Better handling of error in case 'STREAM_ALL'")

    int err = 0;

    unsigned int bgn = (stype == XKRT_STREAM_TYPE_ALL) ?                    0 : stype;
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
                if (blocking)
                {
                    stream->wait_pending_instructions();
                    err= 0;
                }
                else
                    err = stream->progress_pending_instructions();
                stream->unlock();
                n = stream->pending.size();
            } while (n > this->conf->offloader.streams[s].concurrency);
            assert(err == 0 || err == EINPROGRESS);
        }
    }

    return 0;
}

xkrt_stream_t *
xkrt_device_t::offloader_stream_next(xkrt_stream_type_t stype)
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
            int snext = this->next[stype].fetch_add(1, std::memory_order_relaxed) % count;
            return this->streams[stype][snext];
        }
    }
}

int
xkrt_device_t::offloader_poll(void)
{
    int err = 0;
    assert(ThreadWorker::self() == this->thread);

    err = this->offloader_stream_instructions_launch(XKRT_STREAM_TYPE_ALL);
    assert((err == 0) || (err == EINPROGRESS));

    err = this->offloader_stream_instructions_progress<false>(XKRT_STREAM_TYPE_ALL);
    assert((err == 0) || (err == EINPROGRESS));

    return err;
}

////////////////////////////
// INSTRUCTION SUBMISSION //
////////////////////////////

/* commit a stream instruction and wakeup thread */
inline void
xkrt_device_t::offloader_stream_instruction_commit(
    xkrt_stream_t * stream,
    xkrt_stream_instruction_t * instr
) {
    /* commit instruction to the stream */
    stream->commit(instr);

    /* wakeup device worker thread */
    this->thread->wakeup();

    /* unlock the stream */
    stream->unlock();
}

void
xkrt_device_t::offloader_stream_instruction_new(
    const xkrt_stream_type_t stype,             /* IN  */
          xkrt_stream_t ** pstream,             /* OUT */
    const xkrt_stream_instruction_type_t itype, /* IN  */
          xkrt_stream_instruction_t ** pinstr,  /* OUT */
    const xkrt_callback_t & callback            /* IN */
) {
    assert(pstream);
    assert(pinstr);

    /* retrieve native stream */
    xkrt_stream_t * stream = this->offloader_stream_next(stype);
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

        LOGGER_FATAL("Stream is full, increase 'XKRT_OFFLOADER_CAPACITY' or implement support for full-queue management yourself :-) (sorry)");

    } while (1);

    /* stream is locked, will be unlocked in the commit */

    /* out */
    assert(stream);
    assert(instr);
    *pstream = stream;
    *pinstr  = instr;
}

void
xkrt_device_t::offloader_stream_instruction_submit_kernel(
    void (*launch)(void * handle, void * vargs),
    void * vargs,
    const xkrt_callback_t & callback
) {
    /* create a new instruction and retrieve its offload stream */
    xkrt_stream_t * stream;
    xkrt_stream_instruction_t * instr;
    this->offloader_stream_instruction_new(
        XKRT_STREAM_TYPE_KERN,          /* IN */
        &stream,                        /* OUT */
        XKRT_STREAM_INSTR_TYPE_KERN,    /* IN */
        &instr,                         /* OUT */
        callback
    );
    assert(stream);
    assert(instr);
    assert(stream->is_locked());

    /* create a new kernel instruction */
    instr->kern.launch = launch;
    instr->kern.vargs = vargs;

    this->offloader_stream_instruction_commit(stream, instr);
}

# pragma message(TODO "using a full 'host_view' here is overkill, only needing (sizeof_type, n, m) i believe")
void
xkrt_device_t::offloader_stream_instruction_submit_copy(
    const memory_view_t           & host_view,
    const xkrt_device_global_id_t   dst_device_global_id,
    const memory_replicate_view_t & dst_device_view,
    const xkrt_device_global_id_t   src_device_global_id,
    const memory_replicate_view_t & src_device_view,
    const xkrt_callback_t         & callback
) {
    // assert(ThreadWorker::self() == device->thread);
    assert(this->global_id == dst_device_global_id || this->global_id == src_device_global_id);

    assert(dst_device_view.addr);
    assert(dst_device_view.ld);

    assert(src_device_view.addr);
    assert(src_device_view.ld);

    /* find the type of copy instruction */
    xkrt_stream_instruction_type_t itype;
    if (src_device_global_id == HOST_DEVICE_GLOBAL_ID)
    {
        if (dst_device_global_id == HOST_DEVICE_GLOBAL_ID)
            itype = XKRT_STREAM_INSTR_TYPE_COPY_H2H;
        else
            itype = XKRT_STREAM_INSTR_TYPE_COPY_H2D;
    }
    else
    {
        if (dst_device_global_id == HOST_DEVICE_GLOBAL_ID)
            itype = XKRT_STREAM_INSTR_TYPE_COPY_D2H;
        else
            itype = XKRT_STREAM_INSTR_TYPE_COPY_D2D;
    }

    /* find the type of stream to use */
    xkrt_stream_type_t stype;
    switch(itype)
    {
        # pragma message(TODO "No H2H streams, do we want one ? Currently using H2D stream for H2H copies, mimicing original ptr")
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2H):
        case (XKRT_STREAM_INSTR_TYPE_COPY_H2D):
        {
            assert(this->global_id == dst_device_global_id);
            stype = XKRT_STREAM_TYPE_H2D;
            break ;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_D2H):
        {
            assert(this->global_id == src_device_global_id);
            stype = XKRT_STREAM_TYPE_D2H;
            break ;
        }

        case (XKRT_STREAM_INSTR_TYPE_COPY_D2D):
        {
            stype = XKRT_STREAM_TYPE_D2D;
            break ;
        }

        default:
        {
            LOGGER_FATAL("Impossible occured");
            break ;
        }
    }

    /* create a new instruction and retrieve its offload stream */
    xkrt_stream_t * stream;
    xkrt_stream_instruction_t * instr;
    this->offloader_stream_instruction_new(
        stype,      /* IN */
        &stream,    /* OUT */
        itype,      /* IN */
        &instr,     /* OUT */
        callback
    );
    assert(stream);
    assert(instr);

    /* create a new copy instruction */
    instr->copy.host_view       = host_view;
    instr->copy.dst_device_view = dst_device_view;
    instr->copy.src_device_view = src_device_view;

    this->offloader_stream_instruction_commit(stream, instr);

    XKRT_STATS_INCR(stream->stats.transfered, host_view.size());
}
