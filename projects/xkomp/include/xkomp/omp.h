#ifndef __OMP_H__
# define __OMP_H__

extern "C"
{
    int omp_get_thread_num(void);
    int omp_get_num_threads(void);
};

 #endif /* __OMP_H__ */
