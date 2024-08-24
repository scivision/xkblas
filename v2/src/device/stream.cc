# include "stream.h"

void
xkblas_stream_init(
    xkblas_stream_t * stream,
    xkblas_stream_type_t type
) {
    stream->type = type;
}
