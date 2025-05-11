/* ************************************************************************** */
/*                                                                            */
/*   main.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/11 22:52:58 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

extern "C" {

    # include <stdio.h>
    # include <stdlib.h>
    # include <stdint.h>
    # include <string.h>
    # include <time.h>
};

// # include <xkblas/flops.h>

# define KAAPI_NO_DEFAULT_BLAS_ENUM 1

# include "common/blas.h"
# include "common/blas-version.h"
# include "common/impl.hpp"
# include "common/time.cc"
# include "common/utils.cc"

//////////////////////////////
//  TARGETED IMPLEMENTATION //
//////////////////////////////

# include "xkblas/impl.cc"
# include "common/check.cc"
static impl_t impl;
static bool SKIP_CHECK = 0;
static bool ALIGN_MATRICES = 0;

//  PARAMETERS TO TEST //
/////////////////////////

# define N_CBLAS_SIDE 2
static CBLAS_SIDE SIDE[N_CBLAS_SIDE]       = { CblasLeft,    CblasRight };
static const char * SIDE_STR[N_CBLAS_SIDE] = { "Left ", "Right" };

# define N_CBLAS_UPLO 2
static CBLAS_UPLO UPLO[N_CBLAS_UPLO]       = { CblasUpper,   CblasLower };
static const char * UPLO_STR[N_CBLAS_UPLO] = { "Upper", "Lower" };

# define N_CBLAS_DIAG 2
static CBLAS_DIAG DIAG[N_CBLAS_DIAG]       = { CblasNonUnit, CblasUnit  };
static const char * DIAG_STR[N_CBLAS_DIAG]  = { "NonUnit", "Unit   " };

# define N_CBLAS_TRANSPOSE 3
static CBLAS_TRANSPOSE TRANS[N_CBLAS_TRANSPOSE]  = { CblasNoTrans, CblasTrans, CblasConjTrans };
static const char * TRANS_STR[N_CBLAS_TRANSPOSE] = { "N", "T", "H" };

//////////////////////
//  CLI TO RUN-TIME //
//////////////////////

static void
prepare_n_matrices(uintptr_t * matrices, size_t n, size_t ld)
{
    /* allocate matrices */
    const size_t s = sizeof(TYPE);
    const uintptr_t alignon = s * ld;
    const uintptr_t memsize = n * (alignon + alignon/2 + s * ld * ld);
    const uintptr_t mem     = impl.alloc(memsize);

    for (int i = 0 ; i < n ; ++i)
    {
        uintptr_t M;

        /* force alignment on LD.s */
        if (ALIGN_MATRICES)
        {
            M = mem + (alignon - (mem % alignon)) + i * s * (ld * ld);
            assert(M % alignon == 0);
        }
        /* force unaligned (but align on sizeof(type) still) */
        else
        {
            M = mem + (alignon - (mem % alignon)) + i * s * (ld * ld) + alignon / 2;
            assert(M % alignon != 0);

            M = M + (s - (M % s));
            assert(M % s == 0);
        }

        matrices[i] = M;
        FILL((TYPE *)M, ld*ld);
        assert(M + ld*ld <= mem + memsize);
    }
}

// GEMM - GEMM //
static int
main_gemm_gemm(char ** args)
{
    /* parse arguments */
    int m   = atoi(args[0]);
    int n   = atoi(args[1]);
    int k   = atoi(args[2]);
    int ts1 = atoi(args[3]);
    int ts2 = atoi(args[4]);

    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    printf("Set (m, n, k) = (%d, %d, %d)\n", m, n, k);
    int ld = MAX(MAX(m, n), k);

    /* allocate matrices */
    uintptr_t matrices[5];
    # define A     ((TYPE *)matrices[0])
    # define B     ((TYPE *)matrices[1])
    # define C     ((TYPE *)matrices[2])
    # define CRef  ((TYPE *)matrices[3])
    # define CImpl ((TYPE *)matrices[4])
    prepare_n_matrices(matrices, 5, ld);
    FILL(&alpha, 1);
    FILL(&beta, 1);

    int t1 = 0;
    int t2 = 0;

    for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        for (int t2 = 0 ; t2 < N_CBLAS_TRANSPOSE ; ++t2)
        {
            impl.reset();
            memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

            /* run on impl */
            printf("Running implementation with (%s, %s)\n", TRANS_STR[t1], TRANS_STR[t2]);
            {
                uint64_t t0 = get_nanotime();
                impl.set_tile(ts1);
                impl.gemm(TRANS[t1], TRANS[t2], m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);

                impl.set_tile(ts2);
                impl.gemm(TRANS[t1], TRANS[t2], m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);

                impl.coherent(CImpl, m, n, ld);

                uint64_t tt = get_nanotime();
                impl.wait();
                uint64_t tf = get_nanotime();
                printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);
            }

            if (!SKIP_CHECK)
            {
                /* check correctness */
                memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
                int r = gemm_cmp(TRANS[t1], TRANS[t2], m, n, k, alpha, A, ld, B, ld, beta, C, CRef, CImpl, ld, 2);
                printf("Result is %12s with trans (%s, %s)\n", (r == 0) ? "CORRECT" : "INCORRECT", TRANS_STR[t1], TRANS_STR[t2]);
            }
        }
    }

    # undef A
    # undef B
    # undef C
    # undef CRef
    # undef CImpl

    return 0;
}

// GEMM //
static int
main_gemm(char ** args)
{
    /* parse arguments */
    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int k = atoi(args[2]);
    int ts = atoi(args[3]);
    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    int ld = MAX(MAX(m, n), k) * 2;

    printf("Set (m, n, k) = (%d, %d, %d)\n", m, n, k);

    FILL(&alpha, 1);
    FILL(&beta, 1);

    /* allocate matrices */
    uintptr_t matrices[5];
    # define A     ((TYPE *)matrices[0])
    # define B     ((TYPE *)matrices[1])
    # define C     ((TYPE *)matrices[2])
    # define CRef  ((TYPE *)matrices[3])
    # define CImpl ((TYPE *)matrices[4])
    prepare_n_matrices(matrices, 5, ld);

    srand(2023);

    // for small matrices
    if (m <= 64)
    {
        alpha = (TYPE) 1.0;
        beta  = (TYPE) 1.0;
        memset(A, 0, ld * ld * sizeof(TYPE));
        memset(B, 0, ld * ld * sizeof(TYPE));
        memset(C, 0, ld * ld * sizeof(TYPE));
        for (int i = 0 ; i < ld ; ++i)
        {
            B[i * ld + i] = 1.0;
            for (int j = 0 ; j < ld ; ++j)
            {
                A[i * ld + j] = (i * m + j) % 9;
                // A[i * ld + j] = (7 * i * j) % 13;
                // A[i * ld + j] = (i + j);
            }
        }
    }

    int t1 = 0;
    int t2 = 0;

    //for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        //for (int t2 = 0 ; t2 < N_CBLAS_TRANSPOSE ; ++t2)
        {
            memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

            printf("Running implementation with (%s, %s)\n", TRANS_STR[t1], TRANS_STR[t2]);
            uint64_t t0 = get_nanotime();
            impl.set_tile(ts);
            impl.gemm(TRANS[t1], TRANS[t2], m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);
            impl.coherent(CImpl, m, n, ld);
            uint64_t tt = get_nanotime();
            impl.wait();
            uint64_t tf = get_nanotime();

            printf("Implementation took %lf s. (graph construction took %lf s.)\n", // - %.2lf TFlop/s\n",
                    (tf-t0)/1e9, (tt-t0)/1e9 //, FLOPS_SGEMM(m, n, k) / ((tf-t0)/1e9) / 1e12
            );
            if (!SKIP_CHECK)
            {
                memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
                int r = gemm_cmp(TRANS[t1], TRANS[t2], m, n, k, alpha, A, ld, B, ld, beta, C, CRef, CImpl, ld, 1);
                printf("Result is %12s with trans (%s, %s)\n", (r == 0) ? "CORRECT" : "INCORRECT", TRANS_STR[t1], TRANS_STR[t2]);
            }
            impl.reset();
        }
    }

    # undef A
    # undef B
    # undef C
    # undef CRef
    # undef CImpl

    return 0;
}

static int
main_syrk(char ** args)
{
    /* parse arguments */
    CBLAS_UPLO      uplo  = CblasUpper;
    CBLAS_TRANSPOSE trans = CblasNoTrans;
    int n = atoi(args[0]);
    int k = atoi(args[1]);
    int ts = atoi(args[2]);
    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    int ld = MAX(n, k);
    printf("Set (n, k) = (%d, %d)\n", n, k);

    /* allocate matrices */
    uintptr_t matrices[4];
    # define A     ((TYPE *)matrices[0])
    # define C     ((TYPE *)matrices[1])
    # define CRef  ((TYPE *)matrices[2])
    # define CImpl ((TYPE *)matrices[3])
    prepare_n_matrices(matrices, 4, ld);
    FILL(&alpha, 1);
    FILL(&beta, 1);

    /* run on impl */
    int t1 = 0;
    for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        if ((TRANS[t1] != CblasNoTrans) && (TRANS[t1] != CblasTrans))
            continue ;

        memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
        memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

        printf("Running implementation with (%s)\n", TRANS_STR[t1]);
        uint64_t t0 = get_nanotime();
        impl.set_tile(ts);
        impl.syrk(uplo, TRANS[t1], n, k, &alpha, A, ld, &beta, CImpl, ld);
        impl.coherent(CImpl, n, n, ld);
        uint64_t tt = get_nanotime();
        impl.wait();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);

        if (!SKIP_CHECK)
        {
            /* check correctness */
            int r = syrk_cmp(uplo, trans, n, k, alpha, A, ld, beta, C, CRef, CImpl, ld, 1);
            printf("Result is %s\n", (r == 0) ? "CORRECT" : "INCORRECT");
        }
    }

    # undef A
    # undef C
    # undef CRef
    # undef CImpl

    return 0;
}

static int
main_trsm(char ** args)
{
    /* parse arguments */
    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int ts = atoi(args[2]);
    TYPE alpha = (const TYPE) 0.0;

    /* currently only support this */
    int ld = MAX(m, n);

    printf("Set (m, n) = (%d, %d)\n", m, n);

    /* allocate matrices */
    uintptr_t matrices[4];
    # define A     ((TYPE *)matrices[0])
    # define B     ((TYPE *)matrices[1])
    # define BRef  ((TYPE *)matrices[2])
    # define BImpl ((TYPE *)matrices[3])
    prepare_n_matrices(matrices, 4, ld);
    FILL(&alpha, 1);

    // diagonal dominante
    TYPE invmax = 1.0 / (TYPE) MAX(m,n);
    for (int i = 0; i < ld*ld ; ++i)
        A[i] *= invmax;
    for(int i = 0 ; i < ld ; ++i)
        A[ld*i+i] = 1.0;

    int s = 0;
    int u = 0;
    int t = 0;
    int d = 0;

    for (int s = 0 ; s < N_CBLAS_SIDE ; ++s)
    {
        for (int u = 0 ; u < N_CBLAS_UPLO ; ++u)
        {
            for (int t = 0 ; t < N_CBLAS_TRANSPOSE ; ++t)
            {
                for (int d = 0 ; d < N_CBLAS_DIAG ; ++d)
                {
                    impl.reset();
                    memcpy(BImpl, B, sizeof(TYPE) * (ld * ld));

                    /* run on impl */
                    printf("Running implementation with (%s, %s, %s, %s)\n",
                            SIDE_STR[s], UPLO_STR[u], TRANS_STR[t], DIAG_STR[d]);
                    uint64_t t0 = get_nanotime();
                    impl.set_tile(ts);
                    impl.trsm(SIDE[s], UPLO[u], TRANS[t], DIAG[d], m, n, &alpha, A, ld, BImpl, ld);
                    impl.coherent(BImpl, m, n, ld);
                    impl.wait();
                    uint64_t tf = get_nanotime();
                    printf("Implementation took %lf s.\n", (tf - t0) / (double)1e9);

                    if (!SKIP_CHECK)
                    {
                        memcpy(BRef,  B, sizeof(TYPE) * (ld * ld));
                        int r = trsm_cmp(SIDE[s], UPLO[u], TRANS[t], DIAG[d], m, n, alpha, A, ld, B, BRef, BImpl, ld);
                        printf("Result is %12s with params (%s, %s, %s, %s)\n", (r == 0) ? "CORRECT" : "INCORRECT", SIDE_STR[s], UPLO_STR[u], TRANS_STR[t], DIAG_STR[d]);
                    }
                }
            }
        }
    }

    # undef A
    # undef B
    # undef BRef
    # undef BImpl

    return 0;
}

static int
main_copyscale(char ** args)
{
    int m  = atoi(args[0]);
    int n  = atoi(args[1]);
    int ts = atoi(args[2]);

    /* currently only support this */
    int ld = MAX(m, n);
    printf("Set (m, n) = (%d, %d)\n", m, n);

    /* allocate matrices */
    uintptr_t matrices[3];
    # define D  ((TYPE *)matrices[0])
    # define L  ((TYPE *)matrices[1])
    # define U  ((TYPE *)matrices[2])
    prepare_n_matrices(matrices, 3, ld);

    /* run on impl */
    printf("Running implementation...\n");
    {
        uint64_t t0 = get_nanotime();
        bool should_copy = true;
        int * IW = NULL;
        impl.set_tile(ts);
        impl.copyscale(m, n, should_copy, IW, D, ld, L, ld, U, ld);
        impl.coherent(L, m, n, ld);
        impl.coherent(U, n, m, ld);
        impl.wait();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s.\n", (tf - t0) / (double)1e9);
    }

    puts("Correctness check not implemented");

    # undef D
    # undef L
    # undef U

    return 0;
}

# define SEED 0x123

/* return a random integer in [a, b] */
static inline int
rand_int(double a, double b)
{
    static bool initialized = false;
    if (!initialized)
    {
        srand(SEED);
        initialized = true;
    }
    double f = rand() / (double) RAND_MAX;
    return (int) (a + (b - a) * f);
}

static int
main_mumps(char ** args)
{

    /**
     *           n         m
     *      .-------.-----------.
     *  n   |   D   |    U      |
     *      |       |           |
     *      .-------.-----------.
     *      |       |           |
     *      |       |           |
     *   m  |  L    |     G     |
     *      |       |           |
     *      |       |           |
     *      .-------.-----------.
     */

    # define USE_WRITE_BACK     1
    # define USE_ARGS_MATRIX    0
    # define USE_TS_TUNER       1
    # define USE_PREALLOCATE    1
    # define NMATRICES          1

    TYPE alpha, beta;
    FILL(&alpha, 1);
    FILL(&beta, 1);

    bool should_copy = true;
    int * IW = NULL;
    uint64_t tt0;

    /* parse arguments */
    int  Ix = atoi(args[0]);

    // MATRIX SIZE TO TEST
    # if USE_ARGS_MATRIX
    int mn[][2] = {
        {atoi(args[1]), atoi(args[2])}
    };
    # else
    srand(2025);
    int mn[NMATRICES][2];
    for (int k = 0; k < sizeof(mn) / (2 * sizeof(int)); ++k)
    {
        mn[k][0] = 1024 + rand() % (16384 - 1024 + 1);
        mn[k][1] =  512 + rand() % ( 6144 -  512 + 1);
    }
    # endif /*USE_ARGS_MATRIX */

    constexpr int nmatrices = sizeof(mn) / (2 * sizeof(int));
    TYPE * D_matrices[nmatrices];
    TYPE * L_matrices[nmatrices];
    TYPE * U_matrices[nmatrices];
    TYPE * G_matrices[nmatrices];
    for (int k = 0 ; k < nmatrices ; ++k)
    {
        // do not account for first matrix in measurement
        int  m = mn[k][0];
        int  n = mn[k][1];

        int ld = m+n;

        printf("allocating and filling matrices %d/%d with (m, n) = (%d, %d)\n", k+1, nmatrices, m, n);

        /* allocate matrices */
        # if 1
        uintptr_t matrices[1];
        prepare_n_matrices(matrices, 1, ld);

        TYPE * D = (TYPE *) matrices[0];
        TYPE * L = (TYPE *) (D + n*ld + 0);
        TYPE * U = (TYPE *) (D + 0*ld + n);
        TYPE * G = (TYPE *) (D + n*ld + n);
        # else
        uintptr_t matrices[4];
        prepare_n_matrices(matrices, 4, ld);

        TYPE * D = (TYPE *) matrices[0];
        TYPE * L = (TYPE *) matrices[1];
        TYPE * U = (TYPE *) matrices[2];
        TYPE * G = (TYPE *) matrices[3];
        # endif

        D_matrices[k] = D;
        L_matrices[k] = L;
        U_matrices[k] = U;
        G_matrices[k] = G;

        printf("(init) Dumping some values of G : %lf %lf %lf %lf\n",
                G[0], G[m*m/2], G[m*m*3/4], G[3]);
    }

    // TILE SIZES TO TEST
    # if !USE_TS_TUNER
    int ts1 = atoi(args[3]);
    int ts2 = atoi(args[4]);
    int ts3 = atoi(args[5]);
    int ts[][3] = {
        {ts1, ts2, ts3}
    };
    # else
    int m = mn[0][0];
    int n = mn[0][1];

    int ts[][3] = {
        {512, 512, 512},
        {1024, 1024, 1024},
        {2048, 2048, 2048},
        {512, 1024, 2048},
        {1024, 2048, 4096},
        {2048, 4096, 8192},
        {m/1, m/2, m/4},
        {n/1, n/2, n/4},
        {m/8, m/16, 2048},
        {1024, m/2, n/4},
        {1024, m/4, 2048},
        {m/1, 1024, 4096},
        {n/2, m/2, 8192},
        {m/8, 512, n/8},
        {m/16, n/16, 1024},
        {4096, 2048, m/2},
        {n/4, m/4, 2048},
        {8192, n/8, m/8},
        {m/2, 512, 1024},
        {n/2, 4096, m/1},
        {m/1, n/2, 512},
        {1024, 1024, 1024},
        {m/2, m/4, m/8},
        {n/4, n/8, n/16},
        {2048, 4096, m/16},
        {8192, n/4, m/4},
        {n/1, 1024, m/2},
        {m/4, 1024, n/4},
        {n/8, 512, m/8},
        {512, 512, 512},
        {2048, 2048, 2048},
        {m/8, m/2, 1024},
        {n/2, n/1, 4096},
        {m/16, m/8, m/4},
        {n/16, n/8, 512},
        {m/2, 2048, 8192},
        {4096, m/1, 2048},
        {n/2, 1024, 1024},
        {n/4, 512, 512},
        {m/1, m/1, m/1},
        {n/1, n/1, n/1},
        {m/2, m/2, m/2},
        {n/2, n/2, n/2},
        {m/4, m/4, m/4},
        {n/4, n/4, n/4},
        {m/8, m/8, m/8},
        {n/8, n/8, n/8},
        {m/16, m/16, m/16},
        {n/16, n/16, n/16},
        {512, n/2, m/8},
        {8192, m/4, n/8},
        {1024, n/4, 2048},
        {4096, n/2, m/2},
        {m/1, 512, n/16},
        {n/1, 8192, m/16},
        {m/2, 2048, 512},
        {m/4, n/2, 1024},
        {512, m/8, 2048},
        {n/2, 4096, 512},
        {m/4, m/8, 4096},
        {8192, n/1, m/2},
        {1024, 4096, n/4},
        {m/8, 2048, n/4},
        {n/1, m/8, 8192},
        {m/16, n/2, 4096},
        {m/1, 8192, 1024},
        {n/4, 2048, m/4},
        {m/2, 1024, 512},
        {m/4, 512, n/2},
        {1024, m/16, n/8},
        {2048, 4096, 512},
        {8192, m/1, n/16},
        {n/2, 1024, 512},
        {m/4, 4096, 512},
        {m/16, 1024, n/4},
        {m/8, 8192, 2048},
        {1024, 512, n/2},
        {m/2, 4096, n/4},
        {n/1, 1024, m/8},
        {2048, n/2, m/16},
        {m/1, n/2, n/4},
        {m/2, 8192, 1024},
        {m/4, n/1, 4096},
        {n/4, 1024, 2048},
        {m/2, 2048, n/1}
    };
    # endif

    // for each matrices
    for (int k = 0 ; k < nmatrices ; ++k)
    {
        TYPE * D = D_matrices[k];
        TYPE * L = L_matrices[k];
        TYPE * U = U_matrices[k];
        TYPE * G = G_matrices[k];

        int  m = mn[k][0];
        int  n = mn[k][1];
        int ld = m+n;
        printf("running matrices with (m, n) = (%d, %d)\n", m, n);

        uint64_t tmin = UINT64_MAX;
        int imin = 0;
        const int ntiles = sizeof(ts) / (3 * sizeof(int));
        for (int i = 0 ; i < ntiles ; ++i)
        {
            for (int j = 0 ; j < Ix ; ++j)
            {
                # if REPLICATE
                impl.replicate(D, ld, m+n, m+n);
                impl.wait();
                # endif /* ONLY_COMPUTE */

                // if (ts[i][0] < 200 || ts[i][1] < 200 || ts[i][2] < 200)
                //     continue ;
                uint64_t t0 = get_nanotime();

                # if USE_PREALLOCATE
                impl.preallocate(D, n, n, ld);
                impl.preallocate(L, m, n, ld);
                impl.preallocate(U, n, m, ld);
                impl.preallocate(G, m, m, ld);
                # endif /* USE_PREALLOCATE */

                impl.set_tile(ts[i][0]);
                impl.trsm(CblasLeft, CblasUpper, CblasTrans, CblasUnit, n, m, &alpha, D, ld, L, ld);

                impl.set_tile(ts[i][1]);
                impl.copyscale(m, n, should_copy, IW, D, ld, L, ld, U, ld);
                # if USE_WRITE_BACK
             // impl.coherent(D, m, m, ld);
                impl.coherent(L, n, m, ld);
                impl.coherent(U, m, n, ld);
                # endif
                impl.set_tile(ts[i][2]);
                impl.gemmt(CblasUpper, CblasNoTrans, CblasNoTrans, m, n, &alpha, U, ld, L, ld, &beta, G, ld);

                # if USE_WRITE_BACK
                impl.coherent(G, m, m, ld);
                # endif

                uint64_t tt = get_nanotime();
                impl.wait();
                uint64_t tf = get_nanotime();
                printf("(%d/%u) Compute took %lf s. (graph construction took %lf s.) - with ts = {%d, %d, %d}\n", i, ntiles, (tf-t0)/1e9, (tt-t0)/1e9, ts[i][0], ts[i][1], ts[i][2]);

                impl.reset();

                if (tf - t0 < tmin)
                {
                    imin = i;
                    tmin = tf - t0;
                }

                // start accounting for time after 1st iteration
                if (k == 0 && i == 0 && j == 0)
                    tt0 = get_nanotime();
            }
        }

        printf("Best perf obtained with ts = {%d, %d, %d} for %lf s\n",
                ts[imin][0], ts[imin][1], ts[imin][2], (double)tmin/(double)1e9);
        printf("(done) Dumping some values of G : %lf %lf %lf %lf\n", G[0], G[m*m/2], G[m*m*3/4], G[3]);

    }
    uint64_t ttf = get_nanotime();
    printf("Total transfer+compute took %lf s. (excluding first matrix)\n", (ttf-tt0)/(double)1e9);

    return 0;
}

static int
main_trsm_copyscale_gemm(char ** args)
{
    /* parse arguments */
    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int k = atoi(args[2]);
    int ts = atoi(args[3]);
    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;
    int ld = MAX(MAX(m, n), k);

    /* allocate matrices */
    uintptr_t matrices[4];
    # define D  ((TYPE *)matrices[0])
    # define L  ((TYPE *)matrices[1])
    # define U  ((TYPE *)matrices[2])
    # define G  ((TYPE *)matrices[3])
    prepare_n_matrices(matrices, 4, ld);
    FILL(&alpha, 1);
    FILL(&beta, 1);

    int s = 0;
    int u = 0;
    int d = 0;
    int t = 0;
    bool should_copy = true;
    int * IW = NULL;

    // trsm en CblasLeft, CblasUpper, CBlasTrans, CBlasUnit et le gemm CBlasNoTrans, CBlasNoTrans
    int t1 = 0;
    int t2 = 0;

//    for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        // for (int t2 = 0 ; t2 < N_CBLAS_TRANSPOSE ; ++t2)
        {
            impl.reset();
            impl.set_tile(ts);

            printf("Running implementation with (%s, %s)\n", TRANS_STR[t1], TRANS_STR[t2]);
            uint64_t t0 = get_nanotime();
            assert(m == n);
            impl.trsm(SIDE[s], UPLO[u], TRANS[t], DIAG[d], m, k, &alpha, D, ld, L, ld);
            impl.copyscale(m, k, should_copy, IW, D, ld, L, ld, U, ld);
            impl.gemm(TRANS[t1], TRANS[t2], m, n, k, &alpha, L, ld, U, ld, &beta, G, ld);
            impl.coherent(D, k, k, ld);
            impl.coherent(L, m, k, ld);
            impl.coherent(U, k, n, ld);
            impl.coherent(G, m, n, ld);
            uint64_t tt = get_nanotime();
            impl.wait();
            uint64_t tf = get_nanotime();
            printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);
        }
    }

    # undef D
    # undef L
    # undef U
    # undef G

    return 0;
}


typedef struct  func_t
{
    const char * name;
    int (*f)(char **);
    int nargs;
    const char * descr;
    const char * usage;
}               func_t;

static func_t funcs[] = {
# if 0
    {
        .name = "TRSM-COPYSCALE-GEMM",
        .f = main_trsm_copyscale_gemm,
        .nargs = 5,
        .usage =    "ITER MODE M N S\n"
                    "  - ITER   : number of iterations\n"
                    "  - MODE   : '0' to re-use the same matrices, '1' to use new matrices on each iteration\n"
                    "  - M      : number of rows of matrices L and G, and number of cols of U and G\n"
                    "  - N      : number of rows of matrices D and U, cols of matrices D and L\n"
                    "  - S      : number of rows and cols per tile\n"
    },
# endif
    {
        .name = "COPYSCALE",
        .f = main_copyscale,
        .nargs = 3,
        .descr = "???",
        .usage =    "M N BS_M BS_N\n"
                    "  - M  : number of rows of matrices L, and number of cols of U\n"
                    "  - N  : number of rows of matrices D and U, cols of matrices D and L\n"
                    "  - TS : tile size\n"
    },
    {
        .name = "GEMM",
        .f = main_gemm,
        .nargs = 4,
        .descr = "C := A.B + C",
        .usage =    "M N K TS\n"
                    "  - M    : n° of rows of A and C\n"
                    "  - N    : n° of cols of B and C\n"
                    "  - K    : n° of cols of A, rows of B\n"
                    "  - BS_N : tile size\n"
    },

    {
        .name = "SYRK",
        .f = main_syrk,
        .nargs = 3,
        .descr = "C := A.A^T + C",
        .usage =    "N K TS\n"
                    "  - N  : n° of rows of A and C\n"
                    "  - K  : n° of cols of A\n"
                    "  - TS : tile size\n"
    },

    {
        .name = "TRSM",
        .f = main_trsm,
        .nargs = 3,
        .descr = "A.X = B",
        .usage =    "M N TS\n"
                    "  - M  : number of rows of matrices L\n"
                    "  - N  : number of rows of matrices D, cols of matrices D and L\n"
                    "  - TS : tile size\n"
    },

    {
        .name = "GEMM-GEMM",
        .f = main_gemm_gemm,
        .nargs = 5,
        .descr = "C := a.A.B + b.C - repeat twice with two block sizes",
        .usage =    "M N K TS1 TS2\n"
                    "  - M    : n° of rows of A and C\n"
                    "  - N    : n° of cols of B and C\n"
                    "  - K    : n° of cols of A, rows of B\n"
                    "  - TS1  : 1st gemm block size\n"
                    "  - TS1  : 2nd gemm block size\n"
    },


    {
        .name = "TRSM-COPYSCALE-GEMM",
        .f = main_trsm_copyscale_gemm,
        .nargs = 4,
        .descr = "???",
        .usage =    "M N K TS\n"
                    "  - M  : n° of rows of A and C\n"
                    "  - N  : n° of cols of B and C\n"
                    "  - K  : n° of cols of A, rows of B\n"
                    "  - TS : tile size\n"
    },

    {
        .name = "MUMPS",
        .f = main_mumps,
        .nargs = 6,
        .descr = "???",
        .usage =    "I M N K TS\n"
                    "  -   I : number of iterations\n"
                    "  -   M : ?\n"
                    "  -   N : ?\n"
                    "  - TS1 : tile size for trsm\n"
                    "  - TS2 : tile size for copyscale\n"
                    "  - TS3 : tile size for gemm\n",
    },
};
# define N_FUNCS (sizeof(funcs) / sizeof(func_t))

static int
error_usage(const char * label, func_t * func)
{
    if (func == NULL)
    {
        fprintf(stderr, "usage : %s [PRECISION] [FUNC] [...]\n", label);
        fprintf(stderr, "  - [FUNC] is one of");
        for (unsigned int i = 0 ; i < N_FUNCS ; ++i)
            fprintf(stderr, " %s", funcs[i].name);
        fprintf(stderr, "\n");
        fprintf(stderr, "  - [...] are [FUNC] parameters\n");
    }
    else
    {
        fprintf(stderr, "usage : %s %s ", label, func->name);
        fprintf(stderr, "%s", func->usage);
        fprintf(stderr, "to compute %s\n", func->descr);
    }
    return 1;
}

int
main(int argc, char ** argv)
{
    SKIP_CHECK = getenv("SKIP_CHECK") ? atoi(getenv("SKIP_CHECK")) : false;
    ALIGN_MATRICES = getenv("ALIGN_MATRICES") ? atoi(getenv("ALIGN_MATRICES")) : false;
    printf("Skipping checks (SKIP_CHECK) %s\n", SKIP_CHECK ? "enabled" : "disabled");
    printf("Align matrices (ALIGN_MATRICES) %s\n", ALIGN_MATRICES ? "enabled" : "disabled");

    if (argc >= 2)
    {
        int    nargs = argc - 1;
        char ** args = argv + 1;

        for (unsigned int i = 0 ; i < N_FUNCS ; ++i)
        {
            func_t * func = funcs + i;
            if (strcmp(func->name, args[0]) == 0)
            {
                --nargs;
                ++args;
                if (nargs != func->nargs)
                    return error_usage(argv[0], func);

                printf("Init '%s'\n", impl.name());
                impl.init();
                printf("Running '%s'\n", func->name);
                int r = func->f(args);
                impl.deinit();
                return r;
            }
        }
    }

    error_usage(argv[0], NULL);
    return 1;
}
