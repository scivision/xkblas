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

// Define the macro list of precisions
#define XKBLAS_FORALL_PRECISIONS(F) \
    F(S)                            \
    F(C)                            \
    F(D)                            \
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

// Precision name
template <xkblas_precision_t P>
struct _xkblas_precision_name_t;

template <> struct _xkblas_precision_name_t<S>  { static constexpr const char* value = "S"; };
template <> struct _xkblas_precision_name_t<C>  { static constexpr const char* value = "C"; };
template <> struct _xkblas_precision_name_t<D>  { static constexpr const char* value = "D"; };
template <> struct _xkblas_precision_name_t<Z>  { static constexpr const char* value = "Z"; };

template <xkblas_precision_t P>
using xkblas_precision_name_t = typename _xkblas_precision_name_t<P>::value;

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
                                 \
    /* LEVEL 3 */                \
    F(GEMM)                      \
    F(GEMMT)                     \
    F(SYMM)                      \
    F(SYR2K)                     \
    F(SYRK)                      \
    F(TRSM)                      \
    F(TRMM)

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
    F(Z,     TRMM)


# endif /* __KERNEL_H__ */
