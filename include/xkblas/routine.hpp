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

# ifndef __XKBLAS_ROUTINE_DECL_HPP__
#  define __XKBLAS_ROUTINE_DECL_HPP__

# include <xkblas/routine.h>

// Precision type
template <xkblas_precision_t P>
struct _xkblas_precision_type_t;

template <> struct _xkblas_precision_type_t<S>  { using type = float;           };
template <> struct _xkblas_precision_type_t<C>  { using type = float _Complex;  };
template <> struct _xkblas_precision_type_t<D>  { using type = double;          };
template <> struct _xkblas_precision_type_t<Z>  { using type = double _Complex; };

template <xkblas_precision_t P>
using xkblas_precision_type_t = typename _xkblas_precision_type_t<P>::type;

// Real part precision type
template <xkblas_precision_t P>
struct _xkblas_precision_type_real_t;

template <> struct _xkblas_precision_type_real_t<S>  { using type = float;   };
template <> struct _xkblas_precision_type_real_t<C>  { using type = float;   };
template <> struct _xkblas_precision_type_real_t<D>  { using type = double;  };
template <> struct _xkblas_precision_type_real_t<Z>  { using type = double;  };

template <xkblas_precision_t P>
using xkblas_precision_type_real_t = typename _xkblas_precision_type_real_t<P>::type;

// Precision name
template <xkblas_precision_t P>
struct _xkblas_precision_name_t;

template <> struct _xkblas_precision_name_t<S>  { static constexpr const char* value = "S"; };
template <> struct _xkblas_precision_name_t<C>  { static constexpr const char* value = "C"; };
template <> struct _xkblas_precision_name_t<D>  { static constexpr const char* value = "D"; };
template <> struct _xkblas_precision_name_t<Z>  { static constexpr const char* value = "Z"; };

template <xkblas_precision_t P>
using xkblas_precision_name_t = typename _xkblas_precision_name_t<P>::value;

// Index type
template <xkblas_index_t T>
struct _xkblas_index_type_t;

template <> struct _xkblas_index_type_t<I32> { using type = int32_t; };
template <> struct _xkblas_index_type_t<I64> { using type = int64_t; };

template <xkblas_index_t T>
using xkblas_index_type_t = typename _xkblas_index_type_t<T>::type;

// Index type name
template <xkblas_index_t T>
struct _xkblas_index_name_t;

template <> struct _xkblas_index_name_t<I32> { static constexpr const char* value = "I32"; };
template <> struct _xkblas_index_name_t<I64> { static constexpr const char* value = "I64"; };

template <xkblas_index_t T>
using xkblas_index_name_t = typename _xkblas_index_name_t<T>::value;

// Optional: generate an array of string names
constexpr const char *
xkblas_routine_name(xkblas_routine_t k)
{
    switch (k)
    {
        #define CASE_NAME(name) case name: return #name;
        XKBLAS_FORALL_ROUTINES(CASE_NAME)
        #undef CASE_NAME
        default: return "UNKNOWN";
    }
}

# endif /* __XKBLAS_ROUTINE_DECL_HPP__ */
