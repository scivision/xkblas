# include "stream.hpp"

Stream::Stream() {}

Stream::~Stream() {}

bool
Stream::is_empty(xkblas_io_stream_type_t type) const
{
    return true;
}

int
Stream::process_instruction(xkblas_io_stream_type_t type)
{
    return 0;
}

int
Stream::test(xkblas_io_stream_type_t type)
{
    return 0;
}

int
Stream::wait(xkblas_io_stream_type_t type)
{
    return 0;
}
