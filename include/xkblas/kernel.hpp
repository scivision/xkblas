/**
 *  To add a new kernel
 *      - update the `XKBLAS_FORALL_KERNELS` macro
 *      - update the `XKBLAS_FORALL_PRECISIONS_AND_KERNELS` macro
 *      - add a `src/kernels/_.cc file implementing your kernel
 *      - update the CMakeLists.txt
 *
 * and maybe a few more steps, update this tutorial if you find something missing
 */

# ifndef __KERNEL_H__
#  define __KERNEL_H__

# include <stdint.h>

// Define the macro list of precisions
# define XKBLAS_FORALL_PRECISIONS(F) \
    F(S)                             \
    F(C)                             \
    F(D)                             \
    F(Z)

typedef enum    xkblas_precision_t
{
    # define DEFINE_ENUM(name) name,
    XKBLAS_FORALL_PRECISIONS(DEFINE_ENUM)
    # undef DEFINE_ENUM

    XKBLAS_PRECISION_MAX

}               xkblas_precision_t;

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

// Define the macro list of index type
# define XKBLAS_FORALL_INDEX(F) \
    F(I32)                      \
    F(I64)

typedef enum    xkblas_index_t
{
    # define DEFINE_ENUM(name) name,
    XKBLAS_FORALL_INDEX(DEFINE_ENUM)
    # undef DEFINE_ENUM

    XKBLAS_INDEX_MAX

}               xkblas_index_t;

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

/* for all precisions and index types */
# define XKBLAS_FORALL_PRECISIONS_AND_INDEX(F)  \
    F(S, I32)                                   \
    F(C, I32)                                   \
    F(D, I32)                                   \
    F(Z, I32)                                   \
    F(S, I64)                                   \
    F(C, I64)                                   \
    F(D, I64)                                   \
    F(Z, I64)

// Define the macro list of kernels
#define XKBLAS_FORALL_KERNELS(F) \
    /* LEVEL 1 */                \
    F(AXPBY)                     \
    F(AXPY)                      \
    F(DIVCOPY)                   \
    F(DOT)                       \
    F(FILL)                      \
    F(NRM2)                      \
    F(SCALCOPY)                  \
    F(SCALE)                     \
                                 \
    /* LEVEL 2 */                \
    F(COPYSCALE)                 \
    F(GEMV)                      \
                                 \
    /* LEVEL 3 */                \
    F(GEMM)                      \
    F(GEMMT)                     \
    F(HERK)                      \
    F(SYMM)                      \
    F(SYR2K)                     \
    F(SYRK)                      \
    F(TRSM)                      \
    F(TRMM)                      \
                                 \
    /* LAPACKE */                \
    F(POTRF)                     \
                                 \
    /* SPARSE */                 \
    F(SPMV)

// Now define the enum using the macro
typedef enum    xkblas_kernel_t
{
    # define DEFINE_ENUM(name) name,
    XKBLAS_FORALL_KERNELS(DEFINE_ENUM)
    # undef DEFINE_ENUM

   XKBLAS_KERNEL_MAX

}               xkblas_kernel_t;

// Optional: generate an array of string names
constexpr const char *
xkblas_kernel_name(xkblas_kernel_t k)
{
    switch (k)
    {
        #define CASE_NAME(name) case name: return #name;
        XKBLAS_FORALL_KERNELS(CASE_NAME)
        #undef CASE_NAME
        default: return "UNKNOWN";
    }
}

# define XKBLAS_FORALL_PRECISIONS_AND_KERNELS(F)    \
    /* LEVEL 1 */                                   \
    F(S,     DOT)                                   \
    F(D,     DOT)                                   \
    F(S,     NRM2)                                  \
    F(C,     NRM2)                                  \
    F(D,     NRM2)                                  \
    F(Z,     NRM2)                                  \
    F(S,     SCALE)                                 \
    F(C,     SCALE)                                 \
    F(D,     SCALE)                                 \
    F(Z,     SCALE)                                 \
    F(S,     AXPY)                                  \
    F(C,     AXPY)                                  \
    F(D,     AXPY)                                  \
    F(Z,     AXPY)                                  \
    F(S,     AXPBY)                                 \
    F(C,     AXPBY)                                 \
    F(D,     AXPBY)                                 \
    F(Z,     AXPBY)                                 \
                                                    \
    /* LEVEL 2 */                                   \
    F(S,     COPYSCALE)                             \
    F(C,     COPYSCALE)                             \
    F(D,     COPYSCALE)                             \
    F(Z,     COPYSCALE)                             \
    F(S,     GEMV)                                  \
    F(C,     GEMV)                                  \
    F(D,     GEMV)                                  \
    F(Z,     GEMV)                                  \
                                                    \
    /* LEVEL 3 */                                   \
    F(S,     GEMM)                                  \
    F(C,     GEMM)                                  \
    F(D,     GEMM)                                  \
    F(Z,     GEMM)                                  \
    F(S,     GEMMT)                                 \
    F(C,     GEMMT)                                 \
    F(D,     GEMMT)                                 \
    F(Z,     GEMMT)                                 \
    F(C,     HERK)                                  \
    F(Z,     HERK)                                  \
    F(S,     SYMM)                                  \
    F(C,     SYMM)                                  \
    F(D,     SYMM)                                  \
    F(Z,     SYMM)                                  \
    F(S,     SYR2K)                                 \
    F(C,     SYR2K)                                 \
    F(D,     SYR2K)                                 \
    F(Z,     SYR2K)                                 \
    F(S,     SYRK)                                  \
    F(C,     SYRK)                                  \
    F(D,     SYRK)                                  \
    F(Z,     SYRK)                                  \
    F(S,     TRSM)                                  \
    F(C,     TRSM)                                  \
    F(D,     TRSM)                                  \
    F(Z,     TRSM)                                  \
    F(S,     TRMM)                                  \
    F(C,     TRMM)                                  \
    F(D,     TRMM)                                  \
    F(Z,     TRMM)                                  \
                                                    \
   /* lapacke */                                    \
   F(C,      POTRF)                                 \
   F(Z,      POTRF)                                 \
                                                    \
   /* sparse */                                     \
   F(S,      SPMV)                                  \
   F(C,      SPMV)                                  \
   F(D,      SPMV)                                  \
   F(Z,      SPMV)

# define XKBLAS_FORALL_PRECISIONS_AND_KERNELS(F)    \
    /* LEVEL 1 */                                   \
    F(S,     DOT)                                   \
    F(C,     DOT)                                   \
    F(D,     DOT)                                   \
    F(Z,     DOT)                                   \
    F(S,     NRM2)                                  \
    F(C,     NRM2)                                  \
    F(D,     NRM2)                                  \
    F(Z,     NRM2)                                  \
    F(S,     SCALE)                                 \
    F(C,     SCALE)                                 \
    F(D,     SCALE)                                 \
    F(Z,     SCALE)                                 \
    F(S,     AXPY)                                  \
    F(C,     AXPY)                                  \
    F(D,     AXPY)                                  \
    F(Z,     AXPY)                                  \
    F(S,     AXPBY)                                 \
    F(C,     AXPBY)                                 \
    F(D,     AXPBY)                                 \
    F(Z,     AXPBY)                                 \
                                                    \
    /* LEVEL 2 */                                   \
    F(S,     COPYSCALE)                             \
    F(C,     COPYSCALE)                             \
    F(D,     COPYSCALE)                             \
    F(Z,     COPYSCALE)                             \
                                                    \
    /* LEVEL 3 */                                   \
    F(S,     GEMM)                                  \
    F(C,     GEMM)                                  \
    F(D,     GEMM)                                  \
    F(Z,     GEMM)                                  \
    F(S,     GEMMT)                                 \
    F(C,     GEMMT)                                 \
    F(D,     GEMMT)                                 \
    F(Z,     GEMMT)                                 \
    F(C,     HERK)                                  \
    F(Z,     HERK)                                  \
    F(S,     SYMM)                                  \
    F(C,     SYMM)                                  \
    F(D,     SYMM)                                  \
    F(Z,     SYMM)                                  \
    F(S,     SYR2K)                                 \
    F(C,     SYR2K)                                 \
    F(D,     SYR2K)                                 \
    F(Z,     SYR2K)                                 \
    F(S,     SYRK)                                  \
    F(C,     SYRK)                                  \
    F(D,     SYRK)                                  \
    F(Z,     SYRK)                                  \
    F(S,     TRSM)                                  \
    F(C,     TRSM)                                  \
    F(D,     TRSM)                                  \
    F(Z,     TRSM)                                  \
    F(S,     TRMM)                                  \
    F(C,     TRMM)                                  \
    F(D,     TRMM)                                  \
    F(Z,     TRMM)                                  \
                                                    \
   /* lapacke */                                    \
   F(C,      POTRF)                                 \
   F(Z,      POTRF)                                 \
                                                    \
   /* sparse */                                     \
   F(S,      SPMV)                                  \
   F(C,      SPMV)                                  \
   F(D,      SPMV)                                  \
   F(Z,      SPMV)


# endif /* __KERNEL_H__ */
