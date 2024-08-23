#ifndef __CUDA_HELPER_H__
# define __CUDA_HELPER_H__

# include <cublas_v2.h>

static inline cublasOperation_t
cblas2cublas_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:      return CUBLAS_OP_N;
        case CblasTrans:        return CUBLAS_OP_T;
        case CblasConjTrans:    return CUBLAS_OP_C;
    }
    XKBLAS_FATAL("Unknown trans code");
    return CUBLAS_OP_N;
}

#endif /* __CUDA_HELPER_H__ */
