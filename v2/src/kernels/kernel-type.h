#ifndef __KERNEL_TYPE_H__
# define __KERNEL_TYPE_H__

typedef enum    xkblas_kernel_type_t
{
    XKBLAS_KERNEL_TYPE_GEMM,
    XKBLAS_KERNEL_TYPE_TRSM,
    XKBLAS_KERNEL_TYPE_MAX,

}               xkblas_kernel_type_t;

#endif /* __KERNEL_TYPE_H__ */
