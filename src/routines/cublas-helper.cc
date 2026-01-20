/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

# include <xkrt/consts.h>
# include <xkblas/cublas-helper.h>

XKRT_NAMESPACE_USE;

/** Device pointers to constants */
//    XKBLAS_CUBLAS_CONST_ZERO    = 0,
//    XKBLAS_CUBLAS_CONST_HALF    = 1,
//    XKBLAS_CUBLAS_CONST_ONE     = 2,
//    XKBLAS_CUBLAS_CONST_TWO     = 3

# define F(VALUE, NAME) (float) VALUE,
float XKBLAS_CUBLAS_HOST_CONST_S[XKBLAS_CUBLAS_CONST_MAX] = { XKBLAS_CUBLAS_FOREACH_CONST(F) };
# undef F

# define F(VALUE, NAME) (double) VALUE,
double XKBLAS_CUBLAS_HOST_CONST_D[XKBLAS_CUBLAS_CONST_MAX] = { XKBLAS_CUBLAS_FOREACH_CONST(F) };
# undef F

# define F(VALUE, NAME) (cuComplex) VALUE,
cuComplex XKBLAS_CUBLAS_HOST_CONST_C[XKBLAS_CUBLAS_CONST_MAX] = { XKBLAS_CUBLAS_FOREACH_CONST(F) };
# undef F

# define F(VALUE, NAME) (cuDoubleComplex) VALUE,
cuDoubleComplex XKBLAS_CUBLAS_HOST_CONST_Z[XKBLAS_CUBLAS_CONST_MAX] = { XKBLAS_CUBLAS_FOREACH_CONST(F) };
# undef F

float            * XKBLAS_CUBLAS_DEVICE_CONST_S[XKRT_DEVICES_MAX];
double           * XKBLAS_CUBLAS_DEVICE_CONST_D[XKRT_DEVICES_MAX];
cuComplex        * XKBLAS_CUBLAS_DEVICE_CONST_C[XKRT_DEVICES_MAX];
cuDoubleComplex  * XKBLAS_CUBLAS_DEVICE_CONST_Z[XKRT_DEVICES_MAX];
