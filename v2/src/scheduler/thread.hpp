#ifndef __THREAD_HPP__
# define __THREAD_HPP__

class Thread
{
    public:
        Thread() {}
        ~Thread() {}

        void init(void);

    private:

};

extern thread_local Thread __tls;

#endif /* __THREAD_HPP__ */
