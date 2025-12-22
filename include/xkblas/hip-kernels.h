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

#ifndef __HIP_KERNELS_H__
# define __HIP_KERNELS_H__

# include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int hip_scopyscale(hipStream_t hip_queue, int m, int n, int should_copy, int* IW, const float            * D, int ldd, float            * L, int ldl, float            * U, int ldu);
int hip_dcopyscale(hipStream_t hip_queue, int m, int n, int should_copy, int* IW, const double           * D, int ldd, double           * L, int ldl, double           * U, int ldu);
int hip_ccopyscale(hipStream_t hip_queue, int m, int n, int should_copy, int* IW, const hipFloatComplex  * D, int ldd, hipFloatComplex  * L, int ldl, hipFloatComplex  * U, int ldu);
int hip_zcopyscale(hipStream_t hip_queue, int m, int n, int should_copy, int* IW, const hipDoubleComplex * D, int ldd, hipDoubleComplex * L, int ldl, hipDoubleComplex * U, int ldu);

int hip_sfill(hipStream_t hip_queue, int n, float  * x,           const float            value);
int hip_dfill(hipStream_t hip_queue, int n, double * x,           const double           value);
int hip_cfill(hipStream_t hip_queue, int n, hipFloatComplex * x,  const hipFloatComplex  value);
int hip_zfill(hipStream_t hip_queue, int n, hipDoubleComplex * x, const hipDoubleComplex value);

int hip_offset_vector_i32(hipStream_t hip_queue, int n, int32_t * x, const int32_t v);
int hip_offset_vector_i64(hipStream_t hip_queue, int n, int64_t * x, const int64_t v);

#ifdef __cplusplus
}
#endif

#endif /* __HIP_KERNELS_H__ */
