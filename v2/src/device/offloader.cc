# include "stream.hpp"

Offloader::Offloader() {}

Offloader::~Offloader() {}

void
lol(
) {
}

void
Offloader::init(xkblas_conf_stream_t * conf)
{
    int cnt = 0;
    for (xkblas_stream_type_t stype = 0 ; stype < XKBLAS_STREAM_ALL ; ++stype)
    {
        xkblas_conf_iostream_t * sconf = conf->streams.iostream + stype;
        this->next[stype] = 0;
        cnt += sconf->n;
    }
    this->ios = (xkblas_stream_t **) malloc(sizeof(xkblas_stream_t *) * cnt);
    assert(this->ios);



     # if 0
     unsigned int cnt = 0;
     unsigned int prefix[XKBLAS_STREAM_ALL+1];
 
     prefix[XKBLAS_STREAM_H2D] = 0;
     cnt += (stream->count[XKBLAS_STREAM_H2D]  = XKBLAS_CONF.cuda_conc_h2d);
     prefix[XKBLAS_STREAM_D2H] = cnt;
     cnt += (stream->count[XKBLAS_STREAM_D2H]  = XKBLAS_CONF.cuda_conc_d2h);
     prefix[XKBLAS_STREAM_D2D] = cnt;
     cnt += (stream->count[XKBLAS_STREAM_D2D]  = XKBLAS_CONF.cuda_conc_d2d);
     prefix[XKBLAS_STREAM_KERN] = cnt;
     cnt += (stream->count[XKBLAS_STREAM_KERN] = XKBLAS_CONF.cuda_conc_stream_kernel);
     prefix[XKBLAS_STREAM_KERN+1] = cnt;
 
     stream->next[XKBLAS_STREAM_D2H]  = 0;
     stream->next[XKBLAS_STREAM_H2D]  = 0;
     stream->next[XKBLAS_STREAM_D2D]  = 0;
     stream->next[XKBLAS_STREAM_KERN] = 0;
 
     xkblas_stream_t** ios;
     stream->ios = ios = (xkblas_stream_t **) malloc(sizeof(xkblas_stream_t*) * cnt );
     assert( stream->ios[0]!= 0 );
     stream->ios[XKBLAS_STREAM_H2D]  = stream->ios[0]+prefix[XKBLAS_STREAM_H2D];
     stream->ios[XKBLAS_STREAM_D2H]  = stream->ios[0]+prefix[XKBLAS_STREAM_D2H];
     stream->ios[XKBLAS_STREAM_D2D]  = stream->ios[0]+prefix[XKBLAS_STREAM_D2D];
     stream->ios[XKBLAS_STREAM_KERN] = stream->ios[0]+prefix[XKBLAS_STREAM_KERN];
 
     for (unsigned int i = 0; i < cnt; ++i)
     {
         xkblas_stream_type_t type =
             i < prefix[XKBLAS_STREAM_D2H] ? XKBLAS_STREAM_H2D :
             i < prefix[XKBLAS_STREAM_D2D] ? XKBLAS_STREAM_D2H :
             i < prefix[XKBLAS_STREAM_KERN] ? XKBLAS_STREAM_D2D : XKBLAS_STREAM_KERN
             ;
         ios[i]  = stream->f_stream_alloc( device, type, capacity );
         ios[i]->sid = i;
         assert( ios[i] != 0 );
         ios[i]->stream = s;
         //printf("%i:: init stream %i type: %s\n", device->ld->ldid, i, 
         //    type == XKBLAS_STREAM_H2D ? "H2D" : type == XKBLAS_STREAM_KERN ? "kern": type == XKBLAS_STREAM_D2H ? "D2H" : type == XKBLAS_STREAM_D2D ? "D2D" : "<NOTY    PE>" );
         assert( 0 == _xkblas_offload_iostream_init( stream->ios[i], type, capacity )); 
     }
     # endif
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
Offloader::instruction_new(xkblas_instruction_type_t type)
{
    xkblas_stream_instruction_t * instr = ???;
    instr->type = type;
    return instr;
}
