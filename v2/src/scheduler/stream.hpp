#ifndef __STREAM_HPP__
# define __STREAM_HPP__

class Stream
{
    public:
        Stream() {}
        ~Stream() {}

    private:
        cudaStream_t cuStream;
};

#endif /* __STREAM_HPP__ */
