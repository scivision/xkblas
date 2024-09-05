#ifndef __BLAS_H__
# define __BLAS_H__

#if defined(USE_OPENBLAS)||defined(USE_CRAYBLAS)
#  include <cblas.h>
#elif defined(USE_MKL)
#  include <mkl.h>
#  include <mkl_types.h>
#  include <mkl_cblas.h>
#  include <mkl_lapacke.h>
#else
#  error "Blas library undefined"
#endif

#endif /* __BLAS_H__ */
